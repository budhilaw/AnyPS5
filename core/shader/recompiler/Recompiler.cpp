#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <future>
#include "CacheKey.hpp"
#include <iterator>
#include <limits>
#include <mutex>
#include <string_view>
#include <shared_mutex>
#include <unordered_map>
#include "ControlFlow/include/ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/include/ControlFlow/Structurizer.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "IntermediateRepresentation/include/IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/include/Optimization/BindingAllocator.hpp"
#include "Optimization/include/Optimization/ConstantFolder.hpp"
#include "Optimization/include/Optimization/DeadCodeEliminator.hpp"
#include "Optimization/include/Optimization/DescriptorBindingBuilder.hpp"
#include "Optimization/include/Optimization/IndirectBufferExpander.hpp"
#include "Optimization/include/Optimization/ReadLaneEliminator.hpp"
#include "Optimization/include/Optimization/RequestMemoryView.hpp"
#include "Optimization/include/Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/include/Optimization/ResourceTracker.hpp"
#include "Optimization/include/Optimization/ShaderInfoCollector.hpp"
#include "Optimization/include/Optimization/SrtWalker.hpp"
#include "Optimization/include/Optimization/SsaBuilder.hpp"
#include "SpirvBackend/include/SpirvBackend/SpirvEmitter.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "SpirvBackend/SpirvMemory/SpirvInputOutput.hpp"
#include "Translation/include/Translation/InstructionTranslator.hpp"
#include "Translation/include/Translation/ShaderInputInfoBuilder.hpp"
#include <exception>
#include <stdexcept>
#include <string>
#include <ControlFlow/RequestSerializer.hpp>

namespace ShaderRecompiler {

namespace {

ShaderStageKind toShaderStageKind(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Compute:
        return ShaderStageKind::Compute;
    case ShaderStage::Vertex:
        return ShaderStageKind::Vertex;
    case ShaderStage::TessellationControl:
        return ShaderStageKind::TessellationControl;
    case ShaderStage::TessellationEvaluation:
        return ShaderStageKind::TessellationEvaluation;
    case ShaderStage::Fragment:
        return ShaderStageKind::Pixel;
    case ShaderStage::Local:
        return ShaderStageKind::Local;
    case ShaderStage::Mesh:
        return ShaderStageKind::Mesh;
    case ShaderStage::Geometry:
        break;
    }
    throw std::runtime_error("ShaderRecompiler::Recompile: unsupported shader stage");
}

}

IrProgram PrepareResourceProgram(const RecompileRequest& request, bool expandIndirectBuffers) {
    const auto stageKind = toShaderStageKind(request.shader.stage);
    const auto inputInfo = BuildShaderStageInputInfo(stageKind, request.context, request.target.subgroupSize);

    constexpr RdnaInstructionDecoder decoder;
    const auto decoded = decoder.Decode(request.shader.code);

    constexpr GraphBuilder graphBuilder;
    auto cfg = graphBuilder.Build(decoded);

    constexpr Structurizer structurizer;
    structurizer.Structurize(cfg);

    TranslateOptions translateOptions {};
    translateOptions.stage = stageKind;
    translateOptions.shaderHash = request.shader.codeHash;
    translateOptions.waveSize = request.context.waveSize;
    translateOptions.userDataBaseRegister = request.context.userDataBaseRegister;
    translateOptions.userDataCount = static_cast<std::uint32_t>(request.context.userData.size());
    translateOptions.embeddedFetch = nullptr;
    translateOptions.fragmentShaderBarycentricEnabled = request.target.fragmentShaderBarycentricEnabled;
    translateOptions.inputInfo = inputInfo;

    constexpr InstructionTranslator translator;

    EmbeddedFetchPlan embeddedFetch;
    if ((stageKind == ShaderStageKind::Vertex || stageKind == ShaderStageKind::Local) && inputInfo.vertex != nullptr && inputInfo.vertex->fetchEmbedded) {
        constexpr EmbeddedVertexFetchAnalyzer embeddedFetchAnalyzer;
        embeddedFetch = embeddedFetchAnalyzer.Analyze(decoded, inputInfo.vertex->fetchAttribReg, inputInfo.vertex->fetchBufferReg, request.context.userDataBaseRegister, static_cast<std::uint32_t>(request.context.userData.size()), request.context.waveSize);
    }
    translateOptions.embeddedFetch = embeddedFetch.loads.empty() ? nullptr : &embeddedFetch;

    auto program = translator.Translate(decoded, cfg, translateOptions);

    constexpr SsaBuilder ssaBuilder;
    ssaBuilder.Rewrite(program);

    constexpr ConstantFolder constantFolder;
    constexpr DeadCodeEliminator deadCodeEliminator;

    constantFolder.Fold(program);
    ResolveControlFlowIdentities(program);
    deadCodeEliminator.RemoveIdentities(program);
    deadCodeEliminator.Eliminate(program);

    constexpr ReadLaneEliminator readLaneEliminator;
    const auto readLaneStats = readLaneEliminator.Eliminate(program, translateOptions.waveSize);
    if (readLaneStats.rewrittenReads != 0u) {
        constantFolder.Fold(program);
        ResolveControlFlowIdentities(program);
        deadCodeEliminator.RemoveIdentities(program);
        deadCodeEliminator.Eliminate(program);
    }

    constexpr IndirectBufferExpander indirectBufferExpander;
    if (expandIndirectBuffers && indirectBufferExpander.Expand(program) != 0u) deadCodeEliminator.Eliminate(program);

    constexpr SrtWalker srtWalker;
    srtWalker.BuildPlan(program);
    deadCodeEliminator.Eliminate(program);

    constexpr ResourceTracker resourceTracker;
    resourceTracker.Track(program);
    deadCodeEliminator.Eliminate(program);

    return program;
}

IrProgram PrepareResourceProgram(const RecompileRequest& request) {
    try {
        return PrepareResourceProgram(request, false);
    } catch (const std::runtime_error& error) {
        if (std::string_view(error.what()).find("is not a valid runtime value") == std::string_view::npos) throw;
        return PrepareResourceProgram(request, true);
    }
}

namespace {

constexpr std::size_t DefaultVariantLimit = 16;

struct CompiledVariant {
    ResourceSpecialization specialization;
    BindingLayout layout;
    CompiledShaderInfo info;
    BindingAllocationResult bindings;
    RecompileResult result;
    std::uint64_t ordinal = 0;
};

struct ResourceProgram {
    explicit ResourceProgram(const RecompileRequest& request) : program(PrepareResourceProgram(request)), plan(ResourceMaterializer{}.ExtractPlan(program)) {}

    IrProgram program;
    IrResourcePlan plan;
};

std::shared_ptr<const IrResourcePlan> makeResourcePlan(const RecompileRequest& request) {
    const auto resource = std::make_shared<ResourceProgram>(request);
    return std::shared_ptr<const IrResourcePlan>(resource, &resource->plan);
}

struct SourceEntry {
    std::mutex mutex;
    std::shared_ptr<const IrResourcePlan> plan;
    std::unique_ptr<IrProgram> spare;
    std::vector<std::shared_ptr<const CompiledVariant>> variants;
    std::uint64_t compiled = 0;
    std::uint64_t evicted = 0;
    std::vector<std::uint64_t> evictedKeys;
};

struct SourceKeyHash {
    std::size_t operator()(const std::vector<std::uint64_t>& key) const {
        std::size_t hash = 0;
        for (const auto value : key) {
            hash ^= static_cast<std::size_t>(value) + static_cast<std::size_t>(0x9e3779b97f4a7c15ull) + (hash << 6u) + (hash >> 2u);
            if constexpr (sizeof(std::size_t) < sizeof(value)) hash ^= static_cast<std::size_t>(value >> 32u);
        }
        return hash;
    }
};

std::shared_ptr<SourceEntry> getSource(const RecompileRequest& request) {
    static std::shared_mutex mutex;
    static std::unordered_map<std::vector<std::uint64_t>, std::shared_ptr<SourceEntry>, SourceKeyHash> sources;
    thread_local std::vector<std::uint64_t>* keyStorage = nullptr;
    if (keyStorage == nullptr) keyStorage = new std::vector<std::uint64_t>();
    auto& key = *keyStorage;
    RecompileCacheKey::Build(request, key);
    struct Recent {
        const std::vector<std::uint64_t>* key;
        const std::shared_ptr<SourceEntry>* source;
    };
    thread_local std::array<Recent, 4> recent{};
    thread_local std::size_t nextRecent = 0;
    for (const auto& entry : recent) {
        if (entry.source != nullptr && *entry.key == key) return *entry.source;
    }
    const std::pair<const std::vector<std::uint64_t>, std::shared_ptr<SourceEntry>>* cached = nullptr;
    {
        std::shared_lock lock(mutex);
        const auto found = sources.find(key);
        if (found != sources.end()) cached = &*found;
    }
    if (cached == nullptr) {
        static_cast<void>(BuildShaderStageInputInfo(toShaderStageKind(request.shader.stage), request.context, request.target.subgroupSize));
        std::unique_lock lock(mutex);
        auto found = sources.find(key);
        if (found == sources.end()) found = sources.emplace(key, std::make_shared<SourceEntry>()).first;
        cached = &*found;
    }
    const auto& source = cached->second;
    {
        std::lock_guard lock(source->mutex);
        if (source->plan == nullptr) {
            auto spare = std::async(std::launch::async, [&request] { return PrepareResourceProgram(request); });
            source->plan = makeResourcePlan(request);
            source->spare = std::make_unique<IrProgram>(spare.get());
        }
    }
    recent[nextRecent++ % recent.size()] = {&cached->first, &cached->second};
    return source;
}

bool dualLaneWave64Forced() {
    static const bool forced = std::getenv("ANYPS5_DUAL_LANE") != nullptr;
    return forced;
}

CompiledVariant compileVariant(const RecompileRequest& request, IrProgram program, const ResourceSnapshot& resourceSnapshot, const ResourceSpecialization& resourceSpecialization) {
    const auto inputInfo = BuildShaderStageInputInfo(toShaderStageKind(request.shader.stage), request.context, request.target.subgroupSize);
    constexpr DeadCodeEliminator deadCodeEliminator;
    constexpr ResourceMaterializer resourceMaterializer;
    resourceMaterializer.Apply(program, resourceSpecialization);

    deadCodeEliminator.RemoveIdentities(program);
    deadCodeEliminator.Eliminate(program);

    constexpr ShaderInfoCollector shaderInfoCollector;
    shaderInfoCollector.Collect(program, inputInfo);

    constexpr BindingAllocator bindingAllocator;
    auto bindings = bindingAllocator.Allocate(program, request.layout);

    constexpr DescriptorBindingBuilder descriptorBindingBuilder;
    descriptorBindingBuilder.Populate(bindings, program, resourceSnapshot);

    SpirvTargetOptions targetOptions {};
    targetOptions.vulkanVersion = request.target.vulkanVersion;
    targetOptions.spirvVersion = request.target.spirvVersion;
    targetOptions.subgroupSize = request.target.subgroupSize;
    targetOptions.bdaAbiVersion = request.target.bdaAbiVersion;
    targetOptions.supportedCapabilities = request.target.supportedCapabilities;
    targetOptions.supportedExtensions = request.target.supportedExtensions;
    targetOptions.subgroupStageMask = request.target.subgroupStageMask;
    targetOptions.maxWorkgroupSize = request.target.maxWorkgroupSize;
    targetOptions.maxWorkgroupInvocations = request.target.maxWorkgroupInvocations;
    targetOptions.dualLaneWave64 = request.target.dualLaneWave64 || dualLaneWave64Forced();

    constexpr SpirvEmitter spirvEmitter;
    RecompileResult result;
    result.spirv = spirvEmitter.Emit(program, inputInfo, bindings, targetOptions);
    result.lanesPerInvocation = LanesPerInvocation(program, inputInfo, targetOptions);

#if ANYPS5_ENABLE_SPIRV_TOOLS
    result.spirv = ValidateAndOptimizeSpirv(result.spirv, request.target.vulkanVersion, request.target.spirvVersion);
#endif

    {
        std::uint64_t hash = 0x9e3779b97f4a7c15ull ^ result.spirv.size();
        for (const auto word : result.spirv) {
            hash = (hash ^ word) * 0xff51afd7ed558ccdull;
            hash ^= hash >> 32u;
        }
        result.spirvHash = hash != 0 ? hash : 1;
    }
    result.bdaAbiVersion = program.Info().usesDma ? request.target.bdaAbiVersion : 0u;
    result.unresolvedImages = program.Resources().unresolvedImages;
    result.vertexOffsetSgpr = program.Info().vertexOffsetSgpr;
    result.instanceOffsetSgpr = program.Info().instanceOffsetSgpr;
    for (const auto& output : program.Info().outputs) {
        if (output.kind == StageOutputKind::Parameter) result.parameterExports.push_back(output.location);
    }
    if (request.shader.stage == ShaderStage::Fragment) result.fragmentParameters = DescribeFragmentParameters(program, inputInfo);
    if (request.shader.stage == ShaderStage::Vertex || request.shader.stage == ShaderStage::Local) {
        if (inputInfo.vertex == nullptr) throw std::runtime_error("vertex input metadata is missing");
        for (const auto& input : program.Info().inputs) {
            if (input.kind != StageInputKind::Parameter) continue;
            if (input.location >= static_cast<std::uint32_t>(inputInfo.vertex->resourcesNum)) throw std::runtime_error("vertex attribute location exceeds resource count");
            result.vertexAttributes.push_back({input.location, input.componentCount, {inputInfo.vertex->resources[input.location].fields}, inputInfo.vertex->resourcesDst[input.location].fetchIndex});
        }
    }

    result.bindings.clear();
    result.pushConstants.clear();
    for (auto& attribute : result.vertexAttributes) attribute.resource = {};
    bindings.bindings.clear();
    bindings.pushConstants.clear();
    return {resourceSpecialization, request.layout, std::move(program).TakeCompiledInfo(), std::move(bindings), std::move(result)};
}

RecompileResult materializeResult(const CompiledVariant& variant, const RecompileRequest& request, const ResourceSnapshot& snapshot) {
    auto result = variant.result;
    BindingAllocationResult bindings;
    bindings.layout = variant.bindings.layout;
    bindings.pushConstantOffsetBytes = variant.bindings.pushConstantOffsetBytes;
    bindings.pushConstantSizeBytes = variant.bindings.pushConstantSizeBytes;
    DescriptorBindingBuilder{}.Populate(bindings, variant.info.info, variant.info.stage, variant.info.userDataBase, snapshot);
    result.bindings = std::move(bindings.bindings);
    result.pushConstants = std::move(bindings.pushConstants);
    for (auto& attribute : result.vertexAttributes) {
        if (!request.context.vertex || attribute.location >= request.context.vertex->resourcesNum) throw std::runtime_error("Shader cache: invalid vertex attribute metadata");
        attribute.resource = request.context.vertex->resources[attribute.location];
    }
    return result;
}

bool sameLayout(const BindingLayout& left, const BindingLayout& right) {
    return left.descriptorSet == right.descriptorSet && left.firstBinding == right.firstBinding && left.pushConstantOffsetBytes == right.pushConstantOffsetBytes && left.pushConstantSizeBytes == right.pushConstantSizeBytes;
}

std::size_t variantLimit() {
    static const std::size_t limit = [] {
        const char* value = std::getenv("ANYPS5_SHADER_VARIANT_LIMIT");
        char* end = nullptr;
        const auto parsed = value != nullptr ? std::strtoull(value, &end, 10) : 0ull;
        if (value == nullptr || end == value || *end != '\0') {
            return DefaultVariantLimit;
        }
        return parsed == 0 ? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(parsed);
    }();
    return limit;
}

bool traceVariants() {
    static const bool enabled = std::getenv("ANYPS5_TRACE_VARIANTS") != nullptr;
    return enabled;
}

const char* stageName(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Compute:
        return "compute";
    case ShaderStage::Vertex:
        return "vertex";
    case ShaderStage::TessellationControl:
        return "hull";
    case ShaderStage::TessellationEvaluation:
        return "domain";
    case ShaderStage::Geometry:
        return "geometry";
    case ShaderStage::Fragment:
        return "pixel";
    case ShaderStage::Local:
        return "local";
    case ShaderStage::Mesh:
        return "mesh";
    }
    return "unknown";
}

const char* numericClassName(IrTextureNumericClass numericClass) {
    switch (numericClass) {
    case IrTextureNumericClass::Float:
        return "float";
    case IrTextureNumericClass::Uint:
        return "uint";
    case IrTextureNumericClass::Sint:
        return "sint";
    case IrTextureNumericClass::Unsupported:
        break;
    }
    return "unsupported";
}

std::uint64_t variantKey(const BindingLayout& layout, const ResourceSpecialization& specialization) {
    std::uint64_t hash = 0x9e3779b97f4a7c15ull;
    const auto mix = [&hash](std::uint64_t value) {
        hash = (hash ^ value) * 0xff51afd7ed558ccdull;
        hash ^= hash >> 32u;
    };
    mix(layout.descriptorSet);
    mix(layout.firstBinding);
    mix(layout.pushConstantOffsetBytes);
    mix(layout.pushConstantSizeBytes);
    mix(specialization.buffers.size());
    for (const auto& buffer : specialization.buffers) {
        mix(buffer.packedStride);
        mix(static_cast<std::uint64_t>(buffer.descriptorFormat));
        mix(buffer.descriptorSwizzle);
    }
    mix(specialization.images.size());
    for (const auto& image : specialization.images) {
        mix(static_cast<std::uint64_t>(image.numericClass));
        mix(static_cast<std::uint64_t>(image.dimension));
        mix(image.mipCount);
        mix(static_cast<std::uint64_t>(image.conversionFormat));
        mix(image.shaderSwizzle);
        mix(image.indirectRoot);
        mix(image.indirectMappingOffset);
        mix(image.indirectSearchIterations);
        mix(image.cube ? 1u : 0u);
        mix(image.fmask ? 1u : 0u);
    }
    return hash;
}

std::size_t differenceCount(const CompiledVariant& left, const CompiledVariant& right) {
    const auto& before = left.specialization;
    const auto& after = right.specialization;
    std::size_t count = sameLayout(left.layout, right.layout) ? 0u : 1u;
    count += std::max(before.buffers.size(), after.buffers.size()) - std::min(before.buffers.size(), after.buffers.size());
    count += std::max(before.images.size(), after.images.size()) - std::min(before.images.size(), after.images.size());
    for (std::size_t index = 0; index < std::min(before.buffers.size(), after.buffers.size()); ++index) {
        count += before.buffers[index] == after.buffers[index] ? 0u : 1u;
    }
    for (std::size_t index = 0; index < std::min(before.images.size(), after.images.size()); ++index) {
        count += before.images[index] == after.images[index] ? 0u : 1u;
    }
    return count;
}

template<typename... TValues>
void appendFormatted(std::string& text, const char* format, TValues... values) {
    char item[192];
    const int length = std::snprintf(item, sizeof(item), format, values...);
    if (length > 0) {
        text.append(item, std::min(static_cast<std::size_t>(length), sizeof(item) - 1u));
    }
}

std::string describeChange(const CompiledVariant& from, const CompiledVariant& to) {
    std::string text;
    if (!sameLayout(from.layout, to.layout)) {
        appendFormatted(text, "; layout set %u binding %u push constants %u+%u -> set %u binding %u push constants %u+%u", from.layout.descriptorSet, from.layout.firstBinding, from.layout.pushConstantOffsetBytes, from.layout.pushConstantSizeBytes, to.layout.descriptorSet, to.layout.firstBinding, to.layout.pushConstantOffsetBytes, to.layout.pushConstantSizeBytes);
    }
    const auto& before = from.specialization;
    const auto& after = to.specialization;
    if (before.buffers.size() != after.buffers.size()) {
        appendFormatted(text, "; buffer count %zu -> %zu", before.buffers.size(), after.buffers.size());
    }
    for (std::size_t index = 0; index < std::min(before.buffers.size(), after.buffers.size()); ++index) {
        const auto& left = before.buffers[index];
        const auto& right = after.buffers[index];
        if (left == right) {
            continue;
        }
        appendFormatted(text, "; buffer %zu", index);
        if (left.packedStride != right.packedStride) {
            appendFormatted(text, " packedStride 0x%x -> 0x%x", left.packedStride, right.packedStride);
        }
        if (left.descriptorFormat != right.descriptorFormat) {
            appendFormatted(text, " format %u -> %u", static_cast<unsigned>(left.descriptorFormat), static_cast<unsigned>(right.descriptorFormat));
        }
        if (left.descriptorSwizzle != right.descriptorSwizzle) {
            appendFormatted(text, " swizzle 0x%03x -> 0x%03x", left.descriptorSwizzle, right.descriptorSwizzle);
        }
    }
    if (before.images.size() != after.images.size()) {
        appendFormatted(text, "; image count %zu -> %zu", before.images.size(), after.images.size());
    }
    for (std::size_t index = 0; index < std::min(before.images.size(), after.images.size()); ++index) {
        const auto& left = before.images[index];
        const auto& right = after.images[index];
        if (left == right) {
            continue;
        }
        appendFormatted(text, "; image %zu", index);
        if (left.numericClass != right.numericClass) {
            appendFormatted(text, " numericClass %s -> %s", numericClassName(left.numericClass), numericClassName(right.numericClass));
        }
        if (left.dimension != right.dimension) {
            appendFormatted(text, " dimension %s -> %s", RdnaImageDimensionToString(left.dimension), RdnaImageDimensionToString(right.dimension));
        }
        if (left.mipCount != right.mipCount) {
            appendFormatted(text, " mipCount %u -> %u", left.mipCount, right.mipCount);
        }
        if (left.conversionFormat != right.conversionFormat) {
            appendFormatted(text, " conversionFormat %u -> %u", static_cast<unsigned>(left.conversionFormat), static_cast<unsigned>(right.conversionFormat));
        }
        if (left.shaderSwizzle != right.shaderSwizzle) {
            appendFormatted(text, " shaderSwizzle 0x%03x -> 0x%03x", left.shaderSwizzle, right.shaderSwizzle);
        }
        if (left.indirectRoot != right.indirectRoot || left.indirectMappingOffset != right.indirectMappingOffset || left.indirectSearchIterations != right.indirectSearchIterations) {
            appendFormatted(text, " indirect root/offset/iterations %d/%u/%u -> %d/%u/%u", static_cast<int>(left.indirectRoot), left.indirectMappingOffset, left.indirectSearchIterations, static_cast<int>(right.indirectRoot), right.indirectMappingOffset, right.indirectSearchIterations);
        }
        if (left.cube != right.cube) {
            appendFormatted(text, " cube %u -> %u", left.cube ? 1u : 0u, right.cube ? 1u : 0u);
        }
        if (left.fmask != right.fmask) {
            appendFormatted(text, " fmask %u -> %u", left.fmask ? 1u : 0u, right.fmask ? 1u : 0u);
        }
    }
    return text;
}

void traceVariant(const RecompileRequest& request, const SourceEntry& source, const CompiledVariant& created) {
    if (source.variants.empty()) {
        return;
    }
    const CompiledVariant* nearest = nullptr;
    std::size_t nearestDifferences = std::numeric_limits<std::size_t>::max();
    for (const auto& variant : source.variants) {
        const auto differences = differenceCount(*variant, created);
        if (differences < nearestDifferences) {
            nearest = variant.get();
            nearestDifferences = differences;
        }
    }
    const auto key = variantKey(created.layout, created.specialization);
    const bool recreated = std::find(source.evictedKeys.begin(), source.evictedKeys.end(), key) != source.evictedKeys.end();
    const auto change = describeChange(*nearest, created);
    std::fprintf(stderr, "shader recompiler: %s program 0x%llx code hash 0x%016llx compiles variant %llu%s (%zu resident, %llu evicted); variant %llu differs in%s\n", stageName(request.shader.stage), static_cast<unsigned long long>(request.shader.codeAddress), static_cast<unsigned long long>(request.shader.codeHash), static_cast<unsigned long long>(created.ordinal), recreated ? " again after evicting it" : "", source.variants.size(), static_cast<unsigned long long>(source.evicted), static_cast<unsigned long long>(nearest->ordinal), change.empty() ? " nothing" : change.c_str() + 1);
    std::fflush(stderr);
}

void traceEviction(const RecompileRequest& request, SourceEntry& source, const CompiledVariant& evicted) {
    source.evictedKeys.push_back(variantKey(evicted.layout, evicted.specialization));
    std::fprintf(stderr, "shader recompiler: %s program 0x%llx code hash 0x%016llx evicts variant %llu, the least recently used of %zu\n", stageName(request.shader.stage), static_cast<unsigned long long>(request.shader.codeAddress), static_cast<unsigned long long>(request.shader.codeHash), static_cast<unsigned long long>(evicted.ordinal), source.variants.size());
    std::fflush(stderr);
}

RecompileResult RecompileImpl(const RecompileRequest& request) {
    ResourceSnapshot snapshot;
    ResourceSpecialization specialization;
    constexpr ResourceMaterializer materializer;
    if (!request.useCache) {
        static_cast<void>(BuildShaderStageInputInfo(toShaderStageKind(request.shader.stage), request.context, request.target.subgroupSize));
        RequestMemoryView memory(request.context.memory);
        const auto runtime = memory.MakeRuntime(request.context.userData, request.shader.codeAddress);
        auto program = PrepareResourceProgram(request);
        const auto plan = materializer.ExtractPlan(program);
        materializer.Materialize(plan, runtime, snapshot, specialization);
        const auto variant = compileVariant(request, std::move(program), snapshot, specialization);
        return materializeResult(variant, request, snapshot);
    }
    const auto source = request.source != nullptr ? std::static_pointer_cast<SourceEntry>(request.source) : getSource(request);
    const auto* snapshotUsed = request.materializedSnapshot;
    const auto* specializationUsed = request.materializedSpecialization;
    if (snapshotUsed == nullptr || specializationUsed == nullptr) {
        RequestMemoryView memory(request.context.memory);
        const auto runtime = memory.MakeRuntime(request.context.userData, request.shader.codeAddress);
        materializer.Materialize(*source->plan, runtime, snapshot, specialization);
        snapshotUsed = &snapshot;
        specializationUsed = &specialization;
    }
    std::shared_ptr<const CompiledVariant> variant;
    bool cacheHit = false;
    {
        std::lock_guard lock(source->mutex);
        auto& variants = source->variants;
        for (auto candidate = variants.end(); candidate != variants.begin();) {
            --candidate;
            if (sameLayout((*candidate)->layout, request.layout) && (*candidate)->specialization == *specializationUsed) {
                variant = *candidate;
                cacheHit = true;
                std::rotate(candidate, std::next(candidate), variants.end());
                break;
            }
        }
        if (variant == nullptr) {
            auto program = source->spare != nullptr ? std::move(*source->spare) : PrepareResourceProgram(request);
            source->spare.reset();
            auto created = std::make_shared<CompiledVariant>(compileVariant(request, std::move(program), *snapshotUsed, *specializationUsed));
            created->ordinal = ++source->compiled;
            if (traceVariants()) {
                traceVariant(request, *source, *created);
            }
            if (variants.size() >= variantLimit()) {
                if (traceVariants()) {
                    traceEviction(request, *source, *variants.front());
                }
                variants.erase(variants.begin());
                ++source->evicted;
            }
            variants.push_back(created);
            variant = std::move(created);
        }
    }
    auto result = materializeResult(*variant, request, *snapshotUsed);
    result.cacheHit = cacheHit;
    return result;
}

}

std::shared_ptr<const IrResourcePlan> GetResourcePlan(const RecompileRequest& request, std::shared_ptr<void>* source) {
    if (request.useCache) {
        auto entry = getSource(request);
        auto plan = entry->plan;
        if (source != nullptr) *source = std::move(entry);
        return plan;
    }
    static_cast<void>(BuildShaderStageInputInfo(toShaderStageKind(request.shader.stage), request.context, request.target.subgroupSize));
    return makeResourcePlan(request);
}

RecompileResult Recompile(const RecompileRequest& request) {
    try {
        return RecompileImpl(request);
    } catch (const std::exception& e) {
        constexpr auto requestSerializer = RequestSerializer{};
        const auto inputInfo = "\nRecompileRequest:\n" + requestSerializer.Serialize(request);
        throw std::runtime_error(std::string("ShaderRecompiler::Recompile: ") + e.what() + inputInfo);
    } catch (...) {
        throw std::runtime_error("ShaderRecompiler::Recompile: unknown exception");
    }
}

}

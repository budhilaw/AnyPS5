#include "Optimization/ResourceMaterializer.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "Optimization/SrtWalker/SrtAddressArithmetic.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "SpirvBackend/SpirvEmitterState.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdio>
#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

namespace {

constexpr std::uint32_t NoRemap = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint64_t ApplicationMemoryEnd = std::uint64_t{1} << 40u;
constexpr std::size_t EmptyImageTableReportLimit = 256u;

struct DecodedImage {
    IrTextureNumericClass numericClass = IrTextureNumericClass::Unsupported;
    RdnaImageDimension dimension = RdnaImageDimension::Unknown;
    std::uint32_t mipCount = 1;
    IrBufferFormat conversionFormat = IrBufferFormat::Invalid;
    std::uint32_t shaderSwizzle = ShaderImageIdentitySwizzle;
    bool cube = false;
    bool fmask = false;
    bool reshaped = false;
};

ShaderBufferResource decodeBufferDescriptor(const DescriptorValue& value) {
    if (value.dwordCount != 4u) {
        throw std::runtime_error("buffer descriptor has an invalid width");
    }
    ShaderBufferResource result;
    for (std::uint32_t i = 0; i < 4u; i++) {
        result.fields[i] = value.dwords[i];
    }
    return result;
}

bool reachableCandidate(const IrResourcePlan& plan, const BufferResource& buffer, std::span<const std::uint32_t> flattenedSrt) {
    if (buffer.selectorBound >= plan.selectorBounds.size()) {
        return true;
    }
    const auto& bound = plan.selectorBounds[buffer.selectorBound];
    if (bound.compare == nullptr || bound.flatSlot >= flattenedSrt.size()) {
        return true;
    }
    const auto left = bound.selectorArgument == 0u ? buffer.candidate : flattenedSrt[bound.flatSlot];
    const auto right = bound.selectorArgument == 0u ? flattenedSrt[bound.flatSlot] : buffer.candidate;
    const auto signedLeft = std::bit_cast<std::int32_t>(left);
    const auto signedRight = std::bit_cast<std::int32_t>(right);
    switch (bound.compare->Opcode()) {
        case IrOpcode::ULessThan32:
            return (left < right) == bound.holds;
        case IrOpcode::ULessThanEqual32:
            return (left <= right) == bound.holds;
        case IrOpcode::UGreaterThan32:
            return (left > right) == bound.holds;
        case IrOpcode::UGreaterThanEqual32:
            return (left >= right) == bound.holds;
        case IrOpcode::SLessThan32:
            return (signedLeft < signedRight) == bound.holds;
        case IrOpcode::SLessThanEqual32:
            return (signedLeft <= signedRight) == bound.holds;
        case IrOpcode::SGreaterThan32:
            return (signedLeft > signedRight) == bound.holds;
        case IrOpcode::SGreaterThanEqual32:
            return (signedLeft >= signedRight) == bound.holds;
        default:
            return true;
    }
}

bool nullImageDescriptor(const DescriptorValue& descriptor) {
    return descriptor.dwords[0] == 0u && (descriptor.dwords[1] & 0xffu) == 0u;
}

std::uint64_t imageBaseAddress(const DescriptorValue& descriptor) {
    return (static_cast<std::uint64_t>(descriptor.dwords[1] & 0xffu) << 40u) | (static_cast<std::uint64_t>(descriptor.dwords[0]) << 8u);
}

ImageType rawImageType(const DescriptorValue& descriptor) {
    return static_cast<ImageType>((descriptor.dwords[3] >> 28u) & 0xfu);
}

IrBufferFormat rawImageFormat(const DescriptorValue& descriptor) {
    return static_cast<IrBufferFormat>((descriptor.dwords[1] >> 20u) & 0x1ffu);
}

std::uint32_t descriptorImageSwizzle(const DescriptorValue& descriptor) {
    return descriptor.dwords[3] & 0xfffu;
}

bool descriptorIsCube(const DescriptorValue& descriptor) {
    return rawImageType(descriptor) == ImageType::Cube;
}

RdnaImageDimension descriptorDimension(const DescriptorValue& descriptor, RdnaImageDimension requested) {
    const bool wantArray = requested == RdnaImageDimension::Dim1DArray || requested == RdnaImageDimension::Dim2DArray || requested == RdnaImageDimension::Dim2DMsaaArray;
    switch (rawImageType(descriptor)) {
        case ImageType::Color1D:
            return RdnaImageDimension::Dim1D;
        case ImageType::Color1DArray:
            return wantArray ? RdnaImageDimension::Dim1DArray : RdnaImageDimension::Dim1D;
        case ImageType::Color3D:
            return RdnaImageDimension::Dim3D;
        case ImageType::Cube:
            return RdnaImageDimension::Dim2DArray;
        case ImageType::Color2DArray:
            return wantArray ? RdnaImageDimension::Dim2DArray : RdnaImageDimension::Dim2D;
        case ImageType::Color2DMsaaArray:
            return wantArray ? RdnaImageDimension::Dim2DMsaaArray : RdnaImageDimension::Dim2DMsaa;
        case ImageType::Color2D:
            return RdnaImageDimension::Dim2D;
        case ImageType::Color2DMsaa:
            return RdnaImageDimension::Dim2DMsaa;
        default:
            throw std::runtime_error("image descriptor has an unsupported type");
    }
}

bool takesInstructionDimension(RdnaImageDimension descriptor, RdnaImageDimension instruction) {
    return RdnaImageDimensionInfoFor(descriptor).coordinateComponents > RdnaImageDimensionInfoFor(instruction).coordinateComponents || (descriptor == RdnaImageDimension::Dim2D && instruction == RdnaImageDimension::Dim2DArray);
}

const char* imageTypeName(ImageType type) {
    switch (type) {
        case ImageType::Color1D:
            return "1d";
        case ImageType::Color2D:
            return "2d";
        case ImageType::Color3D:
            return "3d";
        case ImageType::Cube:
            return "cube";
        case ImageType::Color1DArray:
            return "1d_array";
        case ImageType::Color2DArray:
            return "2d_array";
        case ImageType::Color2DMsaa:
            return "2d_msaa";
        case ImageType::Color2DMsaaArray:
            return "2d_msaa_array";
    }
    return "unknown";
}

void reportReshapedImage(ImageType type, RdnaImageDimension instruction) {
    static std::atomic<std::uint64_t> reported{0};
    const std::uint64_t pair = std::uint64_t{1} << (((static_cast<std::uint32_t>(type) & 7u) << 3u) | (static_cast<std::uint32_t>(instruction) & 7u));
    if ((reported.load(std::memory_order_relaxed) & pair) != 0u || (reported.fetch_or(pair, std::memory_order_relaxed) & pair) != 0u) {
        return;
    }
    std::fprintf(stderr, "shader recompiler: %s image descriptors read by %s image instructions are specialized to the instruction's dimension\n", imageTypeName(type), RdnaImageDimensionToString(instruction));
}

bool validImageDescriptor(const DescriptorValue& descriptor, bool r128) {
    const auto type = rawImageType(descriptor);
    const auto format = rawImageFormat(descriptor);
    if (type < ImageType::Color1D || format == IrBufferFormat::Invalid) {
        return false;
    }
    if (r128 && type != ImageType::Color1D && type != ImageType::Color2D && type != ImageType::Color2DMsaa) {
        return false;
    }
    if (type == ImageType::Color2DMsaa || type == ImageType::Color2DMsaaArray) {
        const auto baseLevel = (descriptor.dwords[3] >> 12u) & 0xfu;
        const auto fragments = (descriptor.dwords[3] >> 16u) & 0xfu;
        const auto maxMip = (descriptor.dwords[5] >> 4u) & 0xfu;
        return baseLevel == 0u && fragments >= 1u && fragments <= 3u && (r128 || maxMip == fragments);
    }
    return true;
}

std::uint32_t storageMipCount(const ImageResource& base, const DescriptorValue& descriptor) {
    if (base.mipMode != ImageMipMode::DynamicStorage || nullImageDescriptor(descriptor)) {
        return 1u;
    }
    const auto mipBase = (descriptor.dwords[3] >> 12u) & 0xfu;
    const auto mipLast = (descriptor.dwords[3] >> 16u) & 0xfu;
    return mipBase <= mipLast ? mipLast - mipBase + 1u : 0u;
}

bool sameImageShape(const ResourceSpecialization::Image& left, const ResourceSpecialization::Image& right) {
    return left.numericClass == right.numericClass && left.dimension == right.dimension && left.mipCount == right.mipCount && left.conversionFormat == right.conversionFormat && left.shaderSwizzle == right.shaderSwizzle && left.cube == right.cube && left.fmask == right.fmask;
}

DecodedImage decodeImageDescriptor(const DescriptorValue& descriptor, const ImageResource& base) {
    DecodedImage decoded;
    decoded.mipCount = storageMipCount(base, descriptor);
    if (decoded.mipCount == 0u) {
        throw std::runtime_error("storage image descriptor has an invalid mip range");
    }
    if (nullImageDescriptor(descriptor)) {
        decoded.numericClass = base.atomic ? IrTextureNumericClass::Uint : IrTextureNumericClass::Float;
        decoded.dimension = RdnaImageDimension::Dim2D;
        decoded.cube = false;
        return decoded;
    }
    if (base.resourceClass == ImageResourceClass::None || (base.atomic && base.resourceClass != ImageResourceClass::Storage)) {
        throw std::runtime_error("image resource has an invalid class");
    }
    if (!validImageDescriptor(descriptor, base.r128)) {
        throw std::runtime_error("image descriptor is invalid");
    }
    decoded.dimension = descriptorDimension(descriptor, base.dimension);
    decoded.cube = descriptorIsCube(descriptor);
    if (takesInstructionDimension(decoded.dimension, base.dimension)) {
        decoded.dimension = base.dimension;
        decoded.cube = false;
        decoded.reshaped = true;
    }
    const auto format = rawImageFormat(descriptor);
    if (base.atomic && format != IrBufferFormat::Format32UInt) {
        throw std::runtime_error("atomic image descriptor uses an unsupported format");
    }
    const bool storage = base.resourceClass == ImageResourceClass::Storage;
    decoded.fmask = IsFmaskTextureFormat(format);
    if (decoded.fmask && (storage || base.depthCompare || base.indirectRoot != ImageResource::NoIndirectImage)) {
        throw std::runtime_error("FMASK requires a direct sampled image load");
    }
    decoded.conversionFormat = RemapTextureFormat(format) != format ? format : IrBufferFormat::Invalid;
    if (storage || decoded.conversionFormat != IrBufferFormat::Invalid) {
        decoded.shaderSwizzle = descriptorImageSwizzle(descriptor);
    }
    const bool rawSintStorage = storage && format == IrBufferFormat::Format32SInt && base.written && !base.read && !base.atomic;
    decoded.numericClass = SampledTextureNumericClass(format);
    if (storage) {
        if ((!rawSintStorage && decoded.numericClass == IrTextureNumericClass::Sint) || decoded.numericClass == IrTextureNumericClass::Unsupported) {
            throw std::runtime_error("storage image descriptor uses an unsupported format");
        }
        if (rawSintStorage) {
            decoded.numericClass = IrTextureNumericClass::Uint;
        }
    } else if (decoded.numericClass == IrTextureNumericClass::Unsupported || (base.depthCompare && decoded.numericClass != IrTextureNumericClass::Float)) {
        throw std::runtime_error("sampled image descriptor uses an unsupported format");
    }
    return decoded;
}

bool requiresPointSampler(const ResourceSpecialization::Image& image) {
    return image.numericClass == IrTextureNumericClass::Sint || image.conversionFormat != IrBufferFormat::Invalid;
}

void resolveIndirectImage(const IrResourcePlan& plan, std::uint32_t imageIndex, const DescriptorSource::IndirectImage& indirect, const SrtRuntime& runtime, SrtWalker& walker, DescriptorValue& resolved, std::vector<DescriptorValue>& candidates) {
    const auto& image = plan.info.images.at(imageIndex);
    if (image.indirectSearchIterations == 0u) {
        throw std::runtime_error("indirect image has no candidate range");
    }
    DescriptorValue materialValue;
    walker.EvaluateDescriptorSource(plan, indirect.materialSource, runtime, materialValue);
    const ShaderBufferResource material = decodeBufferDescriptor(materialValue);
    if (material.GetSize() == 0u) {
        throw std::runtime_error("indirect image material buffer is empty");
    }
    DescriptorValue heapValue;
    walker.EvaluateDescriptorSource(plan, indirect.heapSource, runtime, heapValue);
    const ShaderBufferResource heap = decodeBufferDescriptor(heapValue);
    DescriptorValue keyValue;
    walker.EvaluateDescriptorSource(plan, indirect.keyArg, runtime, keyValue);
    if (keyValue.dwordCount == 0u) {
        throw std::runtime_error("indirect image selector has no value");
    }
    if (runtime.readMemory == nullptr) {
        throw std::runtime_error("indirect image resolution requires runtime memory access");
    }
    candidates.clear();
    candidates.reserve(image.indirectSearchIterations);
    const std::uint64_t heapBase = heap.Base48();
    for (std::uint32_t iteration = 0; iteration < image.indirectSearchIterations; iteration++) {
        DescriptorValue candidate;
        candidate.dwordCount = 8u;
        const std::uint64_t address = heapBase + indirect.selectorOffset + static_cast<std::uint64_t>(iteration) * indirect.selectorStride;
        for (std::uint32_t dword = 0; dword < 8u; dword++) {
            if (!runtime.readMemory(runtime.userContext, address + dword * sizeof(std::uint32_t), &candidate.dwords[dword])) {
                throw std::runtime_error("failed to read indirect image candidate from memory");
            }
        }
        if (nullImageDescriptor(candidate)) {
            candidate.dwords.fill(0u);
        }
        candidates.push_back(candidate);
    }
    const auto key = keyValue.dwords[0];
    if (key >= candidates.size()) {
        throw std::runtime_error("indirect image selector is out of range");
    }
    resolved = candidates[key];
}

bool readImageDescriptor(const SrtRuntime& runtime, std::uint64_t address, DescriptorValue& descriptor) {
    descriptor.dwordCount = 8u;
    for (std::uint32_t dword = 0; dword < 8u; dword++) {
        if (!runtime.readMemory(runtime.userContext, address + dword * sizeof(std::uint32_t), &descriptor.dwords[dword])) {
            return false;
        }
    }
    return true;
}

bool readPointerTableEntry(const SrtRuntime& runtime, std::uint64_t address, DescriptorValue& descriptor) {
    try {
        return readImageDescriptor(runtime, address, descriptor);
    } catch (const std::exception&) {
        return false;
    }
}

void reportEmptyImageTable(const IrResourcePlan& plan, std::uint32_t imageIndex, bool pointer, std::uint64_t address, std::uint32_t stride, std::uint32_t readEntries) {
    static std::mutex mutex;
    static std::set<std::tuple<std::uint64_t, std::uint32_t, bool>> reported;
    {
        const std::lock_guard lock(mutex);
        if (reported.size() >= EmptyImageTableReportLimit || !reported.emplace(plan.shaderHash, imageIndex, readEntries != 0u).second) {
            return;
        }
    }
    const char* kind = pointer ? "pointer" : "buffer";
    const char* dimension = RdnaImageDimensionToString(plan.info.images.at(imageIndex).dimension);
    if (address == 0u) {
        std::fprintf(stderr, "shader recompiler: program with code hash 0x%016llx samples a %s image through a %s image table whose base is null; it reads as a null texture\n", static_cast<unsigned long long>(plan.shaderHash), dimension, kind);
    } else {
        std::fprintf(stderr, "shader recompiler: program with code hash 0x%016llx: %s image table at 0x%llx (stride %u) holds no image a %s sample can read in %u readable entries; it reads as a null texture\n", static_cast<unsigned long long>(plan.shaderHash), kind, static_cast<unsigned long long>(address), stride, dimension, readEntries);
    }
    std::fflush(stderr);
}

void resolveTableImage(const IrResourcePlan& plan, std::uint32_t imageIndex, const DescriptorSource::TableImage& table, const SrtRuntime& runtime, SrtWalker& walker, DescriptorValue& resolved) {
    const auto& image = plan.info.images.at(imageIndex);
    resolved = DescriptorValue{};
    resolved.dwordCount = 8u;
    if (runtime.readMemory == nullptr || table.stride == 0u) {
        return;
    }
    DescriptorValue baseValue;
    walker.EvaluateDescriptorSource(plan, table.baseSource, runtime, baseValue);
    const bool pointer = baseValue.dwordCount == 2u;
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    if (pointer) {
        base = ((static_cast<std::uint64_t>(baseValue.dwords[1]) << 32u) | baseValue.dwords[0]) & Detail::AddressMask & ~std::uint64_t {3};
    } else {
        const ShaderBufferResource buffer = decodeBufferDescriptor(baseValue);
        base = buffer.Base48();
        size = buffer.GetSize();
    }
    if (base == 0u) {
        reportEmptyImageTable(plan, imageIndex, pointer, 0u, table.stride, 0u);
        return;
    }
    const auto& instruction = RdnaImageDimensionInfoFor(image.dimension);
    const auto plausible = [&](const DescriptorValue& candidate) {
        const auto address = imageBaseAddress(candidate);
        if (!validImageDescriptor(candidate, image.r128) || address == 0u || address >= ApplicationMemoryEnd) {
            return false;
        }
        const auto& entry = RdnaImageDimensionInfoFor(descriptorDimension(candidate, image.dimension));
        if (entry.coordinateComponents > instruction.coordinateComponents || entry.spatialComponents != instruction.spatialComponents || entry.multisampled != instruction.multisampled) {
            return false;
        }
        for (std::uint32_t component = 0; component < 4u; component++) {
            const auto selector = (candidate.dwords[3] >> (component * 3u)) & 7u;
            if (selector == 2u || selector == 3u) {
                return false;
            }
        }
        try {
            (void)decodeImageDescriptor(candidate, image);
        } catch (const std::exception&) {
            return false;
        }
        return true;
    };
    std::vector<std::pair<DescriptorValue, std::uint32_t>> seen;
    std::uint32_t readEntries = 0;
    for (std::uint64_t at = table.offset, entry = 0; (pointer || at + 32u <= size) && entry < 256u; at += table.stride, entry++) {
        DescriptorValue candidate;
        const bool readable = pointer ? readPointerTableEntry(runtime, (base + at) & ~std::uint64_t {3}, candidate) : readImageDescriptor(runtime, base + at, candidate);
        readEntries += readable ? 1u : 0u;
        const bool usable = readable && plausible(candidate) && (!pointer || seen.empty() || rawImageType(candidate) == rawImageType(seen.front().first));
        if (!usable && pointer) {
            break;
        }
        if (!usable) {
            continue;
        }
        const auto match = std::find_if(seen.begin(), seen.end(), [&](const auto& item) { return item.first.dwords == candidate.dwords; });
        if (match == seen.end()) {
            seen.emplace_back(candidate, 1u);
        } else {
            match->second++;
        }
    }
    if (seen.empty()) {
        reportEmptyImageTable(plan, imageIndex, pointer, base + table.offset, table.stride, readEntries);
        return;
    }
    const auto rank = [&image](const auto& item) { return std::pair {!image.cubeInstruction || descriptorIsCube(item.first), item.second}; };
    const auto chosen = std::max_element(seen.begin(), seen.end(), [&rank](const auto& left, const auto& right) { return rank(left) < rank(right); });
    resolved = chosen->first;
    if (seen.size() > 1u) {
        static std::atomic<int> reported{0};
        if (reported.fetch_add(1) < 8) {
            std::fprintf(stderr, "image table at 0x%llx (stride %u) holds %zu different images; every entry reads the most common one\n", static_cast<unsigned long long>(base + table.offset), table.stride, seen.size());
        }
    }
}

void materializeSnapshot(const IrResourcePlan& plan, const SrtRuntime& runtime, SrtWalker& walker, ResourceSnapshot& snapshot, std::vector<std::vector<DescriptorValue>>& indirectCandidates) {
    snapshot = ResourceSnapshot{};
    if (plan.uniformFill.fill.kind != UniformFillKind::None) {
        const auto words = plan.uniformFill.fill.words;
        if (words == 0u || words > plan.uniformFill.values.size()) {
            throw std::runtime_error("uniform fill plan has an invalid word count");
        }
        std::array<std::uint32_t, 4> stored{};
        walker.EvaluateUniformValues(plan, std::span(plan.uniformFill.values).first(words), runtime, std::span(stored).first(words));
        for (std::uint32_t i = 1; i < words; i++) {
            if (stored[i] != stored[0]) {
                throw std::runtime_error("uniform fill values diverge at runtime");
            }
        }
        snapshot.uniformFill = plan.uniformFill.fill;
        snapshot.uniformFill.value = stored[0];
    }
    if (runtime.userData.size() < plan.userDataCount) {
        throw std::runtime_error("runtime user data is smaller than the shader user data count");
    }
    snapshot.userData.assign(runtime.userData.begin(), runtime.userData.begin() + plan.userDataCount);

    std::vector<DescriptorValue> values;
    std::vector<std::uint8_t> activeSources;
    walker.EvaluateRuntimeSources(plan, plan.materializationSources, runtime, values, snapshot.flattenedSrt, plan.cleanFlatSlots, activeSources);

    std::size_t cursor = 0;
    if (values.size() < plan.info.buffers.size()) {
        throw std::runtime_error("materialization sources are missing buffer descriptors");
    }
    snapshot.buffers.assign(values.begin(), values.begin() + plan.info.buffers.size());
    for (std::uint32_t i = 0; i < plan.info.buffers.size(); i++) {
        if (!plan.info.buffers[i].optional) continue;
        if (!reachableCandidate(plan, plan.info.buffers[i], snapshot.flattenedSrt)) {
            snapshot.buffers[i].dwords.fill(0u);
            continue;
        }
        const ShaderBufferResource decoded = decodeBufferDescriptor(snapshot.buffers[i]);
        bool usable = (decoded.Type() == 0u || decoded.Type() == 3u) && (decoded.fields[1] & 0x40000000u) == 0u && decoded.Base48() != 0u && decoded.GetSize() <= (256ull << 20u);
        if (usable && plan.info.buffers[i].formatted) {
            try {
                (void)GetFormatComponentType(decoded.Format());
            } catch (const std::exception&) {
                usable = false;
            }
            for (std::uint32_t component = 0; component < 4u; component++) {
                const auto selector = (decoded.DstSelXYZW() >> (component * 3u)) & 7u;
                if (selector == 2u || selector == 3u) usable = false;
            }
        }
        if (!usable) snapshot.buffers[i].dwords.fill(0u);
    }
    cursor += plan.info.buffers.size();

    snapshot.images.resize(plan.info.images.size());
    indirectCandidates.assign(plan.info.images.size(), {});
    for (std::uint32_t i = 0; i < plan.info.images.size(); i++) {
        const auto& image = plan.info.images[i];
        if (image.source >= plan.descriptorSources.size()) {
            throw std::runtime_error("image resource references an unknown descriptor source");
        }
        const auto& source = plan.descriptorSources[image.source];
        if (source.tableImage.has_value()) {
            resolveTableImage(plan, i, *source.tableImage, runtime, walker, snapshot.images[i]);
            continue;
        }
        if (source.indirectImage.has_value()) {
            if (image.source < activeSources.size() && activeSources[image.source] == 0u) {
                snapshot.images[i].dwordCount = 8u;
                continue;
            }
            resolveIndirectImage(plan, i, *source.indirectImage, runtime, walker, snapshot.images[i], indirectCandidates[i]);
            continue;
        }
        if (cursor >= values.size()) {
            throw std::runtime_error("materialization sources are missing image descriptors");
        }
        auto descriptor = values[cursor];
        cursor++;
        if (descriptor.dwordCount != 8u) {
            throw std::runtime_error("image descriptor has an invalid width");
        }
        if (!validImageDescriptor(descriptor, image.r128) && !nullImageDescriptor(descriptor)) {
            descriptor.dwords.fill(0u);
        }
        snapshot.images[i] = descriptor;
    }

    if (values.size() < cursor + plan.info.samplers.size()) {
        throw std::runtime_error("materialization sources are missing sampler descriptors");
    }
    snapshot.samplers.assign(values.begin() + cursor, values.begin() + cursor + plan.info.samplers.size());
}

void buildResourceSpecialization(const IrResourcePlan& plan, ResourceSnapshot& snapshot, std::vector<std::vector<DescriptorValue>>& indirectCandidates, ResourceSpecialization& specialization) {
    ResourceSpecialization result;
    result.buffers.reserve(plan.info.buffers.size());
    for (std::uint32_t i = 0; i < plan.info.buffers.size(); i++) {
        const ShaderBufferResource decoded = decodeBufferDescriptor(snapshot.buffers[i]);
        if (decoded.Type() != 0u && decoded.Type() != 3u) {
            char message[160];
            std::snprintf(message, sizeof(message), "buffer descriptor %u uses an unsupported type %u: {%08x %08x %08x %08x}", i, decoded.Type(), decoded.fields[0], decoded.fields[1], decoded.fields[2], decoded.fields[3]);
            throw std::runtime_error(message);
        }
        auto packedStride = decoded.PackedStride();
        const auto stride = packedStride & 0x3fffu;
        const bool swizzleActive = stride != 0u && ((packedStride >> 14u) & 1u) != 0u;
        if (stride == 0u) {
            packedStride &= ~((1u << 14u) | (3u << 16u));
        } else if (!swizzleActive) {
            packedStride &= ~(3u << 16u);
        }
        const auto& buffer = plan.info.buffers[i];
        ResourceSpecialization::Buffer entry;
        entry.packedStride = packedStride;
        entry.descriptorFormat = buffer.descriptorFormatted ? decoded.Format() : IrBufferFormat::Invalid;
        const auto swizzledDwords = entry.descriptorFormat == IrBufferFormat::Invalid ? 0u : std::min(buffer.swizzledDwords, 4u);
        const auto swizzledBits = (1u << (swizzledDwords * 3u)) - 1u;
        entry.descriptorSwizzle = (decoded.DstSelXYZW() & swizzledBits) | (DstSel(4, 5, 6, 7) & ~swizzledBits);
        result.buffers.push_back(entry);
    }

    result.images.reserve(plan.info.images.size());
    for (std::uint32_t i = 0; i < plan.info.images.size(); i++) {
        const auto& image = plan.info.images[i];
        const DecodedImage decoded = decodeImageDescriptor(snapshot.images[i], image);
        if (decoded.reshaped) {
            reportReshapedImage(rawImageType(snapshot.images[i]), image.dimension);
        }
        if (decoded.fmask && std::any_of(plan.info.sampledPairs.begin(), plan.info.sampledPairs.end(), [i](const SampledResourcePair& pair) { return pair.image == i; })) {
            throw std::runtime_error("FMASK requires a direct image load");
        }
        ResourceSpecialization::Image entry;
        entry.numericClass = decoded.numericClass;
        entry.dimension = decoded.dimension;
        entry.mipCount = decoded.mipCount;
        entry.conversionFormat = decoded.conversionFormat;
        entry.shaderSwizzle = decoded.shaderSwizzle;
        entry.indirectRoot = ImageResource::NoIndirectImage;
        entry.indirectMappingOffset = 0u;
        entry.indirectSearchIterations = 0u;
        entry.cube = decoded.cube;
        entry.fmask = decoded.fmask;
        result.images.push_back(entry);
    }

    for (std::uint32_t i = 0; i < plan.info.images.size(); i++) {
        auto& candidates = indirectCandidates[i];
        if (candidates.empty()) {
            continue;
        }
        const auto& image = plan.info.images[i];
        const auto mappingOffset = static_cast<std::uint32_t>(snapshot.flattenedSrt.size());
        snapshot.flattenedSrt.push_back(static_cast<std::uint32_t>(candidates.size()));
        for (const auto& candidate : candidates) {
            const DecodedImage decodedCandidate = decodeImageDescriptor(candidate, image);
            if (decodedCandidate.reshaped) {
                reportReshapedImage(rawImageType(candidate), image.dimension);
            }
            ResourceSpecialization::Image entry;
            entry.numericClass = decodedCandidate.numericClass;
            entry.dimension = decodedCandidate.dimension;
            entry.mipCount = decodedCandidate.mipCount;
            entry.conversionFormat = decodedCandidate.conversionFormat;
            entry.shaderSwizzle = decodedCandidate.shaderSwizzle;
            entry.indirectRoot = i;
            entry.indirectMappingOffset = 0u;
            entry.indirectSearchIterations = 0u;
            entry.cube = decodedCandidate.cube;
            entry.fmask = decodedCandidate.fmask;
            std::uint32_t targetIndex = NoRemap;
            for (std::uint32_t search = i; search < result.images.size(); search++) {
                if (sameImageShape(result.images[search], entry)) {
                    targetIndex = search;
                    break;
                }
            }
            if (targetIndex == NoRemap) {
                result.images.push_back(entry);
                targetIndex = static_cast<std::uint32_t>(result.images.size() - 1);
            }
            snapshot.flattenedSrt.push_back(targetIndex);
        }
        result.images[i].indirectRoot = i;
        result.images[i].indirectMappingOffset = mappingOffset;
        result.images[i].indirectSearchIterations = static_cast<std::uint32_t>(candidates.size());
    }

    result.boundDescriptors.clear();
    result.boundDescriptors.reserve(result.buffers.size() + result.images.size());
    for (std::uint32_t index = 0; index < result.buffers.size(); index++) {
        result.boundDescriptors.push_back(index);
    }
    for (std::uint32_t index = 0; index < result.images.size(); index++) {
        result.boundDescriptors.push_back(index);
    }
    specialization = std::move(result);
}

}

void ResourceMaterializer::Apply(IrProgram& program, const ResourceSpecialization& specialization) const {
    IrResourcePlan& resources = program.Resources();
    if (!resources.resourceTrackingComplete) {
        throw std::runtime_error("ResourceMaterializer::Apply requires a completed resource plan");
    }
    if (resources.info.buffers.size() != specialization.buffers.size()) {
        throw std::runtime_error("ResourceMaterializer::Apply buffer count mismatch");
    }
    if (resources.info.images.size() > specialization.images.size()) {
        throw std::runtime_error("ResourceMaterializer::Apply image count mismatch");
    }

    auto buffers = resources.info.buffers;
    for (std::uint32_t i = 0; i < buffers.size(); i++) {
        buffers[i].packedStride = specialization.buffers[i].packedStride;
        buffers[i].descriptorFormat = specialization.buffers[i].descriptorFormat;
        buffers[i].descriptorSwizzle = specialization.buffers[i].descriptorSwizzle;
    }

    auto images = resources.info.images;
    images.reserve(specialization.images.size());
    for (std::uint32_t index = 0; index < specialization.images.size(); index++) {
        const auto& source = specialization.images[index];
        if (index >= images.size()) {
            if (source.indirectRoot >= resources.info.images.size()) {
                throw std::runtime_error("ResourceMaterializer::Apply indirect image root is out of range");
            }
            images.push_back(resources.info.images[source.indirectRoot]);
        }
        auto& image = images[index];
        image.numericClass = source.numericClass;
        image.dimension = source.dimension;
        image.mipCount = source.mipCount;
        image.conversionFormat = source.conversionFormat;
        image.shaderSwizzle = source.shaderSwizzle;
        image.indirectRoot = source.indirectRoot;
        image.indirectMappingOffset = source.indirectMappingOffset;
        image.indirectSearchIterations = source.indirectSearchIterations;
        image.cube = source.cube;
        image.indirectResources.clear();
    }
    for (std::uint32_t index = 0; index < images.size(); index++) {
        const auto root = images[index].indirectRoot;
        if (root != ImageResource::NoIndirectImage) {
            if (root >= images.size()) {
                throw std::runtime_error("ResourceMaterializer::Apply indirect image root is out of range");
            }
            images[root].indirectResources.push_back(index);
        }
    }

    std::vector<std::uint32_t> pointSampler(resources.info.samplers.size(), NoRemap);
    std::uint32_t samplerCount = static_cast<std::uint32_t>(resources.info.samplers.size());
    std::vector<std::uint8_t> samplerUsage(resources.info.samplers.size(), 0u);
    for (const auto& pair : resources.info.sampledPairs) {
        if (pair.image >= images.size() || pair.sampler >= resources.info.samplers.size()) {
            throw std::runtime_error("ResourceMaterializer::Apply sampled pair is out of range");
        }
        samplerUsage[pair.sampler] |= requiresPointSampler(specialization.images[pair.image]) ? 2u : 1u;
    }
    for (std::uint32_t index = 0; index < resources.info.samplers.size(); index++) {
        if ((samplerUsage[index] & 2u) == 0u) {
            continue;
        }
        if ((samplerUsage[index] & 1u) == 0u) {
            pointSampler[index] = index;
        } else {
            if (samplerCount >= ShaderInfo::MaxSamplers) {
                throw std::runtime_error("ResourceMaterializer::Apply exceeds the sampler resource limit");
            }
            pointSampler[index] = samplerCount;
            samplerCount++;
        }
    }
    auto samplers = resources.info.samplers;
    auto sampledPairs = resources.info.sampledPairs;
    samplers.reserve(samplerCount);
    for (std::uint32_t index = 0; index < resources.info.samplers.size(); index++) {
        const auto target = pointSampler[index];
        if (target == NoRemap) {
            continue;
        }
        if (target == index) {
            samplers[index].forcePointFiltering = true;
        } else {
            if (target != samplers.size()) {
                throw std::runtime_error("ResourceMaterializer::Apply sampler plan is inconsistent");
            }
            auto sampler = samplers[index];
            sampler.forcePointFiltering = true;
            samplers.push_back(sampler);
        }
    }
    for (auto& pair : sampledPairs) {
        if (requiresPointSampler(specialization.images[pair.image])) {
            if (pointSampler[pair.sampler] == NoRemap) {
                throw std::runtime_error("ResourceMaterializer::Apply missing point sampler for pair");
            }
            pair.sampler = pointSampler[pair.sampler];
        }
        samplers[pair.sampler].depthCompare = samplers[pair.sampler].depthCompare || images[pair.image].depthCompare;
    }

    auto memoryInfo = resources.memoryInfo;
    std::vector<std::uint32_t> imageRemap(specialization.images.size());
    std::uint32_t remapCount = 0;
    for (std::uint32_t i = 0; i < specialization.images.size(); i++) {
        imageRemap[i] = specialization.images[i].fmask ? NoRemap : remapCount;
        if (!specialization.images[i].fmask) {
            remapCount++;
        }
    }

    IrBuilder builder(program);
    for (auto& block : program.Blocks()) {
        builder.SetInsertionPoint(*block);
        for (auto* value : block->Instructions()) {
            const auto imageOpcode = ImageOpcodeInfoOf(value->Opcode());
            if (imageOpcode.access == ImageAccess::None) {
                continue;
            }
            const auto flags = value->Flags<MemoryFlags>();
            if (flags.index >= memoryInfo.size()) {
                throw std::runtime_error("ResourceMaterializer::Apply memory info index is out of range");
            }
            auto& memory = memoryInfo[flags.index];
            if (memory.resource >= images.size()) {
                throw std::runtime_error("ResourceMaterializer::Apply memory resource index is out of range");
            }
            if (specialization.images[memory.resource].fmask) {
                if (value->Opcode() != IrOpcode::ImageRead || memory.dataBits != 32u) {
                    throw std::runtime_error("ResourceMaterializer::Apply found an unsupported FMASK access");
                }
                constexpr std::array<std::uint32_t, 2> fragmentIndices{0x76543210u, 0xfedcba98u};
                std::array<IrValue*, 2> fragments{};
                for (std::uint32_t component = 0; component < fragments.size(); component++) {
                    IrValue& selected = program.CreateValue(IrOpcode::SelectU32, IrType::U32);
                    selected.AddArgument(value->Argument(2));
                    selected.AddArgument(&builder.Constant(fragmentIndices[component]));
                    selected.AddArgument(&builder.Constant(0u));
                    block->InsertInstructionBefore(value, &selected);
                    fragments[component] = &selected;
                }
                IrValue& result = program.CreateValue(IrOpcode::CompositeConstructU32x4, IrType::U32x4);
                result.AddArgument(fragments[0]);
                result.AddArgument(fragments[1]);
                result.AddArgument(&builder.Constant(0u));
                result.AddArgument(&builder.Constant(0u));
                block->InsertInstructionBefore(value, &result);
                value->ReplaceAllUsesWith(&result);
                continue;
            }
            if (imageOpcode.needsSampler && requiresPointSampler(specialization.images[memory.resource]) && memory.sampler < resources.info.samplers.size()) {
                if (pointSampler[memory.sampler] == NoRemap) {
                    throw std::runtime_error("ResourceMaterializer::Apply missing point sampler for image access");
                }
                memory.sampler = pointSampler[memory.sampler];
            }
        }
    }

    for (auto& memory : memoryInfo) {
        if (memory.kind == ResourceKind::Image && !memory.planningOnly) {
            if (memory.resource >= imageRemap.size() || imageRemap[memory.resource] == NoRemap) {
                throw std::runtime_error("ResourceMaterializer::Apply cannot remap an image memory reference");
            }
            memory.resource = imageRemap[memory.resource];
        }
    }
    for (auto& buffer : buffers) {
        if (buffer.imageAlias != BufferResource::NoImageAlias) {
            if (buffer.imageAlias >= imageRemap.size() || imageRemap[buffer.imageAlias] == NoRemap) {
                throw std::runtime_error("ResourceMaterializer::Apply cannot remap a buffer image alias");
            }
            buffer.imageAlias = imageRemap[buffer.imageAlias];
        }
    }
    for (auto& pair : sampledPairs) {
        if (pair.image >= imageRemap.size() || imageRemap[pair.image] == NoRemap) {
            throw std::runtime_error("ResourceMaterializer::Apply cannot remap a sampled pair image");
        }
        pair.image = imageRemap[pair.image];
    }
    for (auto& image : images) {
        if (image.indirectRoot != ImageResource::NoIndirectImage) {
            if (image.indirectRoot >= imageRemap.size() || imageRemap[image.indirectRoot] == NoRemap) {
                throw std::runtime_error("ResourceMaterializer::Apply cannot remap an indirect image root");
            }
            image.indirectRoot = imageRemap[image.indirectRoot];
        }
        for (auto& resource : image.indirectResources) {
            if (resource >= imageRemap.size() || imageRemap[resource] == NoRemap) {
                throw std::runtime_error("ResourceMaterializer::Apply cannot remap an indirect image resource");
            }
            resource = imageRemap[resource];
        }
    }
    if (images.size() != imageRemap.size()) {
        throw std::runtime_error("ResourceMaterializer::Apply image remap size mismatch");
    }
    for (std::uint32_t index = 0; index < images.size(); index++) {
        if (imageRemap[index] != NoRemap && imageRemap[index] != index) {
            images[imageRemap[index]] = std::move(images[index]);
        }
    }
    images.resize(remapCount);

    resources.info.buffers = std::move(buffers);
    resources.info.images = std::move(images);
    resources.info.samplers = std::move(samplers);
    resources.info.sampledPairs = std::move(sampledPairs);
    resources.memoryInfo = std::move(memoryInfo);
}

IrResourcePlan ResourceMaterializer::ExtractPlan(const IrProgram& program) const {
    const IrResourcePlan& source = program.Resources();
    if (!source.resourceTrackingComplete || !source.srtPlanComplete) {
        throw std::runtime_error("ResourceMaterializer::ExtractPlan requires a completed resource and SRT plan");
    }
    IrResourcePlan plan;
    plan.stage = source.stage;
    plan.shaderHash = source.shaderHash;
    plan.userDataBase = source.userDataBase;
    plan.userDataCount = source.userDataCount;
    plan.memoryInfo = source.memoryInfo;
    plan.descriptorSources = source.descriptorSources;
    plan.controlFlow = source.controlFlow;
    plan.srtReads = source.srtReads;
    plan.selectorBounds = source.selectorBounds;
    plan.cleanFlatSlots = source.cleanFlatSlots;
    plan.requiresSpecializationMemory = source.requiresSpecializationMemory;
    plan.srtPlanComplete = source.srtPlanComplete;
    plan.resourceTrackingComplete = source.resourceTrackingComplete;
    plan.info = source.info;
    plan.uniformFill = source.uniformFill;
    const auto addSource = [&plan](std::uint32_t index) {
        if (index >= plan.descriptorSources.size()) {
            throw std::runtime_error("ResourceMaterializer::ExtractPlan resource references an unknown descriptor source");
        }
        plan.materializationSources.push_back(index);
    };
    for (const auto& buffer : plan.info.buffers) addSource(buffer.source);
    for (const auto& image : plan.info.images) {
        if (image.source >= plan.descriptorSources.size()) {
            throw std::runtime_error("ResourceMaterializer::ExtractPlan image references an unknown descriptor source");
        }
        if (plan.descriptorSources[image.source].indirectImage.has_value() || plan.descriptorSources[image.source].tableImage.has_value()) {
            plan.requiresSpecializationMemory = true;
        } else {
            addSource(image.source);
        }
    }
    for (const auto& sampler : plan.info.samplers) addSource(sampler.source);
    return plan;
}

void ResourceMaterializer::Materialize(const IrResourcePlan& program, const SrtRuntime& runtime, ResourceSnapshot& snapshot, ResourceSpecialization& specialization) const {
    const IrResourcePlan& plan = program;
    if (!plan.resourceTrackingComplete) {
        throw std::runtime_error("ResourceMaterializer::Materialize requires a completed resource plan");
    }
    if (plan.requiresSpecializationMemory && runtime.readMemory == nullptr) {
        throw std::runtime_error("ResourceMaterializer::Materialize requires runtime memory access for indirect images");
    }
    SrtWalker walker;
    ResourceSnapshot nextSnapshot;
    std::vector<std::vector<DescriptorValue>> indirectCandidates;
    materializeSnapshot(plan, runtime, walker, nextSnapshot, indirectCandidates);
    ResourceSpecialization nextSpecialization;
    buildResourceSpecialization(plan, nextSnapshot, indirectCandidates, nextSpecialization);
    snapshot = std::move(nextSnapshot);
    specialization = std::move(nextSpecialization);
}

bool ResourceSpecialization::Buffer::operator==(const Buffer& other) const {
    return packedStride == other.packedStride && descriptorFormat == other.descriptorFormat && descriptorSwizzle == other.descriptorSwizzle;
}

bool ResourceSpecialization::Image::operator==(const Image& other) const {
    return numericClass == other.numericClass && dimension == other.dimension && mipCount == other.mipCount && conversionFormat == other.conversionFormat && shaderSwizzle == other.shaderSwizzle && indirectRoot == other.indirectRoot && indirectMappingOffset == other.indirectMappingOffset && indirectSearchIterations == other.indirectSearchIterations && cube == other.cube && fmask == other.fmask;
}

bool ResourceSpecialization::operator==(const ResourceSpecialization& other) const {
    return buffers == other.buffers && images == other.images;
}

}

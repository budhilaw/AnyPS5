#include "BdaShader.hpp"
#include "BdaAbi.hpp"
#include "Recompiler.hpp"
#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include "tests/SyntheticPrograms.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace {

void defineBdaReads(ShaderRecompiler::SpirvEmitterState& state, bool singleCache, std::uint32_t slots) {
    if (singleCache) {
        ShaderRecompiler::DefineGetBdaPointer(state);
        return;
    }
    state.bdaSlotCount = slots;
    state.bdaReadWidths = 7u;
    ShaderRecompiler::DefineBdaFunctions(state);
}

std::uint32_t emitSingleCacheRead(ShaderRecompiler::SpirvValueEmitContext& ctx, const ShaderRecompiler::IrValue& instruction, std::uint32_t base, std::int64_t immediate, std::uint32_t bits) {
    using namespace ShaderRecompiler;
    auto& state = ctx.state;
    if (immediate != 0) base = AddBdaAddress(ctx, instruction, base, BdaConstant(state, immediate < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(immediate) : static_cast<std::uint64_t>(immediate)), immediate < 0);
    return EmitBdaRead(ctx, instruction, base, bits);
}

}

std::vector<std::uint32_t> MakeBdaTestShader(std::uint64_t address, std::uint32_t bits, std::int64_t offset, bool singleCache) {
    using namespace ShaderRecompiler;
    IrProgram program;
    program.Resources().stage = IrShaderStage::Compute;
    program.Info().usesDma = true;
    SpirvEmitterState state(program, {});
    EmitBaseHeader(state.module, program);
    const auto define = [&](std::uint32_t binding) {
        const auto variable = state.module.DefineGlobalVariable(TypeStorageBufferPointer(state), spv::StorageClassStorageBuffer);
        state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationDescriptorSet, 0u);
        state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBinding, binding);
        return variable;
    };
    state.bdaPagetableVariable = define(0);
    state.faultBufferVariable = define(1);
    const auto output = define(2);
    defineBdaReads(state, singleCache, 1u);
    const auto main = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), main, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.module.AllocateId());
    SpirvValueEmitContext ctx(state);
    auto& instruction = program.CreateValue(IrOpcode::LoadAddressU32, IrType::U32);
    MemoryFlags flags{};
    flags.pc = 0x1234;
    instruction.SetFlags(flags);
    const std::array<BdaLaneAddress, 1> lanes{{{BdaConstant(state, address), 0u, 0u}}};
    const auto value = singleCache ? emitSingleCacheRead(ctx, instruction, lanes[0].base, offset, bits) : EmitBdaGroupRead(state, BdaReadGroup{0u, bits, 0x1234u, {offset}}, lanes).front().front();
    state.module.AddFunction(spv::OpStore, BdaWord(state, output, ConstantU32(state, 0u)), value);
    state.module.AddFunction(spv::OpReturn);
    state.module.AddFunction(spv::OpFunctionEnd);
    state.module.AddExecutionMode(main, spv::ExecutionModeLocalSize, 1u, 1u, 1u);
    state.module.EmitEntryPoint(spv::ExecutionModelGLCompute, main, "main", {});
    return state.module.Finalize();
}

namespace {

using namespace AgcDriver::Graphics;
namespace Synthetic = ShaderRecompiler::SyntheticPrograms;

constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t PushConstantBytes = 128u;
constexpr std::uint32_t LaneGroupSize = 64u;
constexpr std::uint32_t LaneGroups = 2u;
constexpr std::uint32_t LaneInvocations = LaneGroupSize * LaneGroups;

struct StorageBinding {
    std::uint32_t binding = 0;
    std::vector<VkDescriptorBufferInfo> buffers;
};

class ComputeDispatch {
public:
    ComputeDispatch(const Context& context, std::span<const std::uint32_t> code, std::span<const StorageBinding> bindings, bool pushConstants) : context(context) {
        try {
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
            std::uint32_t descriptors = 0;
            for (const auto& binding : bindings) {
                layoutBindings.push_back({binding.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<std::uint32_t>(binding.buffers.size()), VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
                descriptors += static_cast<std::uint32_t>(binding.buffers.size());
            }
            VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setInfo.bindingCount = static_cast<std::uint32_t>(layoutBindings.size());
            setInfo.pBindings = layoutBindings.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
            const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, std::max(descriptors, 1u)};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &size;
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1, &setLayout};
            Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets");
            for (const auto& binding : bindings) {
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = set;
                write.dstBinding = binding.binding;
                write.descriptorCount = static_cast<std::uint32_t>(binding.buffers.size());
                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.pBufferInfo = binding.buffers.data();
                context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
            }
            const VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, PushConstantBytes};
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &setLayout;
            layoutInfo.pushConstantRangeCount = pushConstants ? 1u : 0u;
            layoutInfo.pPushConstantRanges = &range;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout");
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            moduleInfo.codeSize = code.size_bytes();
            moduleInfo.pCode = code.data();
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
            pipelineInfo.layout = layout;
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
        } catch (...) { release(); throw; }
    }

    ~ComputeDispatch() { release(); }

    void Run(std::uint32_t groups, std::span<const std::byte> pushConstants) {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        VkMemoryBarrier upload{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &upload, 0, nullptr, 0, nullptr);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
        if (!pushConstants.empty()) {
            context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, static_cast<std::uint32_t>(pushConstants.size()), pushConstants.data());
        }
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, groups, 1, 1);
        VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
        batch.SubmitAndWait();
    }

private:
    void release() noexcept {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (setLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }

    const Context& context;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

std::uint32_t measuredSubgroupSize(const Context& context) {
    using namespace ShaderRecompiler;
    IrProgram program;
    program.Resources().stage = IrShaderStage::Compute;
    SpirvEmitterState state(program, {});
    EmitBaseHeader(state.module, program);
    state.module.EmitCapability(spv::CapabilityGroupNonUniform);
    const auto output = state.module.DefineGlobalVariable(TypeStorageBufferPointer(state), spv::StorageClassStorageBuffer);
    state.module.AddAnnotation(spv::OpDecorate, output, spv::DecorationDescriptorSet, 0u);
    state.module.AddAnnotation(spv::OpDecorate, output, spv::DecorationBinding, 0u);
    const auto subgroupSize = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassInput, TypeU32(state)), spv::StorageClassInput);
    state.module.AddAnnotation(spv::OpDecorate, subgroupSize, spv::DecorationBuiltIn, spv::BuiltInSubgroupSize);
    const auto main = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), main, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.module.AllocateId());
    const auto size = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), size, subgroupSize);
    state.module.AddFunction(spv::OpStore, BdaWord(state, output, ConstantU32(state, 0u)), size);
    state.module.AddFunction(spv::OpReturn);
    state.module.AddFunction(spv::OpFunctionEnd);
    state.module.AddExecutionMode(main, spv::ExecutionModeLocalSize, 1u, 1u, 1u);
    state.module.EmitEntryPoint(spv::ExecutionModelGLCompute, main, "main", {subgroupSize});
    const auto code = state.module.Finalize();
    Buffer result(context, sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memset(result.Bytes().data(), 0, result.Bytes().size());
    const std::array<StorageBinding, 1> bindings{{{0u, {{result.Handle(), 0, result.Bytes().size()}}}}};
    ComputeDispatch dispatch(context, code, bindings, false);
    dispatch.Run(1u, {});
    std::uint32_t measured = 0;
    std::memcpy(&measured, result.Bytes().data(), sizeof(measured));
    return measured;
}

struct GuestRegion {
    std::uint64_t address = 0;
    std::vector<std::byte> bytes;
};

std::span<std::byte> guestBytes(std::vector<GuestRegion>& memory, std::uint64_t address, std::size_t size) {
    for (auto& region : memory) {
        if (address >= region.address && address + size <= region.address + region.bytes.size()) {
            return std::span<std::byte>(region.bytes).subspan(address - region.address, size);
        }
    }
    throw std::runtime_error("a guest range lies outside the test memory: address " + std::to_string(address) + " size " + std::to_string(size));
}

GuestRegion wordRegion(std::uint64_t address, std::size_t words, std::uint32_t (*value)(std::size_t)) {
    GuestRegion region{address, std::vector<std::byte>(words * sizeof(std::uint32_t))};
    for (std::size_t word = 0; word < words; ++word) {
        const auto bits = value(word);
        std::memcpy(region.bytes.data() + word * sizeof(bits), &bits, sizeof(bits));
    }
    return region;
}

std::uint32_t guestWord(const GuestRegion& region, std::uint64_t address) {
    std::uint32_t value = 0;
    std::memcpy(&value, region.bytes.data() + (address - region.address), sizeof(value));
    return value;
}

std::vector<GuestRegion> runRecompiled(const Context& context, const ShaderRecompiler::RecompileResult& result, std::vector<GuestRegion> memory, std::uint32_t groups, ShaderRecompiler::BdaAbi::Fault* fault = nullptr, std::span<const std::uint64_t> unmapped = {}) {
    using ShaderRecompiler::DescriptorRole;
    namespace Abi = ShaderRecompiler::BdaAbi;
    struct Mirror {
        Buffer* buffer;
        std::uint64_t address;
        std::size_t size;
    };
    std::vector<std::unique_ptr<Buffer>> owned;
    std::vector<Mirror> mirrors;
    std::vector<StorageBinding> bindings;
    Buffer* faultBuffer = nullptr;
    const auto makeBuffer = [&](std::size_t size, VkBufferUsageFlags usage = 0u) {
        return owned.emplace_back(std::make_unique<Buffer>(context, std::max<std::size_t>(size, sizeof(std::uint32_t)), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | usage)).get();
    };
    const auto makeTable = [&] {
        std::vector<const GuestRegion*> mapped;
        for (const auto& region : memory) {
            if (std::find(unmapped.begin(), unmapped.end(), region.address) == unmapped.end()) mapped.push_back(&region);
        }
        std::sort(mapped.begin(), mapped.end(), [](const GuestRegion* left, const GuestRegion* right) { return left->address < right->address; });
        std::vector<Abi::Range> entries;
        for (const auto* region : mapped) {
            auto* buffer = makeBuffer(region->bytes.size(), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            std::memcpy(buffer->Bytes().data(), region->bytes.data(), region->bytes.size());
            entries.push_back({region->address, region->address + region->bytes.size(), buffer->DeviceAddress(), Abi::Read, 0});
        }
        auto* table = makeBuffer(sizeof(Abi::Header) + entries.size() * sizeof(Abi::Range));
        const Abi::Header header{Abi::Version, static_cast<std::uint32_t>(entries.size()), sizeof(Abi::Range), 0};
        std::memcpy(table->Bytes().data(), &header, sizeof(header));
        if (!entries.empty()) std::memcpy(table->Bytes().data() + sizeof(header), entries.data(), entries.size() * sizeof(Abi::Range));
        Require(Abi::IsValidTable(table->Bytes().first(sizeof(Abi::Header) + entries.size() * sizeof(Abi::Range))), "the recompiled program's BDA table is malformed");
        return VkDescriptorBufferInfo{table->Handle(), 0, sizeof(Abi::Header) + entries.size() * sizeof(Abi::Range)};
    };
    for (const auto& binding : result.bindings) {
        Require(binding.kind == ShaderRecompiler::DescriptorKind::StorageBuffer, "the synthetic program binds a descriptor other than a storage buffer");
        StorageBinding storage{binding.binding, {}};
        if (binding.role == DescriptorRole::GuestBuffers) {
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                const auto* fields = binding.guestDescriptor.data() + element * 4u;
                const std::uint64_t base = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
                const std::uint32_t stride = (fields[1] >> 16u) & 0x3fffu;
                const std::uint64_t bytes = stride == 0u ? fields[2] : static_cast<std::uint64_t>(stride) * fields[2];
                const std::uint64_t residue = base & 0xffu;
                const auto range = static_cast<std::size_t>(bytes + residue);
                auto* buffer = makeBuffer(range);
                const auto source = guestBytes(memory, base - residue, range);
                std::memcpy(buffer->Bytes().data(), source.data(), range);
                mirrors.push_back({buffer, base - residue, range});
                storage.buffers.push_back({buffer->Handle(), 0, range});
            }
        } else if (binding.role == DescriptorRole::ShaderData || binding.role == DescriptorRole::FlattenedSrt) {
            const auto size = binding.guestDescriptor.size() * sizeof(std::uint32_t);
            auto* buffer = makeBuffer(size);
            std::memcpy(buffer->Bytes().data(), binding.guestDescriptor.data(), size);
            storage.buffers.push_back({buffer->Handle(), 0, std::max<std::size_t>(size, sizeof(std::uint32_t))});
        } else if (binding.role == DescriptorRole::BdaPagetable) {
            storage.buffers.push_back(makeTable());
        } else if (binding.role == DescriptorRole::FaultBuffer) {
            faultBuffer = makeBuffer(sizeof(Abi::Fault));
            std::memset(faultBuffer->Bytes().data(), 0, sizeof(Abi::Fault));
            storage.buffers.push_back({faultBuffer->Handle(), 0, sizeof(Abi::Fault)});
        } else {
            throw std::runtime_error("the synthetic program binds an unsupported descriptor role");
        }
        bindings.push_back(std::move(storage));
    }
    ComputeDispatch dispatch(context, result.spirv.Words(), bindings, !result.pushConstants.empty());
    dispatch.Run(groups, result.pushConstants);
    for (const auto& mirror : mirrors) {
        std::memcpy(guestBytes(memory, mirror.address, mirror.size).data(), mirror.buffer->Bytes().data(), mirror.size);
    }
    if (fault != nullptr) {
        *fault = {};
        if (faultBuffer != nullptr) std::memcpy(fault, faultBuffer->Bytes().data(), sizeof(Abi::Fault));
    }
    return memory;
}

std::uint32_t inputPattern(std::size_t word) {
    return static_cast<std::uint32_t>(word) * 2654435761u + 0x1234u;
}

std::uint32_t sentinelPattern(std::size_t) {
    return Sentinel;
}

void perThreadProgramsMatchAcrossLaneModes(const Context& context) {
    struct Shape {
        std::array<std::uint32_t, 3> threads;
        std::uint32_t groups;
        std::uint32_t count;
        std::uint32_t threshold;
    };
    constexpr std::uint64_t inputRegion = 0x40000u;
    constexpr std::uint64_t outputRegion = 0x60000u;
    constexpr std::uint64_t input = inputRegion + 0x10u;
    constexpr std::uint64_t output = outputRegion + 0x20u;
    const std::array<Shape, 5> shapes{{{{8u, 8u, 1u}, 5u, 300u, 100u}, {{48u, 1u, 1u}, 4u, 170u, 90u}, {{16u, 16u, 1u}, 2u, 512u, 300u}, {{32u, 2u, 3u}, 3u, 500u, 250u}, {{1024u, 1u, 1u}, 2u, 2000u, 1500u}}};
    for (const auto& shape : shapes) {
        const auto threads = shape.threads[0] * shape.threads[1] * shape.threads[2];
        const auto total = threads * shape.groups;
        const auto program = Synthetic::PerThreadProgram(shape.threads, shape.count, shape.threshold, input, output, total);
        const std::string label = "the per-thread program with " + std::to_string(shape.threads[0]) + "x" + std::to_string(shape.threads[1]) + "x" + std::to_string(shape.threads[2]) + " threads";
        std::array<std::vector<std::byte>, 2> outputs;
        for (const bool dualLane : {false, true}) {
            const auto result = ShaderRecompiler::Recompile(program.Request(64u, 32u, dualLane));
            Require(result.lanesPerInvocation == (dualLane ? 2u : 1u), label + " compiled with the wrong number of guest lanes per invocation");
            const auto memory = runRecompiled(context, result, {wordRegion(inputRegion, total + 64u, inputPattern), wordRegion(outputRegion, total + 64u, sentinelPattern)}, shape.groups);
            for (std::uint32_t index = 0; index < total; ++index) {
                const auto expected = index < shape.count ? Synthetic::PerThreadExpected(index, guestWord(memory[0], input + index * 4u), shape.threshold) : Sentinel;
                const auto actual = guestWord(memory[1], output + index * 4u);
                Require(actual == expected, label + (dualLane ? " in dual-lane" : " in single-lane") + " mode wrote " + std::to_string(actual) + " instead of " + std::to_string(expected) + " for thread " + std::to_string(index));
            }
            outputs[dualLane ? 1u : 0u] = memory[1].bytes;
        }
        Require(outputs[0] == outputs[1], label + " wrote different memory in single-lane and dual-lane mode");
    }
}

void execReadAsScalarSeesTheWholeWave(const Context& context) {
    constexpr std::uint64_t input = 0x40000u;
    constexpr std::uint64_t output = 0x60000u;
    constexpr std::uint32_t groups = 3u;
    constexpr std::uint32_t total = 64u * groups;
    const auto program = Synthetic::ExecMaskProgram(input, output, total);
    const auto result = ShaderRecompiler::Recompile(program.Request(64u, 32u, false));
    Require(result.lanesPerInvocation == 2u, "a program that reads EXEC as a scalar was compiled with one guest lane per invocation");
    const auto memory = runRecompiled(context, result, {wordRegion(input, total, inputPattern), wordRegion(output, total, sentinelPattern)}, groups);
    for (std::uint32_t index = 0; index < total; ++index) {
        const auto actual = guestWord(memory[1], output + index * 4u);
        Require(actual == inputPattern(index) + 0xffffffffu, "a program that reads the upper EXEC dword wrote " + std::to_string(actual) + " for thread " + std::to_string(index));
    }
}

void vccBranchesAreWaveWide(const Context& context) {
    struct Shape {
        std::array<std::uint32_t, 3> threads;
        std::uint32_t groups;
        std::uint32_t threshold;
    };
    constexpr std::uint64_t input = 0x40000u;
    constexpr std::uint64_t output = 0x60000u;
    const std::array<Shape, 3> shapes{{{{64u, 1u, 1u}, 4u, 100u}, {{16u, 16u, 1u}, 2u, 300u}, {{48u, 1u, 1u}, 4u, 70u}}};
    for (const auto& shape : shapes) {
        const auto threads = shape.threads[0] * shape.threads[1] * shape.threads[2];
        const auto total = threads * shape.groups;
        const auto program = Synthetic::VccBranchProgram(shape.threads, shape.threshold, input, output, total);
        const std::string label = "the VCC branch program with " + std::to_string(shape.threads[0]) + "x" + std::to_string(shape.threads[1]) + "x" + std::to_string(shape.threads[2]) + " threads";
        for (const bool dualLane : {false, true}) {
            const auto result = ShaderRecompiler::Recompile(program.Request(64u, 32u, dualLane));
            Require(result.lanesPerInvocation == 2u, label + " was compiled with one guest lane per invocation");
            const auto memory = runRecompiled(context, result, {wordRegion(input, total, inputPattern), wordRegion(output, total, sentinelPattern)}, shape.groups);
            for (std::uint32_t index = 0; index < total; ++index) {
                const auto expected = Synthetic::VccBranchExpected(index, inputPattern(index), shape.threshold, threads);
                const auto actual = guestWord(memory[1], output + index * 4u);
                Require(actual == expected, label + " wrote " + std::to_string(actual) + " instead of " + std::to_string(expected) + " for thread " + std::to_string(index) + ", so its s_cbranch_vccz did not test the VCC of the whole wave");
            }
        }
    }
}

void wideLoadsKeepPerDwordBounds(const Context& context) {
    constexpr std::uint64_t inputRegion = 0x80000u;
    constexpr std::uint64_t output = 0x90000u;
    constexpr std::uint32_t threads = 64u;
    constexpr std::uint32_t inputBytes = 200u;
    for (const std::uint32_t dwords : {2u, 3u, 4u}) {
        for (const std::uint32_t residue : {0u, 4u, 8u, 12u}) {
            for (const std::uint32_t offset : {0u, 4u, 12u}) {
                for (const bool dualLane : {false, true}) {
                    const auto program = Synthetic::WideLoadProgram(dwords, {threads, 1u, 1u}, 4u, offset, inputRegion + residue, inputBytes, output, threads * 16u);
                    const auto result = ShaderRecompiler::Recompile(program.Request(64u, 32u, dualLane));
                    const auto memory = runRecompiled(context, result, {wordRegion(inputRegion, 128u, inputPattern), wordRegion(output, threads * 4u, sentinelPattern)}, 1u);
                    const std::string label = "buffer_load_dwordx" + std::to_string(dwords) + " at residue " + std::to_string(residue) + " offset " + std::to_string(offset) + (dualLane ? " in dual-lane mode" : " in single-lane mode");
                    for (std::uint32_t thread = 0; thread < threads; ++thread) {
                        for (std::uint32_t component = 0; component < 4u; ++component) {
                            std::uint32_t expected = Sentinel;
                            if (component < dwords) {
                                const std::uint32_t word = (thread * 4u + offset + component * 4u) / 4u + residue / 4u;
                                expected = word < (inputBytes + residue) / 4u ? inputPattern(word) : 0u;
                            }
                            const auto actual = guestWord(memory[1], output + (thread * 4u + component) * 4u);
                            Require(actual == expected, label + " read " + std::to_string(actual) + " instead of " + std::to_string(expected) + " for thread " + std::to_string(thread) + " dword " + std::to_string(component));
                        }
                    }
                }
            }
        }
    }
}

struct BdaTestRead {
    std::uint32_t slot = 0;
    std::vector<std::int64_t> immediates;
};

struct BdaTestShape {
    std::uint32_t lanes = 1u;
    std::uint32_t slots = 1u;
    std::uint32_t localSize = LaneGroupSize;
    bool countMisses = false;
};

std::size_t bdaTestValues(std::span<const BdaTestRead> reads, const BdaTestShape& shape) {
    std::size_t values = 0;
    for (const auto& read : reads) values += read.immediates.size() * shape.lanes;
    return values;
}

std::vector<std::uint32_t> makeBdaReadsShader(bool singleCache, const BdaTestShape& shape, std::span<const BdaTestRead> reads) {
    using namespace ShaderRecompiler;
    IrProgram program;
    program.Resources().stage = IrShaderStage::Compute;
    program.Info().usesDma = true;
    SpirvEmitterState state(program, {});
    EmitBaseHeader(state.module, program);
    const auto define = [&](std::uint32_t binding) {
        const auto variable = state.module.DefineGlobalVariable(TypeStorageBufferPointer(state), spv::StorageClassStorageBuffer);
        state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationDescriptorSet, 0u);
        state.module.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBinding, binding);
        return variable;
    };
    state.bdaPagetableVariable = define(0);
    state.faultBufferVariable = define(1);
    const auto output = define(2);
    const auto addresses = define(3);
    if (shape.countMisses) state.bdaMissCounterVariable = define(4);
    const auto invocation = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassInput, TypeU32Vector(state, 3u)), spv::StorageClassInput);
    state.module.AddAnnotation(spv::OpDecorate, invocation, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId);
    defineBdaReads(state, singleCache, shape.slots);
    const auto main = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), main, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.module.AllocateId());
    SpirvValueEmitContext ctx(state);
    auto& instruction = program.CreateValue(IrOpcode::LoadAddressU32, IrType::U32);
    MemoryFlags flags{};
    flags.pc = 0x1234;
    instruction.SetFlags(flags);
    const auto invocationId = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32Vector(state, 3u), invocationId, invocation);
    const auto index = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeU32(state), index, invocationId, 0u);
    const auto address = [&](std::uint32_t lane, std::uint32_t slot) {
        const auto entry = Binary(state, spv::OpIAdd, TypeU32(state), Binary(state, spv::OpIMul, TypeU32(state), index, ConstantU32(state, shape.lanes * shape.slots)), ConstantU32(state, lane * shape.slots + slot));
        const auto word = [&](std::uint32_t half) {
            const auto value = state.module.AllocateId();
            state.module.AddFunction(spv::OpLoad, TypeU32(state), value, BdaWord(state, addresses, Binary(state, spv::OpIAdd, TypeU32(state), Binary(state, spv::OpIMul, TypeU32(state), entry, ConstantU32(state, 2u)), ConstantU32(state, half))));
            return Unary(state, spv::OpUConvert, TypeScalarU64(state), value);
        };
        return Binary(state, spv::OpBitwiseOr, TypeScalarU64(state), word(0u), Binary(state, spv::OpShiftLeftLogical, TypeScalarU64(state), word(1u), BdaConstant(state, 32u)));
    };
    std::vector<std::uint32_t> values;
    for (const auto& read : reads) {
        if (singleCache) {
            for (const auto immediate : read.immediates) {
                for (std::uint32_t lane = 0; lane < shape.lanes; ++lane) values.push_back(emitSingleCacheRead(ctx, instruction, address(lane, read.slot), immediate, 32u));
            }
            continue;
        }
        std::vector<BdaLaneAddress> lanes;
        for (std::uint32_t lane = 0; lane < shape.lanes; ++lane) lanes.push_back({address(lane, read.slot), 0u, 0u});
        const auto loaded = EmitBdaGroupRead(state, BdaReadGroup{read.slot, 32u, 0x1234u, read.immediates}, lanes);
        for (std::size_t member = 0; member < read.immediates.size(); ++member) {
            for (std::uint32_t lane = 0; lane < shape.lanes; ++lane) values.push_back(loaded[lane][member]);
        }
    }
    const auto first = Binary(state, spv::OpIMul, TypeU32(state), index, ConstantU32(state, static_cast<std::uint32_t>(values.size())));
    for (std::size_t value = 0; value < values.size(); ++value) {
        state.module.AddFunction(spv::OpStore, BdaWord(state, output, Binary(state, spv::OpIAdd, TypeU32(state), first, ConstantU32(state, static_cast<std::uint32_t>(value)))), values[value]);
    }
    state.module.AddFunction(spv::OpReturn);
    state.module.AddFunction(spv::OpFunctionEnd);
    state.module.AddExecutionMode(main, spv::ExecutionModeLocalSize, shape.localSize, 1u, 1u);
    state.module.EmitEntryPoint(spv::ExecutionModelGLCompute, main, "main", {invocation});
    return state.module.Finalize();
}

struct GuestBda {
    std::uint64_t address;
    std::vector<std::byte> bytes;
    std::unique_ptr<Buffer> buffer;
    std::uint32_t permissions = ShaderRecompiler::BdaAbi::Read;
};

void addGuestBda(const Context& context, std::vector<GuestBda>& ranges, std::uint64_t address, std::size_t size, std::uint32_t seed, std::uint32_t permissions = ShaderRecompiler::BdaAbi::Read) {
    GuestBda range{address, std::vector<std::byte>(size), std::make_unique<Buffer>(context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT), permissions};
    for (std::size_t byte = 0; byte < size; ++byte) range.bytes[byte] = static_cast<std::byte>(seed + byte * 7u);
    std::memcpy(range.buffer->Bytes().data(), range.bytes.data(), size);
    ranges.push_back(std::move(range));
}

std::unique_ptr<Buffer> makeGuestBdaTable(const Context& context, const std::vector<GuestBda>& ranges) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    std::vector<Abi::Range> entries;
    for (const auto& range : ranges) entries.push_back({range.address, range.address + range.bytes.size(), range.buffer->DeviceAddress(), range.permissions, 0});
    std::sort(entries.begin(), entries.end(), [](const Abi::Range& left, const Abi::Range& right) { return left.begin < right.begin; });
    auto table = std::make_unique<Buffer>(context, sizeof(Abi::Header) + entries.size() * sizeof(Abi::Range), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const Abi::Header header{Abi::Version, static_cast<std::uint32_t>(entries.size()), sizeof(Abi::Range), 0};
    std::memcpy(table->Bytes().data(), &header, sizeof(header));
    std::memcpy(table->Bytes().data() + sizeof(header), entries.data(), entries.size() * sizeof(Abi::Range));
    Require(Abi::IsValidTable(table->Bytes()), "the BDA test table is malformed");
    return table;
}

std::optional<std::uint32_t> readGuestDword(const std::vector<GuestBda>& ranges, std::uint64_t address) {
    std::uint32_t value = 0;
    for (std::uint32_t byte = 0; byte < 4u; ++byte) {
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const GuestBda& range) { return address + byte >= range.address && address + byte < range.address + range.bytes.size(); });
        if (found == ranges.end() || (found->permissions & ShaderRecompiler::BdaAbi::Read) == 0u) {
            return std::nullopt;
        }
        value |= static_cast<std::uint32_t>(found->bytes[address + byte - found->address]) << (byte * 8u);
    }
    return value;
}

struct BdaReadsResult {
    std::vector<std::uint32_t> output;
    ShaderRecompiler::BdaAbi::Fault fault{};
    std::uint32_t misses = 0;
};

BdaReadsResult runBdaReads(const Context& context, std::span<const std::uint32_t> shader, Buffer& table, std::span<const std::uint64_t> addresses, std::size_t outputWords, std::uint32_t groups, bool countMisses) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    Buffer fault(context, sizeof(Abi::Fault), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Buffer output(context, outputWords * sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Buffer addressBuffer(context, addresses.size_bytes(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Buffer counter(context, sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memset(fault.Bytes().data(), 0, fault.Bytes().size());
    std::memset(counter.Bytes().data(), 0, counter.Bytes().size());
    std::memcpy(addressBuffer.Bytes().data(), addresses.data(), addresses.size_bytes());
    for (std::size_t word = 0; word < outputWords; ++word) std::memcpy(output.Bytes().data() + word * sizeof(Sentinel), &Sentinel, sizeof(Sentinel));
    std::vector<StorageBinding> bindings{
        {0u, {{table.Handle(), 0, table.Bytes().size()}}},
        {1u, {{fault.Handle(), 0, fault.Bytes().size()}}},
        {2u, {{output.Handle(), 0, output.Bytes().size()}}},
        {3u, {{addressBuffer.Handle(), 0, addressBuffer.Bytes().size()}}},
    };
    if (countMisses) bindings.push_back({4u, {{counter.Handle(), 0, counter.Bytes().size()}}});
    ComputeDispatch dispatch(context, shader, bindings, false);
    dispatch.Run(groups, {});
    BdaReadsResult result;
    result.output.resize(outputWords);
    std::memcpy(result.output.data(), output.Bytes().data(), outputWords * sizeof(std::uint32_t));
    std::memcpy(&result.fault, fault.Bytes().data(), sizeof(Abi::Fault));
    std::memcpy(&result.misses, counter.Bytes().data(), sizeof(result.misses));
    return result;
}

std::string describeFault(const ShaderRecompiler::BdaAbi::Fault& fault) {
    return "state " + std::to_string(static_cast<std::uint32_t>(fault.state)) + " reason " + std::to_string(static_cast<std::uint32_t>(fault.reason)) + " address " + std::to_string(fault.address) + " bytes " + std::to_string(fault.bytes) + " instruction " + std::to_string(fault.instruction);
}

void requireSameBdaRuns(const BdaReadsResult& single, const BdaReadsResult& slots, const std::string& label) {
    Require(single.output == slots.output, label + " read different values through the per-resource cache than through the single cache");
    Require(std::memcmp(&single.fault, &slots.fault, sizeof(single.fault)) == 0, label + " published fault " + describeFault(slots.fault) + " through the per-resource cache instead of " + describeFault(single.fault));
}

void tableValidationRejectsMalformedTables() {
    namespace Abi = ShaderRecompiler::BdaAbi;
    const auto table = [](std::uint32_t count, std::vector<Abi::Range> ranges) {
        std::vector<std::byte> bytes(sizeof(Abi::Header) + ranges.size() * sizeof(Abi::Range));
        const Abi::Header header{Abi::Version, count, sizeof(Abi::Range), 0};
        std::memcpy(bytes.data(), &header, sizeof(header));
        if (!ranges.empty()) std::memcpy(bytes.data() + sizeof(header), ranges.data(), ranges.size() * sizeof(Abi::Range));
        return bytes;
    };
    const Abi::Range first{0x1000u, 0x2000u, 0x10000u, Abi::Read, 0};
    const Abi::Range second{0x2000u, 0x2800u, 0x20000u, Abi::Read, 0};
    Require(Abi::IsValidTable(table(2u, {first, second})) && Abi::IsValidTable(table(0u, {})), "a well-formed BDA table was rejected");
    Require(!Abi::IsValidTable(table(3u, {first, second})), "a BDA table whose count exceeds its entries was accepted");
    Require(!Abi::IsValidTable(table(2u, {second, first})), "a BDA table with unsorted ranges was accepted");
    Require(!Abi::IsValidTable(table(2u, {first, Abi::Range{0x1800u, 0x2800u, 0x20000u, Abi::Read, 0}})), "a BDA table with overlapping ranges was accepted");
    Require(!Abi::IsValidTable(table(1u, {Abi::Range{0x1000u, 0x1000u, 0x10000u, Abi::Read, 0}})), "a BDA table with an empty range was accepted");
    Require(!Abi::IsValidTable(table(1u, {Abi::Range{0x1000u, 0x2000u, 0u, Abi::Read, 0}})), "a BDA table with a null device address was accepted");
    Require(!Abi::IsValidTable(table(1u, {Abi::Range{0x1000u, 0x2000u, 0x10000u, Abi::Read, 1u}})), "a BDA table with a reserved field set was accepted");
    Require(!Abi::IsValidTable(table(1u, {Abi::Range{0x1000u, 0x2000u, ~std::uint64_t{0} - 0x10u, Abi::Read, 0}})), "a BDA table whose device range overflows was accepted");
    auto wrongVersion = table(1u, {first});
    wrongVersion[0] = std::byte{0x7f};
    Require(!Abi::IsValidTable(wrongVersion), "a BDA table with another ABI version was accepted");
    auto truncated = table(1u, {first});
    truncated.pop_back();
    Require(!Abi::IsValidTable(truncated), "a truncated BDA table was accepted");
}

void bdaLaneReadsMatchGuestMemory(const Context& context) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    constexpr std::uint64_t guest = 0x7fff12340001ULL;
    constexpr std::uint64_t aligned = 0x7fff12350000ULL;
    constexpr std::uint64_t wide = 0x7fff12360000ULL;
    constexpr std::uint64_t unmapped = 0x7fff12370000ULL;
    std::vector<GuestBda> ranges;
    addGuestBda(context, ranges, guest, 3u, 0x11u);
    addGuestBda(context, ranges, guest + 3u, 2u, 0x44u);
    addGuestBda(context, ranges, aligned, 8u, 0x10u);
    addGuestBda(context, ranges, wide, 256u, 0x80u);
    const auto table = makeGuestBdaTable(context, ranges);
    const std::array<BdaTestRead, 1> reads{{{0u, {0}}}};
    const std::array<std::vector<std::uint32_t>, 2> shaders{makeBdaReadsShader(true, {}, reads), makeBdaReadsShader(false, {}, reads)};
    struct Pattern {
        const char* name;
        std::uint64_t (*address)(std::uint32_t invocation);
    };
    const std::array<Pattern, 8> patterns{{
        {"a uniform aligned address", [](std::uint32_t) { return aligned + 4u; }},
        {"a uniform unaligned address spanning two ranges", [](std::uint32_t) { return guest + 1u; }},
        {"lane addresses inside one range", [](std::uint32_t invocation) { return wide + 4u * (invocation % 63u); }},
        {"lane addresses across ranges", [](std::uint32_t invocation) { return invocation % 3u == 0u ? aligned : invocation % 3u == 1u ? wide + 8u * (invocation % 31u) : guest; }},
        {"one address per subgroup", [](std::uint32_t invocation) { const std::array<std::uint64_t, 4> perSubgroup{aligned, wide + 16u, guest + 1u, wide + 40u}; return perSubgroup[invocation / 32u]; }},
        {"one address per half of each subgroup", [](std::uint32_t invocation) { return invocation % 32u < 16u ? wide + 100u : aligned + 4u; }},
        {"a uniform unmapped address", [](std::uint32_t) { return unmapped; }},
        {"mapped even lanes and unmapped odd lanes", [](std::uint32_t invocation) { return invocation % 2u == 0u ? wide + 4u * (invocation % 60u) : unmapped + invocation; }},
    }};
    for (const auto& pattern : patterns) {
        std::vector<std::uint64_t> addresses(LaneInvocations);
        for (std::uint32_t invocation = 0; invocation < LaneInvocations; ++invocation) addresses[invocation] = pattern.address(invocation);
        std::array<BdaReadsResult, 2> results;
        for (std::size_t mode = 0; mode < shaders.size(); ++mode) {
            results[mode] = runBdaReads(context, shaders[mode], *table, addresses, LaneInvocations, LaneGroups, false);
            const std::string label = std::string("a BDA read of ") + pattern.name + (mode == 0u ? " through the single cache" : " through the per-resource cache");
            bool faulted = false;
            for (std::uint32_t invocation = 0; invocation < LaneInvocations; ++invocation) {
                const auto expected = readGuestDword(ranges, addresses[invocation]);
                faulted = faulted || !expected.has_value();
                Require(results[mode].output[invocation] == expected.value_or(Sentinel), label + " returned " + std::to_string(results[mode].output[invocation]) + " for invocation " + std::to_string(invocation));
            }
            const auto& report = results[mode].fault;
            Require(faulted ? report.state == Abi::FaultState::Ready && report.reason == Abi::FaultReason::Unmapped && report.instruction == 0x1234u : report.state == Abi::FaultState::Empty, label + " published the wrong fault " + describeFault(report));
        }
        Require(results[0].output == results[1].output, std::string("a BDA read of ") + pattern.name + " read different values through the per-resource cache than through the single cache");
    }
}

void shortBdaTablesReportInvalidTable(const Context& context) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    constexpr std::uint64_t address = 0x7fff12350000ULL;
    const std::array<std::uint32_t, 4> header{Abi::Version, 1u, sizeof(Abi::Range), 0u};
    const std::array<BdaTestRead, 1> reads{{{0u, {0}}}};
    std::vector<std::uint64_t> addresses(LaneInvocations);
    for (std::uint32_t invocation = 0; invocation < LaneInvocations; ++invocation) addresses[invocation] = address + invocation * 4u;
    for (const bool singleCache : {true, false}) {
        const auto shader = makeBdaReadsShader(singleCache, {}, reads);
        for (const std::uint32_t words : {1u, 3u, 4u}) {
            Buffer table(context, words * sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            std::memcpy(table.Bytes().data(), header.data(), words * sizeof(std::uint32_t));
            const auto result = runBdaReads(context, shader, table, addresses, LaneInvocations, LaneGroups, false);
            const std::string label = "a BDA read through a table of " + std::to_string(words) + " dword(s) that counts one range" + (singleCache ? " and a single cache" : " and per-resource caches");
            for (std::uint32_t invocation = 0; invocation < LaneInvocations; ++invocation) {
                Require(result.output[invocation] == Sentinel, label + " returned " + std::to_string(result.output[invocation]) + " for invocation " + std::to_string(invocation));
            }
            Require(result.fault.state == Abi::FaultState::Ready && result.fault.reason == Abi::FaultReason::InvalidTable && result.fault.instruction == 0x1234u && result.fault.bytes == 4u, label + " did not publish an InvalidTable fault");
        }
    }
}

void groupedReadsMatchSingleCache(const Context& context) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    constexpr std::uint64_t first = 0x7fff12400000ULL;
    constexpr std::uint64_t second = 0x7fff12410000ULL;
    constexpr std::uint64_t third = 0x7fff12420000ULL;
    constexpr std::uint64_t denied = 0x7fff12430000ULL;
    constexpr std::uint64_t odd = 0x7fff12440001ULL;
    std::vector<GuestBda> ranges;
    addGuestBda(context, ranges, first, 64u, 0x21u);
    addGuestBda(context, ranges, first + 64u, 64u, 0x52u);
    addGuestBda(context, ranges, second, 64u, 0x13u);
    addGuestBda(context, ranges, third, 64u, 0x77u);
    addGuestBda(context, ranges, denied, 64u, 0x35u, 0u);
    addGuestBda(context, ranges, odd, 5u, 0x91u);
    addGuestBda(context, ranges, odd + 5u, 35u, 0x0bu);
    const auto table = makeGuestBdaTable(context, ranges);
    const std::array<BdaTestRead, 1> reads{{{0u, {0, 4, 8, 12}}}};
    const BdaTestShape shape{2u, 1u, 1u, true};
    const std::array<std::vector<std::uint32_t>, 2> shaders{makeBdaReadsShader(true, shape, reads), makeBdaReadsShader(false, shape, reads)};
    struct Pattern {
        const char* name;
        std::uint64_t first;
        std::uint64_t second;
        Abi::FaultReason reason;
        std::uint64_t faultAddress;
        std::uint32_t faultBytes;
        std::uint32_t singleMisses;
        std::uint32_t slotMisses;
    };
    const std::array<Pattern, 8> patterns{{
        {"two lanes inside their ranges", first + 8u, second + 16u, Abi::FaultReason{}, 0u, 0u, 8u, 2u},
        {"a lane crossing into the adjacent range", first + 56u, second, Abi::FaultReason{}, 0u, 0u, 0u, 0u},
        {"unaligned lanes crossing ranges", odd + 2u, first + 2u, Abi::FaultReason{}, 0u, 0u, 0u, 0u},
        {"an aligned read crossing ranges", odd + 3u, first, Abi::FaultReason::Unmapped, odd + 3u, 4u, 0u, 0u},
        {"unmapped reads in both lanes", second + 56u, third + 60u, Abi::FaultReason::Unmapped, third + 64u, 4u, 0u, 0u},
        {"an unmapped third read in the first lane", second + 56u, first + 4u, Abi::FaultReason::Unmapped, second + 64u, 4u, 0u, 0u},
        {"a read without permission", denied + 4u, first, Abi::FaultReason::Permission, denied + 4u, 4u, 0u, 0u},
        {"an unaligned read crossing into unmapped memory", odd + 30u, first, Abi::FaultReason::Unmapped, odd + 40u, 1u, 0u, 0u},
    }};
    const auto values = bdaTestValues(reads, shape);
    for (const auto& pattern : patterns) {
        const std::array<std::uint64_t, 2> addresses{pattern.first, pattern.second};
        const auto single = runBdaReads(context, shaders[0], *table, addresses, values, 1u, true);
        const auto slots = runBdaReads(context, shaders[1], *table, addresses, values, 1u, true);
        const std::string label = std::string("a grouped BDA read of ") + pattern.name;
        requireSameBdaRuns(single, slots, label);
        if (pattern.reason == Abi::FaultReason{}) {
            Require(slots.fault.state == Abi::FaultState::Empty, label + " published fault " + describeFault(slots.fault));
            for (std::size_t member = 0; member < 4u; ++member) {
                for (std::size_t lane = 0; lane < 2u; ++lane) {
                    const auto expected = readGuestDword(ranges, addresses[lane] + member * 4u);
                    Require(expected.has_value() && slots.output[member * 2u + lane] == *expected, label + " returned " + std::to_string(slots.output[member * 2u + lane]) + " for lane " + std::to_string(lane) + " dword " + std::to_string(member));
                }
            }
        } else {
            Require(slots.fault.state == Abi::FaultState::Ready && slots.fault.reason == pattern.reason && slots.fault.address == pattern.faultAddress && slots.fault.bytes == pattern.faultBytes && slots.fault.instruction == 0x1234u && slots.fault.stage == static_cast<std::uint32_t>(ShaderRecompiler::IrShaderStage::Compute), label + " published fault " + describeFault(slots.fault));
            Require(std::all_of(slots.output.begin(), slots.output.end(), [](std::uint32_t value) { return value == Sentinel; }), label + " stored values after its fault");
        }
        if (pattern.slotMisses != 0u) {
            Require(single.misses == pattern.singleMisses && slots.misses == pattern.slotMisses, label + " searched the BDA table " + std::to_string(slots.misses) + " times through per-resource caches and " + std::to_string(single.misses) + " times through the single cache instead of " + std::to_string(pattern.slotMisses) + " and " + std::to_string(pattern.singleMisses));
        }
    }
}

void alternatingResourcesMissOncePerResource(const Context& context) {
    constexpr std::uint64_t left = 0x7fff12500000ULL;
    constexpr std::uint64_t right = 0x7fff12510000ULL;
    constexpr std::uint32_t invocations = LaneGroupSize;
    std::vector<GuestBda> ranges;
    addGuestBda(context, ranges, left, invocations * 16u, 0x3du);
    addGuestBda(context, ranges, right, invocations * 16u, 0x6au);
    const auto table = makeGuestBdaTable(context, ranges);
    std::vector<std::uint64_t> addresses;
    for (std::uint32_t invocation = 0; invocation < invocations; ++invocation) {
        addresses.push_back(left + invocation * 16u);
        addresses.push_back(right + invocation * 16u);
    }
    struct Case {
        const char* name;
        std::vector<BdaTestRead> reads;
        std::uint32_t singleMisses;
        std::uint32_t slotMisses;
    };
    const std::array<Case, 2> cases{{
        {"reads alternating between two resources", {{0u, {0}}, {1u, {0}}, {0u, {4}}, {1u, {4}}, {0u, {8}}, {1u, {8}}, {0u, {12}}, {1u, {12}}}, 8u, 2u},
        {"reads of one resource", {{0u, {0}}, {0u, {4}}, {0u, {8}}, {0u, {12}}}, 1u, 1u},
    }};
    const BdaTestShape shape{1u, 2u, invocations, true};
    for (const auto& testCase : cases) {
        const auto values = bdaTestValues(testCase.reads, shape);
        const auto single = runBdaReads(context, makeBdaReadsShader(true, shape, testCase.reads), *table, addresses, values * invocations, 1u, true);
        const auto slots = runBdaReads(context, makeBdaReadsShader(false, shape, testCase.reads), *table, addresses, values * invocations, 1u, true);
        const std::string label = std::string("BDA ") + testCase.name;
        requireSameBdaRuns(single, slots, label);
        for (std::uint32_t invocation = 0; invocation < invocations; ++invocation) {
            for (std::size_t read = 0; read < testCase.reads.size(); ++read) {
                const auto& entry = testCase.reads[read];
                const auto expected = readGuestDword(ranges, addresses[invocation * 2u + entry.slot] + static_cast<std::uint64_t>(entry.immediates.front()));
                Require(expected.has_value() && slots.output[invocation * values + read] == *expected, label + " returned the wrong value for invocation " + std::to_string(invocation) + " read " + std::to_string(read));
            }
        }
        Require(single.misses == testCase.singleMisses * invocations, label + " searched the BDA table " + std::to_string(single.misses) + " times through the single cache instead of " + std::to_string(testCase.singleMisses * invocations));
        Require(slots.misses == testCase.slotMisses * invocations, label + " searched the BDA table " + std::to_string(slots.misses) + " times through the per-resource cache instead of once per resource and invocation (" + std::to_string(testCase.slotMisses * invocations) + ")");
    }
}

std::uint32_t leftPattern(std::size_t word) {
    return static_cast<std::uint32_t>(word) * 0x01010101u + 0x10203040u;
}

std::uint32_t rightPattern(std::size_t word) {
    return static_cast<std::uint32_t>(word) * 0x11111111u + 0x0a0b0c0du;
}

bool declaresName(std::span<const std::uint32_t> words, std::string_view name) {
    const std::string_view text(reinterpret_cast<const char*>(words.data()), words.size_bytes());
    return text.find(name) != std::string_view::npos;
}

void recompiledScalarLoadsMatchSingleCache(const Context& context) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    constexpr std::uint64_t left = 0x50000u;
    constexpr std::uint64_t right = 0x58000u;
    constexpr std::uint64_t output = 0x60000u;
    constexpr std::uint32_t groups = 3u;
    constexpr std::uint32_t total = 64u * groups;
    constexpr std::array<std::uint32_t, 3> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    constexpr std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    const auto program = Synthetic::ScalarAddressLoadProgram(left, right, output, total * Synthetic::ScalarAddressOutputDwords * 4u);
    const auto expected = Synthetic::ScalarAddressLoadExpected(leftPattern, rightPattern);
    for (const bool unmappedRight : {false, true}) {
        std::array<std::vector<std::byte>, 2> outputs;
        std::array<Abi::Fault, 2> faults{};
        for (const bool singleCache : {true, false}) {
            auto request = program.Request(64u, 32u, true);
            request.target.bdaAbiVersion = Abi::Version;
            request.target.supportedCapabilities = capabilities;
            request.target.supportedExtensions = extensions;
            request.target.bdaSingleCache = singleCache;
            const auto result = ShaderRecompiler::Recompile(request);
            const std::string label = std::string("a recompiled program reading two scalar pointers") + (unmappedRight ? " with an unmapped second pointer" : "") + (singleCache ? " through the single cache" : " through per-resource caches");
            Require(std::any_of(result.bindings.begin(), result.bindings.end(), [](const ShaderRecompiler::DescriptorBinding& binding) { return binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable; }), label + " does not read through the BDA table");
            Require(declaresName(result.spirv.Words(), "bda_slot1_begin") != singleCache && declaresName(result.spirv.Words(), "bda_slot2_begin") == false, label + " does not give each scalar pointer its own cache slot");
            const std::array<std::uint64_t, 1> unmapped{right};
            auto& fault = faults[singleCache ? 0u : 1u];
            const auto memory = runRecompiled(context, result, {wordRegion(left, 16u, leftPattern), wordRegion(right, 16u, rightPattern), wordRegion(output, total * Synthetic::ScalarAddressOutputDwords, sentinelPattern)}, groups, &fault, unmappedRight ? std::span<const std::uint64_t>(unmapped) : std::span<const std::uint64_t>());
            for (std::uint32_t thread = 0; thread < total; ++thread) {
                for (std::uint32_t dword = 0; dword < Synthetic::ScalarAddressOutputDwords; ++dword) {
                    const auto actual = guestWord(memory[2], output + (thread * Synthetic::ScalarAddressOutputDwords + dword) * 4u);
                    const auto wanted = unmappedRight ? Sentinel : expected[dword];
                    Require(actual == wanted, label + " wrote " + std::to_string(actual) + " instead of " + std::to_string(wanted) + " for thread " + std::to_string(thread) + " dword " + std::to_string(dword));
                }
            }
            Require(unmappedRight ? fault.state == Abi::FaultState::Ready && fault.reason == Abi::FaultReason::Unmapped && fault.address == right && fault.bytes == 4u : fault.state == Abi::FaultState::Empty, label + " published fault " + describeFault(fault));
            outputs[singleCache ? 0u : 1u] = memory[2].bytes;
        }
        Require(outputs[0] == outputs[1] && std::memcmp(&faults[0], &faults[1], sizeof(Abi::Fault)) == 0, std::string("a recompiled program reading two scalar pointers") + (unmappedRight ? " with an unmapped second pointer" : "") + " behaved differently through per-resource caches than through the single cache");
    }
}

}

void RunRecompiledShaderTests(const AgcDriver::Graphics::Context& context) {
    tableValidationRejectsMalformedTables();
    bdaLaneReadsMatchGuestMemory(context);
    shortBdaTablesReportInvalidTable(context);
    groupedReadsMatchSingleCache(context);
    alternatingResourcesMissOncePerResource(context);
    const auto subgroupSize = measuredSubgroupSize(context);
    if (subgroupSize != 32u) {
        std::cout << "skipped the single-lane and dual-lane wave64 GPU tests: they compile for 32-wide subgroups and the device runs " << subgroupSize << "-wide subgroups\n";
        return;
    }
    perThreadProgramsMatchAcrossLaneModes(context);
    execReadAsScalarSeesTheWholeWave(context);
    vccBranchesAreWaveWide(context);
    wideLoadsKeepPerDwordBounds(context);
    recompiledScalarLoadsMatchSingleCache(context);
}

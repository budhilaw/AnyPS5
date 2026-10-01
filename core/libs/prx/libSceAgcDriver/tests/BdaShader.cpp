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

std::vector<std::uint32_t> MakeBdaTestShader(std::uint64_t address, std::uint32_t bits, std::int64_t offset) {
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
    DefineGetBdaPointer(state);
    const auto main = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunction, TypeVoid(state), main, spv::FunctionControlMaskNone, TypeFunction(state));
    EmitLabel(state, state.module.AllocateId());
    SpirvValueEmitContext ctx(state);
    auto& instruction = program.CreateValue(IrOpcode::LoadAddressU32, IrType::U32);
    MemoryFlags flags{};
    flags.pc = 0x1234;
    instruction.SetFlags(flags);
    auto base = BdaConstant(state, address);
    if (offset != 0) base = AddBdaAddress(ctx, instruction, base, BdaConstant(state, offset < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(offset) : static_cast<std::uint64_t>(offset)), offset < 0);
    const auto value = EmitBdaRead(ctx, instruction, base, bits);
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

std::vector<GuestRegion> runRecompiled(const Context& context, const ShaderRecompiler::RecompileResult& result, std::vector<GuestRegion> memory, std::uint32_t groups) {
    using ShaderRecompiler::DescriptorRole;
    struct Mirror {
        Buffer* buffer;
        std::uint64_t address;
        std::size_t size;
    };
    std::vector<std::unique_ptr<Buffer>> owned;
    std::vector<Mirror> mirrors;
    std::vector<StorageBinding> bindings;
    const auto makeBuffer = [&](std::size_t size) {
        return owned.emplace_back(std::make_unique<Buffer>(context, std::max<std::size_t>(size, sizeof(std::uint32_t)), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)).get();
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

std::vector<std::uint32_t> makeBdaLaneTestShader() {
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
    const auto invocation = state.module.DefineGlobalVariable(TypePointer(state, spv::StorageClassInput, TypeU32Vector(state, 3u)), spv::StorageClassInput);
    state.module.AddAnnotation(spv::OpDecorate, invocation, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId);
    DefineGetBdaPointer(state);
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
    const auto addressWord = [&](std::uint32_t half) {
        const auto word = Binary(state, spv::OpIAdd, TypeU32(state), Binary(state, spv::OpIMul, TypeU32(state), index, ConstantU32(state, 2u)), ConstantU32(state, half));
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, BdaWord(state, addresses, word));
        return Unary(state, spv::OpUConvert, TypeScalarU64(state), value);
    };
    const auto address = Binary(state, spv::OpBitwiseOr, TypeScalarU64(state), addressWord(0u), Binary(state, spv::OpShiftLeftLogical, TypeScalarU64(state), addressWord(1u), BdaConstant(state, 32u)));
    const auto value = EmitBdaRead(ctx, instruction, address, 32u);
    state.module.AddFunction(spv::OpStore, BdaWord(state, output, index), value);
    state.module.AddFunction(spv::OpReturn);
    state.module.AddFunction(spv::OpFunctionEnd);
    state.module.AddExecutionMode(main, spv::ExecutionModeLocalSize, LaneGroupSize, 1u, 1u);
    state.module.EmitEntryPoint(spv::ExecutionModelGLCompute, main, "main", {invocation});
    return state.module.Finalize();
}

struct GuestBda {
    std::uint64_t address;
    std::vector<std::byte> bytes;
    std::unique_ptr<Buffer> buffer;
};

std::optional<std::uint32_t> readGuestDword(const std::vector<GuestBda>& ranges, std::uint64_t address) {
    std::uint32_t value = 0;
    for (std::uint32_t byte = 0; byte < 4u; ++byte) {
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const GuestBda& range) { return address + byte >= range.address && address + byte < range.address + range.bytes.size(); });
        if (found == ranges.end()) {
            return std::nullopt;
        }
        value |= static_cast<std::uint32_t>(found->bytes[address + byte - found->address]) << (byte * 8u);
    }
    return value;
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
    const auto addRange = [&](std::uint64_t address, std::size_t size, std::uint32_t seed) {
        GuestBda range{address, std::vector<std::byte>(size), std::make_unique<Buffer>(context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)};
        for (std::size_t byte = 0; byte < size; ++byte) range.bytes[byte] = static_cast<std::byte>(seed + byte * 7u);
        std::memcpy(range.buffer->Bytes().data(), range.bytes.data(), size);
        ranges.push_back(std::move(range));
    };
    addRange(guest, 3u, 0x11u);
    addRange(guest + 3u, 2u, 0x44u);
    addRange(aligned, 8u, 0x10u);
    addRange(wide, 256u, 0x80u);
    std::vector<Abi::Range> entries;
    for (const auto& range : ranges) entries.push_back({range.address, range.address + range.bytes.size(), range.buffer->DeviceAddress(), Abi::Read, 0});
    Buffer table(context, sizeof(Abi::Header) + entries.size() * sizeof(Abi::Range), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const Abi::Header header{Abi::Version, static_cast<std::uint32_t>(entries.size()), sizeof(Abi::Range), 0};
    std::memcpy(table.Bytes().data(), &header, sizeof(header));
    std::memcpy(table.Bytes().data() + sizeof(header), entries.data(), entries.size() * sizeof(Abi::Range));
    Require(Abi::IsValidTable(table.Bytes()), "the BDA test table is malformed");
    Buffer fault(context, sizeof(Abi::Fault), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Buffer output(context, LaneInvocations * sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Buffer addresses(context, LaneInvocations * sizeof(std::uint64_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const std::array<StorageBinding, 4> bindings{{
        {0u, {{table.Handle(), 0, table.Bytes().size()}}},
        {1u, {{fault.Handle(), 0, fault.Bytes().size()}}},
        {2u, {{output.Handle(), 0, output.Bytes().size()}}},
        {3u, {{addresses.Handle(), 0, addresses.Bytes().size()}}},
    }};
    const auto shader = makeBdaLaneTestShader();
    ComputeDispatch dispatch(context, shader, bindings, false);
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
        for (std::uint32_t invocation = 0; invocation < LaneInvocations; ++invocation) {
            const auto address = pattern.address(invocation);
            std::memcpy(addresses.Bytes().data() + invocation * sizeof(address), &address, sizeof(address));
            std::memcpy(output.Bytes().data() + invocation * sizeof(Sentinel), &Sentinel, sizeof(Sentinel));
        }
        std::memset(fault.Bytes().data(), 0, fault.Bytes().size());
        dispatch.Run(LaneGroups, {});
        bool faulted = false;
        for (std::uint32_t invocation = 0; invocation < LaneInvocations; ++invocation) {
            std::uint32_t value = 0;
            std::memcpy(&value, output.Bytes().data() + invocation * sizeof(value), sizeof(value));
            const auto expected = readGuestDword(ranges, pattern.address(invocation));
            faulted = faulted || !expected.has_value();
            Require(value == expected.value_or(Sentinel), std::string("a BDA read of ") + pattern.name + " returned " + std::to_string(value) + " for invocation " + std::to_string(invocation));
        }
        Abi::Fault report{};
        std::memcpy(&report, fault.Bytes().data(), sizeof(report));
        Require(faulted ? report.state == Abi::FaultState::Ready && report.reason == Abi::FaultReason::Unmapped && report.instruction == 0x1234u : report.state == Abi::FaultState::Empty, std::string("a BDA read of ") + pattern.name + " published the wrong fault");
    }
}

void shortBdaTablesReportInvalidTable(const Context& context) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    constexpr std::uint64_t address = 0x7fff12350000ULL;
    const std::array<std::uint32_t, 4> header{Abi::Version, 1u, sizeof(Abi::Range), 0u};
    const auto shader = makeBdaLaneTestShader();
    for (const std::uint32_t words : {1u, 3u, 4u}) {
        Buffer table(context, words * sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        std::memcpy(table.Bytes().data(), header.data(), words * sizeof(std::uint32_t));
        Buffer fault(context, sizeof(Abi::Fault), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        Buffer output(context, LaneInvocations * sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        Buffer addresses(context, LaneInvocations * sizeof(std::uint64_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        for (std::uint32_t invocation = 0; invocation < LaneInvocations; ++invocation) {
            const std::uint64_t lane = address + invocation * 4u;
            std::memcpy(addresses.Bytes().data() + invocation * sizeof(lane), &lane, sizeof(lane));
            std::memcpy(output.Bytes().data() + invocation * sizeof(Sentinel), &Sentinel, sizeof(Sentinel));
        }
        std::memset(fault.Bytes().data(), 0, fault.Bytes().size());
        const std::array<StorageBinding, 4> bindings{{
            {0u, {{table.Handle(), 0, table.Bytes().size()}}},
            {1u, {{fault.Handle(), 0, fault.Bytes().size()}}},
            {2u, {{output.Handle(), 0, output.Bytes().size()}}},
            {3u, {{addresses.Handle(), 0, addresses.Bytes().size()}}},
        }};
        ComputeDispatch dispatch(context, shader, bindings, false);
        dispatch.Run(LaneGroups, {});
        const std::string label = "a BDA read through a table of " + std::to_string(words) + " dword(s) that counts one range";
        for (std::uint32_t invocation = 0; invocation < LaneInvocations; ++invocation) {
            std::uint32_t value = 0;
            std::memcpy(&value, output.Bytes().data() + invocation * sizeof(value), sizeof(value));
            Require(value == Sentinel, label + " returned " + std::to_string(value) + " for invocation " + std::to_string(invocation));
        }
        Abi::Fault report{};
        std::memcpy(&report, fault.Bytes().data(), sizeof(report));
        Require(report.state == Abi::FaultState::Ready && report.reason == Abi::FaultReason::InvalidTable && report.instruction == 0x1234u, label + " did not publish an InvalidTable fault");
    }
}

}

void RunRecompiledShaderTests(const AgcDriver::Graphics::Context& context) {
    tableValidationRejectsMalformedTables();
    bdaLaneReadsMatchGuestMemory(context);
    shortBdaTablesReportInvalidTable(context);
    const auto subgroupSize = measuredSubgroupSize(context);
    if (subgroupSize != 32u) {
        std::cout << "skipped the single-lane and dual-lane wave64 GPU tests: they compile for 32-wide subgroups and the device runs " << subgroupSize << "-wide subgroups\n";
        return;
    }
    perThreadProgramsMatchAcrossLaneModes(context);
    execReadAsScalarSeesTheWholeWave(context);
    vccBranchesAreWaveWide(context);
    wideLoadsKeepPerDwordBounds(context);
}

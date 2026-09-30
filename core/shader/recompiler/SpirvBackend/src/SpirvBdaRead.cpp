#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <algorithm>
#include <format>
#include <limits>
#include <unordered_map>
#include <stdexcept>

namespace ShaderRecompiler {

std::uint32_t AddBdaAddress(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t offset, bool subtract) {
    auto& state = ctx.state;
    const auto result = Binary(state, subtract ? spv::OpISub : spv::OpIAdd, TypeScalarU64(state), address, offset);
    const auto overflow = subtract ? Binary(state, spv::OpUGreaterThan, TypeBool(state), offset, address) : Binary(state, spv::OpULessThan, TypeBool(state), result, address);
    EmitIfCondition(state, overflow, [&] { RecordBdaFault(state, address, ConstantU32(state, 0u), ConstantU32(state, inst.Flags<MemoryFlags>().pc), BdaAbi::FaultReason::Overflow); });
    StopBdaInvocationIf(state, overflow);
    return result;
}

void ValidateBdaTarget(const IrProgram& program, const SpirvTargetOptions& target) {
    if (!program.Info().usesDma) return;
    if (target.bdaAbiVersion != BdaAbi::Version) throw std::runtime_error("unsupported BDA ABI version");
    for (const auto capability : {spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess}) {
        if (std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), static_cast<std::uint32_t>(capability)) == target.supportedCapabilities.end()) throw std::runtime_error("BDA requires unsupported SPIR-V capability " + std::to_string(capability));
    }
    for (const auto extension : {"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"}) {
        if (std::find(target.supportedExtensions.begin(), target.supportedExtensions.end(), extension) == target.supportedExtensions.end()) throw std::runtime_error(std::string("BDA requires unsupported extension ") + extension);
    }
    if (program.Resources().stage == IrShaderStage::Mesh || program.Resources().stage == IrShaderStage::TessellationControl) throw std::runtime_error("BDA fault termination requires a barrier-safe mesh or tessellation-control execution protocol");
    std::unordered_map<const IrBlock*, bool> reachesBarrier;
    for (const auto* block : program.BlockOrder()) {
        reachesBarrier[block] = std::ranges::any_of(block->Instructions(), [](const IrValue* instruction) { return instruction->Opcode() == IrOpcode::Barrier; });
    }
    for (bool changed = true; changed;) {
        changed = false;
        for (const auto* block : program.BlockOrder()) {
            if (reachesBarrier[block]) continue;
            if (std::ranges::any_of(block->Successors(), [&](const IrBlock* successor) { return reachesBarrier[successor]; })) {
                reachesBarrier[block] = true;
                changed = true;
            }
        }
    }
    for (const auto* block : program.BlockOrder()) {
        bool barrierAfter = std::ranges::any_of(block->Successors(), [&](const IrBlock* successor) { return reachesBarrier[successor]; });
        for (auto it = block->Instructions().rbegin(); it != block->Instructions().rend(); ++it) {
            if ((*it)->Opcode() == IrOpcode::Barrier) barrierAfter = true;
            else if (barrierAfter && AddressOpcodeInfoOf((*it)->Opcode()).access != AddressAccess::None) {
                const auto flags = (*it)->Flags<MemoryFlags>();
                if (flags.index < program.Resources().memoryInfo.size()) {
                    const auto& memory = program.Resources().memoryInfo[flags.index];
                    if (memory.planningOnly || memory.kind == ResourceKind::Scratch) continue;
                }
                throw std::runtime_error("BDA fault termination cannot bypass a workgroup barrier (read at pc 0x" + std::format("{:x}", flags.pc) + ")");
            }
        }
    }
}

std::uint32_t EmitBdaRead(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t bits) {
    auto& state = ctx.state;
    if (bits != 8u && bits != 16u && bits != 32u) ctx.Fail(inst, "unsupported BDA read width");
    if (state.bdaPointerFunction == 0) ctx.Fail(inst, "BDA lookup function is missing");
    const auto instruction = ConstantU32(state, inst.Flags<MemoryFlags>().pc);
    const auto overflow = Binary(state, spv::OpUGreaterThan, TypeBool(state), address, BdaConstant(state, std::numeric_limits<std::uint64_t>::max() - bits / 8u));
    EmitIfCondition(state, overflow, [&] { RecordBdaFault(state, address, ConstantU32(state, bits / 8u), instruction, BdaAbi::FaultReason::Overflow); });
    StopBdaInvocationIf(state, overflow);
    const auto byteType = state.module.Type(spv::OpTypeInt, 8u, 0u);
    const auto bytePointer = TypePointer(state, spv::StorageClassPhysicalStorageBuffer, byteType);
    std::uint32_t fastValue = 0;
    std::uint32_t fastEnd = 0;
    std::uint32_t merge = 0;
    if (bits == 32u) {
        const auto aligned = Binary(state, spv::OpIEqual, TypeBool(state), Binary(state, spv::OpBitwiseAnd, TypeScalarU64(state), address, BdaConstant(state, 3u)), BdaConstant(state, 0u));
        const auto fast = state.module.AllocateId();
        const auto slow = state.module.AllocateId();
        merge = state.module.AllocateId();
        state.module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, aligned, fast, slow);
        EmitLabel(state, fast);
        const auto physical = state.module.AllocateId();
        state.module.AddFunction(spv::OpFunctionCall, TypeScalarU64(state), physical, state.bdaPointerFunction, address, ConstantU32(state, 4u), instruction);
        StopBdaInvocationIf(state, Binary(state, spv::OpIEqual, TypeBool(state), physical, BdaConstant(state, 0u)));
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, TypeU32(state)), pointer, physical);
        fastValue = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), fastValue, pointer, spv::MemoryAccessAlignedMask, 4u);
        fastEnd = state.currentLabel;
        state.module.AddFunction(spv::OpBranch, merge);
        EmitLabel(state, slow);
    }
    auto result = ConstantU32(state, 0u);
    for (std::uint32_t byte = 0; byte < bits / 8u; ++byte) {
        const auto guest = Binary(state, spv::OpIAdd, TypeScalarU64(state), address, BdaConstant(state, byte));
        const auto physical = state.module.AllocateId();
        state.module.AddFunction(spv::OpFunctionCall, TypeScalarU64(state), physical, state.bdaPointerFunction, guest, ConstantU32(state, 1u), instruction);
        StopBdaInvocationIf(state, Binary(state, spv::OpIEqual, TypeBool(state), physical, BdaConstant(state, 0u)));
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpConvertUToPtr, bytePointer, pointer, physical);
        const auto loaded = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, byteType, loaded, pointer, spv::MemoryAccessAlignedMask, 1u);
        const auto value = Unary(state, spv::OpUConvert, TypeU32(state), loaded);
        result = Binary(state, spv::OpBitwiseOr, TypeU32(state), result, Binary(state, spv::OpShiftLeftLogical, TypeU32(state), value, ConstantU32(state, byte * 8u)));
    }
    if (merge == 0u) return result;
    const auto slowEnd = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, merge);
    const auto merged = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, TypeU32(state), merged, fastValue, fastEnd, result, slowEnd);
    return merged;
}

}

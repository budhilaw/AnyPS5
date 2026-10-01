#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <limits>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

std::uint32_t BdaReadFunctionIndex(std::uint32_t bytes) {
    switch (bytes) {
    case 1u:
        return 0u;
    case 2u:
        return 1u;
    case 4u:
        return 2u;
    default:
        throw std::runtime_error("unsupported BDA read width");
    }
}

std::uint32_t CallBdaFind(SpirvEmitterState& state, std::uint32_t address, std::uint32_t bytes, std::uint32_t instruction, std::uint32_t record) {
    const auto window = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionCall, state.bdaWindowType, window, state.bdaFindFunction, address, bytes, instruction, record);
    return window;
}

std::uint32_t WindowField(SpirvEmitterState& state, std::uint32_t window, std::uint32_t field) {
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeExtract, TypeScalarU64(state), value, window, field);
    return value;
}

std::uint32_t LoadPhysicalByte(SpirvEmitterState& state, std::uint32_t physical) {
    const auto byteType = state.module.Type(spv::OpTypeInt, 8u, 0u);
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, byteType), pointer, physical);
    const auto loaded = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, byteType, loaded, pointer, spv::MemoryAccessAlignedMask, 1u);
    return Unary(state, spv::OpUConvert, TypeU32(state), loaded);
}

std::uint32_t LoadPhysicalBytes(SpirvEmitterState& state, std::uint32_t physical, std::uint32_t bytes) {
    std::uint32_t value = 0;
    for (std::uint32_t byte = 0; byte < bytes; ++byte) {
        const auto address = byte == 0u ? physical : Binary(state, spv::OpIAdd, TypeScalarU64(state), physical, BdaConstant(state, byte));
        const auto loaded = LoadPhysicalByte(state, address);
        const auto shifted = byte == 0u ? loaded : Binary(state, spv::OpShiftLeftLogical, TypeU32(state), loaded, ConstantU32(state, byte * 8u));
        value = byte == 0u ? shifted : Binary(state, spv::OpBitwiseOr, TypeU32(state), value, shifted);
    }
    return value;
}

std::uint32_t LoadPhysicalDword(SpirvEmitterState& state, std::uint32_t physical) {
    const auto pointer = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, TypeU32(state)), pointer, physical);
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer, spv::MemoryAccessAlignedMask, 4u);
    return value;
}

void DefineBdaFind(SpirvEmitterState& state) {
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const auto constant = [&](std::uint32_t value) { return ConstantU32(state, value); };
    const auto binary = [&](std::uint32_t op, std::uint32_t type, std::uint32_t left, std::uint32_t right) { return Binary(state, op, type, left, right); };
    const auto load = [&](std::uint32_t pointer) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, u32, value, pointer);
        return value;
    };
    const auto functionType = state.module.Type(spv::OpTypeFunction, state.bdaWindowType, u64, u32, u32, boolean);
    const auto empty = state.module.Constant(spv::OpConstantNull, state.bdaWindowType);
    state.bdaFindFunction = state.module.AllocateId();
    state.module.AddName(state.bdaFindFunction, "bda_find");
    state.module.AddFunction(spv::OpFunction, state.bdaWindowType, state.bdaFindFunction, spv::FunctionControlDontInlineMask, functionType);
    const auto address = state.module.AllocateId();
    const auto bytes = state.module.AllocateId();
    const auto instruction = state.module.AllocateId();
    const auto record = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionParameter, u64, address);
    state.module.AddFunction(spv::OpFunctionParameter, u32, bytes);
    state.module.AddFunction(spv::OpFunctionParameter, u32, instruction);
    state.module.AddFunction(spv::OpFunctionParameter, boolean, record);
    EmitLabel(state, state.module.AllocateId());
    const auto low = state.module.AllocateId();
    const auto high = state.module.AllocateId();
    const auto pointer = TypePointer(state, spv::StorageClassFunction, u32);
    state.module.AddFunction(spv::OpVariable, pointer, low, spv::StorageClassFunction);
    state.module.AddFunction(spv::OpVariable, pointer, high, spv::StorageClassFunction);
    EmitBdaMissCount(state);
    const auto fail = [&](std::uint32_t condition, BdaAbi::FaultReason reason) {
        const auto failed = state.module.AllocateId();
        const auto next = state.module.AllocateId();
        state.module.AddFunction(spv::OpSelectionMerge, next, spv::SelectionControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, condition, failed, next);
        EmitLabel(state, failed);
        EmitIfCondition(state, record, [&] {
            const auto call = state.module.AllocateId();
            state.module.AddFunction(spv::OpFunctionCall, TypeVoid(state), call, state.bdaFaultFunction, address, bytes, instruction, constant(static_cast<std::uint32_t>(reason)));
        });
        state.module.AddFunction(spv::OpReturnValue, empty);
        EmitLabel(state, next);
    };
    const auto length = state.module.AllocateId();
    state.module.AddFunction(spv::OpArrayLength, u32, length, state.bdaPagetableVariable, 0u);
    fail(binary(spv::OpULessThan, boolean, length, constant(4)), BdaAbi::FaultReason::InvalidTable);
    const auto count = BdaLoadWord(state, constant(1));
    fail(binary(spv::OpUGreaterThan, boolean, count, binary(spv::OpShiftRightLogical, u32, binary(spv::OpISub, u32, length, constant(4)), constant(3))), BdaAbi::FaultReason::InvalidTable);
    const auto end = binary(spv::OpIAdd, u64, address, Unary(state, spv::OpUConvert, u64, bytes));
    fail(binary(spv::OpULessThanEqual, boolean, end, address), BdaAbi::FaultReason::Overflow);
    state.module.AddFunction(spv::OpStore, low, constant(0));
    state.module.AddFunction(spv::OpStore, high, count);
    const auto header = state.module.AllocateId();
    const auto body = state.module.AllocateId();
    const auto continuation = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, header);
    const auto lower = load(low);
    const auto upper = load(high);
    const auto search = binary(spv::OpULessThan, boolean, lower, upper);
    state.module.AddFunction(spv::OpLoopMerge, merge, continuation, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, search, body, merge);
    EmitLabel(state, body);
    const auto midpoint = binary(spv::OpIAdd, u32, lower, binary(spv::OpShiftRightLogical, u32, binary(spv::OpISub, u32, upper, lower), constant(1)));
    const auto entry = binary(spv::OpIAdd, u32, constant(4), binary(spv::OpIMul, u32, midpoint, constant(8)));
    const auto before = binary(spv::OpULessThan, boolean, address, BdaLoadAddress(state, entry));
    const auto newLow = state.module.AllocateId();
    const auto newHigh = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelect, u32, newLow, before, lower, binary(spv::OpIAdd, u32, midpoint, constant(1)));
    state.module.AddFunction(spv::OpSelect, u32, newHigh, before, midpoint, upper);
    state.module.AddFunction(spv::OpStore, low, newLow);
    state.module.AddFunction(spv::OpStore, high, newHigh);
    state.module.AddFunction(spv::OpBranch, continuation);
    EmitLabel(state, continuation);
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, merge);
    const auto index = load(low);
    fail(binary(spv::OpIEqual, boolean, index, constant(0)), BdaAbi::FaultReason::Unmapped);
    const auto selected = binary(spv::OpIAdd, u32, constant(4), binary(spv::OpIMul, u32, binary(spv::OpISub, u32, index, constant(1)), constant(8)));
    const auto at = [&](std::uint32_t offset) { return binary(spv::OpIAdd, u32, selected, constant(offset)); };
    const auto begin = BdaLoadAddress(state, selected);
    const auto finish = BdaLoadAddress(state, at(2));
    const auto base = BdaLoadAddress(state, at(4));
    const auto permissions = BdaLoadWord(state, at(6));
    fail(binary(spv::OpUGreaterThan, boolean, end, finish), BdaAbi::FaultReason::Unmapped);
    fail(binary(spv::OpIEqual, boolean, binary(spv::OpBitwiseAnd, u32, permissions, constant(BdaAbi::Read)), constant(0)), BdaAbi::FaultReason::Permission);
    const auto found = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, state.bdaWindowType, found, begin, finish, binary(spv::OpISub, u64, base, begin));
    state.module.AddFunction(spv::OpReturnValue, found);
    state.module.AddFunction(spv::OpFunctionEnd);
}

void DefineBdaRead(SpirvEmitterState& state, std::uint32_t bytes) {
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const auto binary = [&](std::uint32_t op, std::uint32_t type, std::uint32_t left, std::uint32_t right) { return Binary(state, op, type, left, right); };
    const auto functionType = state.module.Type(spv::OpTypeFunction, u64, u64, u64, u64, u32, u32);
    const auto function = state.module.AllocateId();
    state.bdaReadFunctions.at(BdaReadFunctionIndex(bytes)) = function;
    state.module.AddName(function, "bda_read_" + std::to_string(bytes));
    state.module.AddFunction(spv::OpFunction, u64, function, spv::FunctionControlDontInlineMask, functionType);
    const auto base = state.module.AllocateId();
    const auto offset = state.module.AllocateId();
    const auto immediate = state.module.AllocateId();
    const auto mode = state.module.AllocateId();
    const auto instruction = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionParameter, u64, base);
    state.module.AddFunction(spv::OpFunctionParameter, u64, offset);
    state.module.AddFunction(spv::OpFunctionParameter, u64, immediate);
    state.module.AddFunction(spv::OpFunctionParameter, u32, mode);
    state.module.AddFunction(spv::OpFunctionParameter, u32, instruction);
    EmitLabel(state, state.module.AllocateId());
    const auto failure = BdaConstant(state, std::uint64_t{1} << 32u);
    const auto returnIf = [&](std::uint32_t condition, std::uint32_t value, auto&& before) {
        const auto taken = state.module.AllocateId();
        const auto next = state.module.AllocateId();
        state.module.AddFunction(spv::OpSelectionMerge, next, spv::SelectionControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, condition, taken, next);
        EmitLabel(state, taken);
        before();
        state.module.AddFunction(spv::OpReturnValue, value);
        EmitLabel(state, next);
    };
    const auto overflowIf = [&](std::uint32_t condition, std::uint32_t address, std::uint32_t size) {
        returnIf(condition, failure, [&] {
            const auto call = state.module.AllocateId();
            state.module.AddFunction(spv::OpFunctionCall, TypeVoid(state), call, state.bdaFaultFunction, address, size, instruction, ConstantU32(state, static_cast<std::uint32_t>(BdaAbi::FaultReason::Overflow)));
        });
    };
    const auto flag = [&](std::uint32_t bit) { return binary(spv::OpINotEqual, boolean, binary(spv::OpBitwiseAnd, u32, mode, ConstantU32(state, bit)), ConstantU32(state, 0u)); };
    const auto addOffset = flag(1u);
    const auto sum = binary(spv::OpIAdd, u64, base, offset);
    overflowIf(binary(spv::OpLogicalAnd, boolean, addOffset, binary(spv::OpULessThan, boolean, sum, base)), base, ConstantU32(state, 0u));
    const auto address = Select(state, u64, addOffset, sum, base);
    const auto subtract = flag(2u);
    const auto lowered = binary(spv::OpISub, u64, address, immediate);
    const auto raised = binary(spv::OpIAdd, u64, address, immediate);
    overflowIf(Select(state, boolean, subtract, binary(spv::OpUGreaterThan, boolean, immediate, address), binary(spv::OpULessThan, boolean, raised, address)), address, ConstantU32(state, 0u));
    const auto guest = Select(state, u64, subtract, lowered, raised);
    const auto size = ConstantU32(state, bytes);
    overflowIf(binary(spv::OpUGreaterThan, boolean, guest, BdaConstant(state, std::numeric_limits<std::uint64_t>::max() - bytes)), guest, size);
    const auto returnValue = [&](std::uint32_t value) { state.module.AddFunction(spv::OpReturnValue, Unary(state, spv::OpUConvert, u64, value)); };
    const auto nothing = [] {};
    if (bytes == 1u) {
        const auto window = CallBdaFind(state, guest, size, instruction, ConstantBool(state, true));
        returnIf(binary(spv::OpIEqual, boolean, WindowField(state, window, 1u), BdaConstant(state, 0u)), failure, nothing);
        returnValue(LoadPhysicalByte(state, binary(spv::OpIAdd, u64, guest, WindowField(state, window, 2u))));
        state.module.AddFunction(spv::OpFunctionEnd);
        return;
    }
    const auto aligned = bytes == 4u ? binary(spv::OpIEqual, boolean, binary(spv::OpBitwiseAnd, u64, guest, BdaConstant(state, 3u)), BdaConstant(state, 0u)) : ConstantBool(state, false);
    const auto window = CallBdaFind(state, guest, size, instruction, aligned);
    const auto found = binary(spv::OpINotEqual, boolean, WindowField(state, window, 1u), BdaConstant(state, 0u));
    const auto physical = binary(spv::OpIAdd, u64, guest, WindowField(state, window, 2u));
    const auto foundLabel = state.module.AllocateId();
    const auto missingLabel = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, missingLabel, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, found, foundLabel, missingLabel);
    EmitLabel(state, foundLabel);
    if (bytes == 4u) {
        const auto dwordLabel = state.module.AllocateId();
        const auto bytesLabel = state.module.AllocateId();
        const auto joined = state.module.AllocateId();
        state.module.AddFunction(spv::OpSelectionMerge, joined, spv::SelectionControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, aligned, dwordLabel, bytesLabel);
        EmitLabel(state, dwordLabel);
        const auto dword = LoadPhysicalDword(state, physical);
        const auto dwordEnd = state.currentLabel;
        state.module.AddFunction(spv::OpBranch, joined);
        EmitLabel(state, bytesLabel);
        const auto assembled = LoadPhysicalBytes(state, physical, bytes);
        const auto bytesEnd = state.currentLabel;
        state.module.AddFunction(spv::OpBranch, joined);
        EmitLabel(state, joined);
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpPhi, u32, value, dword, dwordEnd, assembled, bytesEnd);
        returnValue(value);
    } else {
        returnValue(LoadPhysicalBytes(state, physical, bytes));
    }
    EmitLabel(state, missingLabel);
    if (bytes == 4u) returnIf(aligned, failure, nothing);
    const auto preheader = state.currentLabel;
    const auto header = state.module.AllocateId();
    const auto body = state.module.AllocateId();
    const auto continuation = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    const auto byte = state.module.AllocateId();
    const auto value = state.module.AllocateId();
    const auto nextByte = state.module.AllocateId();
    const auto nextValue = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, header);
    state.module.AddFunction(spv::OpPhi, u32, byte, ConstantU32(state, 0u), preheader, nextByte, continuation);
    state.module.AddFunction(spv::OpPhi, u32, value, ConstantU32(state, 0u), preheader, nextValue, continuation);
    const auto remaining = binary(spv::OpULessThan, boolean, byte, size);
    state.module.AddFunction(spv::OpLoopMerge, merge, continuation, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, remaining, body, merge);
    EmitLabel(state, body);
    const auto byteAddress = binary(spv::OpIAdd, u64, guest, Unary(state, spv::OpUConvert, u64, byte));
    const auto byteWindow = CallBdaFind(state, byteAddress, ConstantU32(state, 1u), instruction, ConstantBool(state, true));
    returnIf(binary(spv::OpIEqual, boolean, WindowField(state, byteWindow, 1u), BdaConstant(state, 0u)), failure, nothing);
    const auto loaded = LoadPhysicalByte(state, binary(spv::OpIAdd, u64, byteAddress, WindowField(state, byteWindow, 2u)));
    state.module.AddFunction(spv::OpBitwiseOr, u32, nextValue, value, binary(spv::OpShiftLeftLogical, u32, loaded, binary(spv::OpShiftLeftLogical, u32, byte, ConstantU32(state, 3u))));
    state.module.AddFunction(spv::OpBranch, continuation);
    EmitLabel(state, continuation);
    state.module.AddFunction(spv::OpIAdd, u32, nextByte, byte, ConstantU32(state, 1u));
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, merge);
    returnValue(value);
    state.module.AddFunction(spv::OpFunctionEnd);
}

}

void DefineGetBdaPointer(SpirvEmitterState& state) {
    if (!state.program.Info().usesDma) return;
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const auto constant = [&](std::uint32_t value) { return ConstantU32(state, value); };
    const auto binary = [&](std::uint32_t op, std::uint32_t type, std::uint32_t left, std::uint32_t right) { return Binary(state, op, type, left, right); };
    const auto load = [&](std::uint32_t pointer) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, u32, value, pointer);
        return value;
    };
    const auto cachePointer = TypePointer(state, spv::StorageClassPrivate, u64);
    const auto zero = BdaConstant(state, 0u);
    state.bdaCacheBegin = state.module.DefineInitializedGlobalVariable(cachePointer, spv::StorageClassPrivate, zero);
    state.bdaCacheEnd = state.module.DefineInitializedGlobalVariable(cachePointer, spv::StorageClassPrivate, zero);
    state.bdaCacheBase = state.module.DefineInitializedGlobalVariable(cachePointer, spv::StorageClassPrivate, zero);
    state.module.AddName(state.bdaCacheBegin, "bda_cache_begin");
    state.module.AddName(state.bdaCacheEnd, "bda_cache_end");
    state.module.AddName(state.bdaCacheBase, "bda_cache_base");
    const auto functionType = state.module.Type(spv::OpTypeFunction, u64, u64, u32, u32);
    const auto missFunction = state.module.AllocateId();
    state.module.AddName(missFunction, "get_bda_pointer_miss");
    state.module.AddFunction(spv::OpFunction, u64, missFunction, spv::FunctionControlDontInlineMask, functionType);
    const auto address = state.module.AllocateId();
    const auto bytes = state.module.AllocateId();
    const auto instruction = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionParameter, u64, address);
    state.module.AddFunction(spv::OpFunctionParameter, u32, bytes);
    state.module.AddFunction(spv::OpFunctionParameter, u32, instruction);
    EmitLabel(state, state.module.AllocateId());
    const auto low = state.module.AllocateId();
    const auto high = state.module.AllocateId();
    const auto pointer = TypePointer(state, spv::StorageClassFunction, u32);
    state.module.AddFunction(spv::OpVariable, pointer, low, spv::StorageClassFunction);
    state.module.AddFunction(spv::OpVariable, pointer, high, spv::StorageClassFunction);
    EmitBdaMissCount(state);
    const auto fail = [&](std::uint32_t condition, BdaAbi::FaultReason reason) { ReturnBdaFailureIf(state, condition, address, bytes, instruction, reason); };
    const auto length = state.module.AllocateId();
    state.module.AddFunction(spv::OpArrayLength, u32, length, state.bdaPagetableVariable, 0u);
    fail(binary(spv::OpULessThan, boolean, length, constant(4)), BdaAbi::FaultReason::InvalidTable);
    const auto count = BdaLoadWord(state, constant(1));
    fail(binary(spv::OpUGreaterThan, boolean, count, binary(spv::OpShiftRightLogical, u32, binary(spv::OpISub, u32, length, constant(4)), constant(3))), BdaAbi::FaultReason::InvalidTable);
    const auto end = binary(spv::OpIAdd, u64, address, Unary(state, spv::OpUConvert, u64, bytes));
    fail(binary(spv::OpULessThanEqual, boolean, end, address), BdaAbi::FaultReason::Overflow);
    state.module.AddFunction(spv::OpStore, low, constant(0));
    state.module.AddFunction(spv::OpStore, high, count);
    const auto header = state.module.AllocateId();
    const auto body = state.module.AllocateId();
    const auto continuation = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, header);
    const auto lower = load(low);
    const auto upper = load(high);
    const auto search = binary(spv::OpULessThan, boolean, lower, upper);
    state.module.AddFunction(spv::OpLoopMerge, merge, continuation, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, search, body, merge);
    EmitLabel(state, body);
    const auto midpoint = binary(spv::OpIAdd, u32, lower, binary(spv::OpShiftRightLogical, u32, binary(spv::OpISub, u32, upper, lower), constant(1)));
    const auto entry = binary(spv::OpIAdd, u32, constant(4), binary(spv::OpIMul, u32, midpoint, constant(8)));
    const auto before = binary(spv::OpULessThan, boolean, address, BdaLoadAddress(state, entry));
    const auto newLow = state.module.AllocateId();
    const auto newHigh = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelect, u32, newLow, before, lower, binary(spv::OpIAdd, u32, midpoint, constant(1)));
    state.module.AddFunction(spv::OpSelect, u32, newHigh, before, midpoint, upper);
    state.module.AddFunction(spv::OpStore, low, newLow);
    state.module.AddFunction(spv::OpStore, high, newHigh);
    state.module.AddFunction(spv::OpBranch, continuation);
    EmitLabel(state, continuation);
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, merge);
    const auto index = load(low);
    fail(binary(spv::OpIEqual, boolean, index, constant(0)), BdaAbi::FaultReason::Unmapped);
    const auto selected = binary(spv::OpIAdd, u32, constant(4), binary(spv::OpIMul, u32, binary(spv::OpISub, u32, index, constant(1)), constant(8)));
    const auto at = [&](std::uint32_t offset) { return binary(spv::OpIAdd, u32, selected, constant(offset)); };
    const auto begin = BdaLoadAddress(state, selected);
    const auto finish = BdaLoadAddress(state, at(2));
    const auto base = BdaLoadAddress(state, at(4));
    const auto permissions = BdaLoadWord(state, at(6));
    fail(binary(spv::OpUGreaterThan, boolean, end, finish), BdaAbi::FaultReason::Unmapped);
    fail(binary(spv::OpIEqual, boolean, binary(spv::OpBitwiseAnd, u32, permissions, constant(BdaAbi::Read)), constant(0)), BdaAbi::FaultReason::Permission);
    const auto result = binary(spv::OpIAdd, u64, base, binary(spv::OpISub, u64, address, begin));
    state.module.AddFunction(spv::OpStore, state.bdaCacheBegin, begin);
    state.module.AddFunction(spv::OpStore, state.bdaCacheEnd, finish);
    state.module.AddFunction(spv::OpStore, state.bdaCacheBase, base);
    state.module.AddFunction(spv::OpReturnValue, result);
    state.module.AddFunction(spv::OpFunctionEnd);

    state.bdaPointerFunction = state.module.AllocateId();
    state.module.AddName(state.bdaPointerFunction, "get_bda_pointer");
    state.module.AddFunction(spv::OpFunction, u64, state.bdaPointerFunction, spv::FunctionControlMaskNone, functionType);
    const auto lookupAddress = state.module.AllocateId();
    const auto lookupBytes = state.module.AllocateId();
    const auto lookupInstruction = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionParameter, u64, lookupAddress);
    state.module.AddFunction(spv::OpFunctionParameter, u32, lookupBytes);
    state.module.AddFunction(spv::OpFunctionParameter, u32, lookupInstruction);
    EmitLabel(state, state.module.AllocateId());
    const auto loadCache = [&](std::uint32_t variable) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, u64, value, variable);
        return value;
    };
    const auto cachedBegin = loadCache(state.bdaCacheBegin);
    const auto cachedEnd = loadCache(state.bdaCacheEnd);
    const auto accessEnd = binary(spv::OpIAdd, u64, lookupAddress, Unary(state, spv::OpUConvert, u64, lookupBytes));
    const auto inside = binary(spv::OpLogicalAnd, boolean, binary(spv::OpUGreaterThanEqual, boolean, lookupAddress, cachedBegin), binary(spv::OpULessThanEqual, boolean, accessEnd, cachedEnd));
    const auto hit = binary(spv::OpLogicalAnd, boolean, inside, binary(spv::OpUGreaterThan, boolean, accessEnd, lookupAddress));
    const auto hitLabel = state.module.AllocateId();
    const auto missLabel = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, missLabel, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, hit, hitLabel, missLabel);
    EmitLabel(state, hitLabel);
    state.module.AddFunction(spv::OpReturnValue, binary(spv::OpIAdd, u64, loadCache(state.bdaCacheBase), binary(spv::OpISub, u64, lookupAddress, cachedBegin)));
    EmitLabel(state, missLabel);
    const auto missed = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionCall, u64, missed, missFunction, lookupAddress, lookupBytes, lookupInstruction);
    state.module.AddFunction(spv::OpReturnValue, missed);
    state.module.AddFunction(spv::OpFunctionEnd);
}

void DefineBdaFunctions(SpirvEmitterState& state) {
    if (!state.program.Info().usesDma || state.bdaSlotCount == 0u) return;
    const auto u64 = TypeScalarU64(state);
    state.bdaWindowType = state.module.Type(spv::OpTypeStruct, u64, u64, u64);
    DefineBdaFaultFunction(state);
    DefineBdaFind(state);
    for (const std::uint32_t bytes : {1u, 2u, 4u}) {
        if ((state.bdaReadWidths & bytes) != 0u) DefineBdaRead(state, bytes);
    }
    const auto slotPointer = TypePointer(state, spv::StorageClassPrivate, u64);
    const auto zero = BdaConstant(state, 0u);
    state.bdaSlots.clear();
    for (std::uint32_t slot = 0; slot < state.bdaSlotCount; ++slot) {
        BdaCacheSlot cache;
        cache.begin = state.module.DefineInitializedGlobalVariable(slotPointer, spv::StorageClassPrivate, zero);
        cache.end = state.module.DefineInitializedGlobalVariable(slotPointer, spv::StorageClassPrivate, zero);
        cache.delta = state.module.DefineInitializedGlobalVariable(slotPointer, spv::StorageClassPrivate, zero);
        const auto prefix = "bda_slot" + std::to_string(slot);
        state.module.AddName(cache.begin, prefix + "_begin");
        state.module.AddName(cache.end, prefix + "_end");
        state.module.AddName(cache.delta, prefix + "_delta");
        state.bdaSlots.push_back(cache);
    }
}

}

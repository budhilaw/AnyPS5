#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include <algorithm>
#include <format>
#include <limits>
#include <unordered_map>
#include <stdexcept>
#include <utility>

namespace ShaderRecompiler {

namespace {

bool PlannedBdaWidth(const IrProgram& program, const IrValue& inst, std::uint32_t& bits) {
    switch (inst.Opcode()) {
    case IrOpcode::LoadAddressU8:
        bits = 8u;
        break;
    case IrOpcode::LoadAddressU16:
        bits = 16u;
        break;
    case IrOpcode::LoadAddressU32:
        bits = 32u;
        break;
    default:
        return false;
    }
    const auto& mem = program.Resources().memoryInfo.at(inst.Flags<MemoryFlags>().index);
    if (bits == 32u && mem.planningOnly) return false;
    return mem.kind == ResourceKind::ScalarAddress || mem.kind == ResourceKind::Flat || mem.kind == ResourceKind::Global;
}

std::int64_t PlannedBdaImmediate(const MemoryInfo& mem) {
    auto immediate = static_cast<std::int32_t>(mem.offset);
    if (mem.kind == ResourceKind::ScalarAddress) immediate = static_cast<std::int32_t>(static_cast<std::uint32_t>(immediate) & ~3u);
    return immediate;
}

bool SameBdaOperand(const IrValue* left, const IrValue* right) {
    if (left == nullptr || right == nullptr) return left == right;
    left = left->Resolve();
    right = right->Resolve();
    if (left == right) return true;
    if (!left->HasImmediate() || !right->HasImmediate() || left->Type() != right->Type()) return false;
    switch (left->Type()) {
    case IrType::Bool:
        return left->ImmediateBool() == right->ImmediateBool();
    case IrType::U32:
        return left->ImmediateU32() == right->ImmediateU32();
    default:
        return false;
    }
}

bool TransparentToBdaGroup(IrOpcode opcode) {
    switch (opcode) {
    case IrOpcode::Identity:
    case IrOpcode::Waitcnt:
    case IrOpcode::SelectU32:
    case IrOpcode::BitCastF32U32:
    case IrOpcode::BitCastU32F32:
        return true;
    default:
        return false;
    }
}

bool JoinsBdaGroup(const IrProgram& program, const IrValue& leader, const IrValue& candidate, std::int64_t leaderImmediate) {
    std::uint32_t bits = 0;
    if (candidate.Opcode() != IrOpcode::LoadAddressU32 || !PlannedBdaWidth(program, candidate, bits)) return false;
    if (candidate.Flags<MemoryFlags>().pc != leader.Flags<MemoryFlags>().pc || candidate.ArgumentCount() != leader.ArgumentCount()) return false;
    const auto& first = program.Resources().memoryInfo.at(leader.Flags<MemoryFlags>().index);
    const auto& next = program.Resources().memoryInfo.at(candidate.Flags<MemoryFlags>().index);
    if (first.kind != next.kind || first.addressIsFull != next.addressIsFull) return false;
    for (std::size_t index = 0; index < leader.ArgumentCount(); ++index) {
        if (!SameBdaOperand(leader.Argument(index), candidate.Argument(index))) return false;
    }
    const auto distance = PlannedBdaImmediate(next) - leaderImmediate;
    return distance % 4 == 0 && distance >= -64 && distance <= 64;
}

std::pair<const IrValue*, const IrValue*> BdaSlotKey(const IrValue& inst, const MemoryInfo& mem) {
    if (mem.addressIsFull) return {nullptr, nullptr};
    const IrValue* argument = inst.Argument(0);
    const IrValue* handle = argument != nullptr ? argument->Resolve() : nullptr;
    if (handle == nullptr || handle->Opcode() != IrOpcode::GetAddressResource || handle->ArgumentCount() != 2u) return {nullptr, nullptr};
    return {handle->Argument(0)->Resolve(), handle->Argument(1)->Resolve()};
}

struct BdaProbe {
    std::uint32_t pointer = 0;
    std::uint32_t valid = 0;
};

BdaProbe EmitBdaSlotProbe(SpirvEmitterState& state, const BdaCacheSlot& slot, std::uint32_t first, std::uint32_t span, std::uint32_t condition, std::uint32_t instruction) {
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const auto load = [&](std::uint32_t variable) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, u64, value, variable);
        return value;
    };
    const auto begin = load(slot.begin);
    const auto end = load(slot.end);
    const auto delta = load(slot.delta);
    const auto last = Binary(state, spv::OpIAdd, u64, first, BdaConstant(state, span));
    const auto inside = Binary(state, spv::OpLogicalAnd, boolean, Binary(state, spv::OpUGreaterThanEqual, boolean, first, begin), Binary(state, spv::OpULessThanEqual, boolean, last, end));
    const auto hit = Binary(state, spv::OpLogicalAnd, boolean, condition, inside);
    const auto missing = Binary(state, spv::OpLogicalAnd, boolean, condition, Unary(state, spv::OpLogicalNot, boolean, inside));
    const auto entry = state.currentLabel;
    const auto findLabel = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, missing, findLabel, merge);
    EmitLabel(state, findLabel);
    const auto window = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionCall, state.bdaWindowType, window, state.bdaFindFunction, first, ConstantU32(state, span), instruction, ConstantBool(state, false));
    const auto field = [&](std::uint32_t index) {
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, u64, value, window, index);
        return value;
    };
    const auto foundBegin = field(0u);
    const auto foundEnd = field(1u);
    const auto foundDelta = field(2u);
    state.module.AddFunction(spv::OpStore, slot.begin, foundBegin);
    state.module.AddFunction(spv::OpStore, slot.end, foundEnd);
    state.module.AddFunction(spv::OpStore, slot.delta, foundDelta);
    const auto found = Binary(state, spv::OpINotEqual, boolean, foundEnd, BdaConstant(state, 0u));
    const auto findEnd = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, merge);
    const auto selectedDelta = state.module.AllocateId();
    const auto valid = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, u64, selectedDelta, delta, entry, foundDelta, findEnd);
    state.module.AddFunction(spv::OpPhi, boolean, valid, hit, entry, found, findEnd);
    return {Binary(state, spv::OpIAdd, u64, first, selectedDelta), valid};
}

std::uint32_t LoadBdaPhysical(SpirvEmitterState& state, std::uint32_t physical, std::uint32_t bits) {
    if (bits == 32u) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpConvertUToPtr, TypePointer(state, spv::StorageClassPhysicalStorageBuffer, TypeU32(state)), pointer, physical);
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, TypeU32(state), value, pointer, spv::MemoryAccessAlignedMask, 4u);
        return value;
    }
    const auto byteType = state.module.Type(spv::OpTypeInt, 8u, 0u);
    const auto bytePointer = TypePointer(state, spv::StorageClassPhysicalStorageBuffer, byteType);
    std::uint32_t result = 0;
    for (std::uint32_t byte = 0; byte < bits / 8u; ++byte) {
        const auto address = byte == 0u ? physical : Binary(state, spv::OpIAdd, TypeScalarU64(state), physical, BdaConstant(state, byte));
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpConvertUToPtr, bytePointer, pointer, address);
        const auto loaded = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, byteType, loaded, pointer, spv::MemoryAccessAlignedMask, 1u);
        const auto value = Unary(state, spv::OpUConvert, TypeU32(state), loaded);
        result = byte == 0u ? value : Binary(state, spv::OpBitwiseOr, TypeU32(state), result, Binary(state, spv::OpShiftLeftLogical, TypeU32(state), value, ConstantU32(state, byte * 8u)));
    }
    return result;
}

template<typename TFunction>
std::vector<std::uint32_t> EmitBdaValuesIfActive(SpirvEmitterState& state, std::uint32_t active, std::size_t count, TFunction&& function) {
    if (active == 0u) return function();
    const auto entry = state.currentLabel;
    const auto activeLabel = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, active, activeLabel, merge);
    EmitLabel(state, activeLabel);
    const auto values = function();
    const auto activeEnd = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, merge);
    std::vector<std::uint32_t> merged(count);
    for (std::size_t index = 0; index < count; ++index) {
        merged[index] = state.module.AllocateId();
        state.module.AddFunction(spv::OpPhi, TypeU32(state), merged[index], values[index], activeEnd, ConstantU32(state, 0u), entry);
    }
    return merged;
}

}

void PlanBdaReads(SpirvEmitterState& state) {
    const auto& program = state.program;
    state.bdaReads.clear();
    state.bdaReadIndex.clear();
    state.bdaReadWidths = 0u;
    state.bdaSlotCount = 0u;
    if (!program.Info().usesDma) return;
    std::vector<std::pair<const IrValue*, const IrValue*>> keys;
    for (const IrBlock* block : program.BlockOrder()) {
        const auto& instructions = block->Instructions();
        for (auto position = instructions.begin(); position != instructions.end(); ++position) {
            const IrValue* leader = *position;
            std::uint32_t bits = 0;
            if (!PlannedBdaWidth(program, *leader, bits) || state.bdaReadIndex.contains(leader)) continue;
            const auto& mem = program.Resources().memoryInfo.at(leader->Flags<MemoryFlags>().index);
            const auto key = BdaSlotKey(*leader, mem);
            auto found = std::find(keys.begin(), keys.end(), key);
            if (found == keys.end()) found = keys.insert(keys.end(), key);
            BdaPlannedRead read;
            read.members.push_back(leader);
            read.group.slot = static_cast<std::uint32_t>(found - keys.begin()) % MaxBdaCacheSlots;
            read.group.bits = bits;
            read.group.pc = leader->Flags<MemoryFlags>().pc;
            read.group.immediates.push_back(PlannedBdaImmediate(mem));
            for (auto next = std::next(position); bits == 32u && next != instructions.end() && read.members.size() < MaxBdaGroupReads; ++next) {
                const IrValue* candidate = *next;
                if (JoinsBdaGroup(program, *leader, *candidate, read.group.immediates.front())) {
                    read.members.push_back(candidate);
                    read.group.immediates.push_back(PlannedBdaImmediate(program.Resources().memoryInfo.at(candidate->Flags<MemoryFlags>().index)));
                    continue;
                }
                if (!TransparentToBdaGroup(candidate->Opcode())) break;
            }
            for (const IrValue* member : read.members) state.bdaReadIndex.emplace(member, state.bdaReads.size());
            state.bdaReadWidths |= bits / 8u;
            state.bdaReads.push_back(std::move(read));
        }
    }
    state.bdaSlotCount = static_cast<std::uint32_t>(std::min<std::size_t>(keys.size(), MaxBdaCacheSlots));
}

std::vector<std::vector<std::uint32_t>> EmitBdaGroupRead(SpirvEmitterState& state, const BdaReadGroup& group, std::span<const BdaLaneAddress> lanes) {
    const auto u32 = TypeU32(state);
    const auto u64 = TypeScalarU64(state);
    const auto boolean = TypeBool(state);
    const std::uint32_t bytes = group.bits / 8u;
    if (group.bits != 8u && group.bits != 16u && group.bits != 32u) throw std::runtime_error("unsupported BDA read width");
    if (group.immediates.empty() || lanes.empty()) throw std::runtime_error("a BDA read group has no reads or no lanes");
    if (group.slot >= state.bdaSlots.size() || state.bdaFindFunction == 0u) throw std::runtime_error("BDA lookup functions are missing");
    const auto readFunction = state.bdaReadFunctions.at(bytes == 1u ? 0u : bytes == 2u ? 1u : 2u);
    if (readFunction == 0u) throw std::runtime_error("the BDA read function for this width is missing");
    const auto [lowest, highest] = std::minmax_element(group.immediates.begin(), group.immediates.end());
    const std::int64_t low = *lowest;
    const std::int64_t end = *highest + static_cast<std::int64_t>(bytes);
    if (end - low > 4 * static_cast<std::int64_t>(MaxBdaGroupReads) + 64 || (bytes == 4u && std::any_of(group.immediates.begin(), group.immediates.end(), [&](std::int64_t immediate) { return (immediate - low) % 4 != 0; }))) throw std::runtime_error("a BDA read group spans an unsupported range");
    const auto span = static_cast<std::uint32_t>(end - low);
    const auto instruction = ConstantU32(state, group.pc);
    const auto& slot = state.bdaSlots[group.slot];
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    std::vector<BdaProbe> probes;
    std::uint32_t allValid = 0;
    for (const auto& lane : lanes) {
        std::uint32_t condition = 0;
        const auto require = [&](std::uint32_t term) { condition = condition == 0u ? term : Binary(state, spv::OpLogicalAnd, boolean, condition, term); };
        if (lane.active != 0u) require(lane.active);
        auto address = lane.base;
        if (lane.offset != 0u) {
            address = Binary(state, spv::OpIAdd, u64, lane.base, lane.offset);
            require(Binary(state, spv::OpUGreaterThanEqual, boolean, address, lane.base));
        }
        if (low < 0) require(Binary(state, spv::OpUGreaterThanEqual, boolean, address, BdaConstant(state, static_cast<std::uint64_t>(-low))));
        if (low > 0) require(Binary(state, spv::OpULessThanEqual, boolean, address, BdaConstant(state, maximum - static_cast<std::uint64_t>(low))));
        if (end > 0) require(Binary(state, spv::OpULessThanEqual, boolean, address, BdaConstant(state, maximum - static_cast<std::uint64_t>(end))));
        const auto first = low == 0 ? address : low > 0 ? Binary(state, spv::OpIAdd, u64, address, BdaConstant(state, static_cast<std::uint64_t>(low))) : Binary(state, spv::OpISub, u64, address, BdaConstant(state, static_cast<std::uint64_t>(-low)));
        if (bytes == 4u) require(Binary(state, spv::OpIEqual, boolean, Binary(state, spv::OpBitwiseAnd, u64, first, BdaConstant(state, 3u)), BdaConstant(state, 0u)));
        const auto probe = EmitBdaSlotProbe(state, slot, first, span, condition == 0u ? ConstantBool(state, true) : condition, instruction);
        const auto laneValid = lane.active == 0u ? probe.valid : Binary(state, spv::OpLogicalOr, boolean, Unary(state, spv::OpLogicalNot, boolean, lane.active), probe.valid);
        allValid = allValid == 0u ? laneValid : Binary(state, spv::OpLogicalAnd, boolean, allValid, laneValid);
        probes.push_back(probe);
    }
    const auto fastLabel = state.module.AllocateId();
    const auto carefulLabel = state.module.AllocateId();
    const auto merge = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, allValid, fastLabel, carefulLabel);
    EmitLabel(state, fastLabel);
    std::vector<std::vector<std::uint32_t>> fast;
    for (std::size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex) {
        fast.push_back(EmitBdaValuesIfActive(state, lanes[laneIndex].active, group.immediates.size(), [&] {
            std::vector<std::uint32_t> values;
            for (const auto immediate : group.immediates) {
                const auto distance = static_cast<std::uint64_t>(immediate - low);
                const auto physical = distance == 0u ? probes[laneIndex].pointer : Binary(state, spv::OpIAdd, u64, probes[laneIndex].pointer, BdaConstant(state, distance));
                values.push_back(LoadBdaPhysical(state, physical, group.bits));
            }
            return values;
        }));
    }
    const auto fastEnd = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, carefulLabel);
    std::vector<std::vector<std::uint32_t>> careful(lanes.size());
    for (const auto immediate : group.immediates) {
        for (std::size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex) {
            const auto& lane = lanes[laneIndex];
            const auto read = [&] {
                const std::uint32_t mode = (lane.offset != 0u ? 1u : 0u) | (immediate < 0 ? 2u : 0u);
                const auto magnitude = immediate < 0 ? static_cast<std::uint64_t>(-immediate) : static_cast<std::uint64_t>(immediate);
                const auto result = state.module.AllocateId();
                state.module.AddFunction(spv::OpFunctionCall, u64, result, readFunction, lane.base, lane.offset != 0u ? lane.offset : BdaConstant(state, 0u), BdaConstant(state, magnitude), ConstantU32(state, mode), instruction);
                StopBdaInvocationIf(state, Binary(state, spv::OpUGreaterThan, boolean, result, BdaConstant(state, 0xffffffffu)));
                return Unary(state, spv::OpUConvert, u32, result);
            };
            careful[laneIndex].push_back(lane.active == 0u ? read() : EmitValueOrZeroIfCondition(state, lane.active, read));
        }
    }
    const auto carefulEnd = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, merge);
    EmitLabel(state, merge);
    std::vector<std::vector<std::uint32_t>> values(lanes.size());
    for (std::size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex) {
        for (std::size_t member = 0; member < group.immediates.size(); ++member) {
            const auto value = state.module.AllocateId();
            state.module.AddFunction(spv::OpPhi, u32, value, fast[laneIndex][member], fastEnd, careful[laneIndex][member], carefulEnd);
            values[laneIndex].push_back(value);
        }
    }
    return values;
}

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

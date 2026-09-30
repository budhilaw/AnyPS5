#include "Optimization/IndirectBufferExpander.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace ShaderRecompiler {

namespace {

constexpr std::uint32_t DescriptorBytes = 16u;
constexpr std::uint32_t MaxCandidates = 24u;
constexpr std::uint32_t DefaultCandidates = 8u;

std::uint64_t PackFlags(MemoryFlags flags) {
    std::uint64_t raw = 0;
    std::memcpy(&raw, &flags, sizeof(flags));
    return raw;
}

bool Immediate(IrValue* value, std::uint32_t& result) {
    value = value->Resolve();
    if (!value->HasImmediate() || value->Type() != IrType::U32) return false;
    result = value->ImmediateU32();
    return true;
}

bool Evaluable(IrValue* value, std::uint32_t depth) {
    value = value->Resolve();
    if (value->HasImmediate()) return true;
    if (depth == 0u) return false;
    switch (value->Opcode()) {
        case IrOpcode::GetUserData:
            return true;
        case IrOpcode::IAdd32:
        case IrOpcode::ISub32:
        case IrOpcode::IMul32:
        case IrOpcode::ShiftLeftLogical32:
        case IrOpcode::ShiftRightLogical32:
        case IrOpcode::BitwiseAnd32:
        case IrOpcode::BitwiseOr32:
        case IrOpcode::BitwiseXor32:
        case IrOpcode::LoadAddressU32:
        case IrOpcode::ReadConstBuffer:
        case IrOpcode::GetAddressResource:
        case IrOpcode::GetBufferResource:
            for (std::size_t i = 0; i < value->ArgumentCount(); ++i) {
                if (!Evaluable(value->Argument(i), depth - 1u)) return false;
            }
            return true;
        default:
            return false;
    }
}

std::uint32_t ConstantPart(IrValue* offset) {
    offset = offset->Resolve();
    std::uint32_t constant = 0;
    if (Immediate(offset, constant)) return constant;
    if (offset->Opcode() == IrOpcode::IAdd32 && offset->ArgumentCount() == 2u) {
        if (Immediate(offset->Argument(0), constant) || Immediate(offset->Argument(1), constant)) return constant;
    }
    return 0u;
}

bool SplitOffset(IrValue* offset, IrValue*& selector, std::uint32_t& constant) {
    offset = offset->Resolve();
    constant = 0;
    if (offset->Opcode() == IrOpcode::IAdd32 && offset->ArgumentCount() == 2u) {
        if (Immediate(offset->Argument(0), constant)) {
            offset = offset->Argument(1)->Resolve();
        } else if (Immediate(offset->Argument(1), constant)) {
            offset = offset->Argument(0)->Resolve();
        } else {
            return false;
        }
    }
    std::uint32_t factor = 0;
    if (offset->Opcode() == IrOpcode::ShiftLeftLogical32 && offset->ArgumentCount() == 2u && Immediate(offset->Argument(1), factor) && factor == 4u) {
        selector = offset->Argument(0)->Resolve();
        return true;
    }
    if (offset->Opcode() == IrOpcode::IMul32 && offset->ArgumentCount() == 2u) {
        if (Immediate(offset->Argument(1), factor) && factor == DescriptorBytes) {
            selector = offset->Argument(0)->Resolve();
            return true;
        }
        if (Immediate(offset->Argument(0), factor) && factor == DescriptorBytes) {
            selector = offset->Argument(1)->Resolve();
            return true;
        }
    }
    return false;
}

struct IndirectHandle {
    IrValue* base = nullptr;
    IrValue* selector = nullptr;
    std::uint32_t argumentConstant = 0;
    std::uint32_t tableOffset = 0;
    std::array<IrValue*, 4> loads{};
};

bool MatchIndirectHandle(const IrProgram& program, IrValue* handle, IndirectHandle& match) {
    handle = handle->Resolve();
    if (handle->Opcode() != IrOpcode::GetBufferResource || handle->ArgumentCount() != 4u) return false;
    IrValue* offset = nullptr;
    std::uint32_t firstOffset = 0;
    for (std::uint32_t dword = 0; dword < 4u; ++dword) {
        IrValue* load = handle->Argument(dword)->Resolve();
        if (load->Opcode() != IrOpcode::LoadAddressU32 || load->ArgumentCount() != 4u) return false;
        const auto index = load->Flags<MemoryFlags>().index;
        if (index >= program.Resources().memoryInfo.size()) return false;
        const auto memoryOffset = program.Resources().memoryInfo[index].offset;
        if (dword == 0u) {
            match.base = load->Argument(0)->Resolve();
            offset = load->Argument(1)->Resolve();
            firstOffset = memoryOffset;
        } else if (load->Argument(0)->Resolve() != match.base || load->Argument(1)->Resolve() != offset || memoryOffset != firstOffset + dword * 4u) {
            return false;
        }
        match.loads[dword] = load;
    }
    if (Evaluable(offset, 12u)) return false;
    if (!SplitOffset(offset, match.selector, match.argumentConstant)) return false;
    match.tableOffset = match.argumentConstant + firstOffset;
    return true;
}

std::uint32_t CandidateCount(IrProgram& program, const IndirectHandle& match) {
    std::uint32_t limit = std::numeric_limits<std::uint32_t>::max();
    for (const auto& block : program.Blocks()) {
        for (IrValue* inst : block->Instructions()) {
            if (inst->Opcode() != IrOpcode::LoadAddressU32 || inst->ArgumentCount() < 2u || !EquivalentValue(program.Resources(), inst->Argument(0)->Resolve(), match.base)) continue;
            const auto index = inst->Flags<MemoryFlags>().index;
            if (index >= program.Resources().memoryInfo.size()) continue;
            const auto start = ConstantPart(inst->Argument(1)) + program.Resources().memoryInfo[index].offset;
            if (start >= match.tableOffset + DescriptorBytes) limit = std::min(limit, start);
        }
    }
    std::uint32_t count = limit == std::numeric_limits<std::uint32_t>::max() ? DefaultCandidates : (limit - match.tableOffset) / DescriptorBytes;
    std::uint32_t mask = 0;
    IrValue* selector = match.selector->Resolve();
    if (selector->Opcode() == IrOpcode::BitwiseAnd32 && selector->ArgumentCount() == 2u && (Immediate(selector->Argument(0), mask) || Immediate(selector->Argument(1), mask)) && mask < MaxCandidates) {
        count = std::min(count, mask + 1u);
    }
    return std::min(count, MaxCandidates);
}

bool IsOrderedCompare(IrOpcode opcode) {
    switch (opcode) {
        case IrOpcode::SLessThan32:
        case IrOpcode::ULessThan32:
        case IrOpcode::SLessThanEqual32:
        case IrOpcode::ULessThanEqual32:
        case IrOpcode::SGreaterThan32:
        case IrOpcode::UGreaterThan32:
        case IrOpcode::SGreaterThanEqual32:
        case IrOpcode::UGreaterThanEqual32:
            return true;
        default:
            return false;
    }
}

bool MatchSelectorBound(IrValue* condition, IrValue* selector, bool holds, SelectorBound& bound) {
    condition = condition->Resolve();
    while (condition->Opcode() == IrOpcode::LogicalNot && condition->ArgumentCount() == 1u) {
        condition = condition->Argument(0)->Resolve();
        holds = !holds;
    }
    if (!IsOrderedCompare(condition->Opcode()) || condition->ArgumentCount() != 2u) return false;
    for (std::uint32_t argument = 0; argument < 2u; ++argument) {
        if (condition->Argument(argument)->Resolve() == selector && condition->Argument(1u - argument)->Resolve() != selector) {
            bound = SelectorBound{condition, argument, holds};
            return true;
        }
    }
    return false;
}

const BlockInfo* InfoOf(const IrProgram& program, const IrBlock* block) {
    const auto& order = program.BlockOrder();
    const auto& info = program.Metadata().blockInfo;
    for (std::size_t index = 0; index < order.size() && index < info.size(); ++index) {
        if (order[index] == block) return &info[index];
    }
    return nullptr;
}

std::uint32_t FindSelectorBound(IrProgram& program, IrBlock& block, IrValue* selector) {
    IrBlock* current = &block;
    for (std::size_t step = 0; step < program.BlockOrder().size() && current->Predecessors().size() == 1u; ++step) {
        IrBlock* predecessor = current->Predecessors().front();
        const BlockInfo* branch = InfoOf(program, predecessor);
        const BlockInfo* target = InfoOf(program, current);
        if (predecessor == current || branch == nullptr || target == nullptr) return NoSelectorBound;
        if (branch->terminator.kind == TerminatorKind::ConditionalBranch) {
            const bool taken = branch->terminator.trueBlock == target->id;
            if (taken == (branch->terminator.falseBlock == target->id)) return NoSelectorBound;
            SelectorBound bound;
            if (branch->condition != nullptr && MatchSelectorBound(branch->condition, selector, taken, bound)) {
                auto& bounds = program.Resources().selectorBounds;
                const auto found = std::find(bounds.begin(), bounds.end(), bound);
                if (found != bounds.end()) return static_cast<std::uint32_t>(found - bounds.begin());
                bounds.push_back(bound);
                return static_cast<std::uint32_t>(bounds.size() - 1u);
            }
        }
        current = predecessor;
    }
    return NoSelectorBound;
}

bool ExpandAccess(IrProgram& program, IrBuilder& builder, IrBlock& block, IrValue* access) {
    if (access->ArgumentCount() < 2u || access->Argument(access->ArgumentCount() - 1u)->Resolve()->Type() != IrType::Bool) return false;
    if (access->Type() != IrType::Void && access->Type() != IrType::U32) return false;
    IndirectHandle match;
    if (!MatchIndirectHandle(program, access->Argument(0), match)) return false;
    const auto count = CandidateCount(program, match);
    if (count == 0u) return false;
    const auto accessFlags = access->Flags<MemoryFlags>();
    if (accessFlags.index >= program.Resources().memoryInfo.size()) return false;
    const auto selectorBound = FindSelectorBound(program, block, match.selector);
    auto& memoryInfo = program.Resources().memoryInfo;
    IrValue* handle = access->Argument(0)->Resolve();
    IrValue* predicate = access->Argument(access->ArgumentCount() - 1u);
    const auto insert = [&](IrValue& value) {
        block.InsertInstructionBefore(access, &value);
        return &value;
    };
    std::vector<IrValue*> results;
    std::vector<IrValue*> selected;
    for (std::uint32_t candidate = 0; candidate < count; ++candidate) {
        std::array<IrValue*, 4> dwords{};
        for (std::uint32_t dword = 0; dword < 4u; ++dword) {
            IrValue* original = match.loads[dword];
            const auto flags = original->Flags<MemoryFlags>();
            memoryInfo.push_back(memoryInfo.at(flags.index));
            IrValue& load = program.CreateValue(IrOpcode::LoadAddressU32, original->Type(), PackFlags(MemoryFlags{static_cast<std::uint32_t>(memoryInfo.size() - 1u), flags.pc}));
            load.AddArgument(match.base);
            load.AddArgument(&builder.Constant(match.argumentConstant + candidate * DescriptorBytes));
            load.AddArgument(original->Argument(2));
            load.AddArgument(original->Argument(3));
            dwords[dword] = insert(load);
        }
        IrValue& candidateHandle = program.CreateValue(IrOpcode::GetBufferResource, handle->Type());
        for (auto* dword : dwords) candidateHandle.AddArgument(dword);
        insert(candidateHandle);
        IrValue& equal = program.CreateValue(IrOpcode::IEqual32, IrType::Bool);
        equal.AddArgument(match.selector);
        equal.AddArgument(&builder.Constant(candidate));
        selected.push_back(insert(equal));
        IrValue& enabled = program.CreateValue(IrOpcode::LogicalAnd, IrType::Bool);
        enabled.AddArgument(predicate);
        enabled.AddArgument(&equal);
        insert(enabled);
        MemoryInfo info = memoryInfo.at(accessFlags.index);
        info.indirectCandidate = true;
        info.candidate = candidate;
        info.selectorBound = selectorBound;
        memoryInfo.push_back(info);
        IrValue& clone = program.CreateValue(access->Opcode(), access->Type(), PackFlags(MemoryFlags{static_cast<std::uint32_t>(memoryInfo.size() - 1u), accessFlags.pc}));
        clone.AddArgument(&candidateHandle);
        for (std::size_t argument = 1; argument + 1u < access->ArgumentCount(); ++argument) clone.AddArgument(access->Argument(argument));
        clone.AddArgument(&enabled);
        results.push_back(insert(clone));
    }
    if (access->Type() != IrType::Void) {
        IrValue* merged = results.front();
        for (std::uint32_t candidate = 1; candidate < count; ++candidate) {
            IrValue& select = program.CreateValue(IrOpcode::SelectU32, IrType::U32);
            select.AddArgument(selected[candidate]);
            select.AddArgument(results[candidate]);
            select.AddArgument(merged);
            merged = insert(select);
        }
        access->ReplaceAllUsesWith(merged);
    }
    access->Invalidate();
    block.RemoveInstruction(access);
    return true;
}

}

std::uint32_t IndirectBufferExpander::Expand(IrProgram& program) const {
    IrBuilder builder(program);
    std::uint32_t expanded = 0;
    for (auto& block : program.Blocks()) {
        std::vector<IrValue*> accesses;
        for (IrValue* inst : block->Instructions()) {
            const auto access = BufferAccessOf(inst->Opcode());
            if (access != BufferAccess::None && inst->Opcode() != IrOpcode::ReadConstBuffer) accesses.push_back(inst);
        }
        for (IrValue* access : accesses) {
            if (ExpandAccess(program, builder, *block, access)) ++expanded;
        }
    }
    return expanded;
}

}

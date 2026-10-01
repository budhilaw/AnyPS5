#ifndef CORE_SHADER_RECOMPILER_TESTS_SYNTHETICPROGRAMS_HPP
#define CORE_SHADER_RECOMPILER_TESTS_SYNTHETICPROGRAMS_HPP

#include "../Recompiler.hpp"
#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace ShaderRecompiler::SyntheticPrograms {

inline constexpr std::uint64_t CodeAddress = 0x20000u;
inline constexpr std::uint32_t Literal = 0xffu;
inline constexpr std::uint32_t ExecLo = 126u;
inline constexpr std::uint32_t VccLo = 106u;
inline constexpr std::uint32_t InlineZero = 0x80u;
inline constexpr std::uint32_t EndProgram = 0xbf810000u;
inline constexpr std::uint32_t XorMask = 0x5a5a0000u;
inline constexpr std::uint32_t BranchBonus = 1000u;
inline constexpr std::uint32_t BufferFields = 0x00000facu;

constexpr std::uint32_t Vgpr(std::uint32_t reg) {
    return 256u + reg;
}

constexpr std::uint32_t InlineInteger(std::uint32_t value) {
    return 0x80u + value;
}

constexpr std::uint32_t Vop1(std::uint32_t op, std::uint32_t vdst, std::uint32_t src0) {
    return 0x7e000000u | (vdst << 17u) | (op << 9u) | src0;
}

constexpr std::uint32_t Vop2(std::uint32_t op, std::uint32_t vdst, std::uint32_t src0, std::uint32_t vsrc1) {
    return (op << 25u) | (vdst << 17u) | (vsrc1 << 9u) | src0;
}

constexpr std::uint32_t Vopc(std::uint32_t op, std::uint32_t src0, std::uint32_t vsrc1) {
    return 0x7c000000u | (op << 17u) | (vsrc1 << 9u) | src0;
}

constexpr std::uint32_t Sop1(std::uint32_t op, std::uint32_t sdst, std::uint32_t ssrc0) {
    return 0xbe800000u | (sdst << 16u) | (op << 8u) | ssrc0;
}

constexpr std::uint32_t Sop2(std::uint32_t op, std::uint32_t sdst, std::uint32_t ssrc0, std::uint32_t ssrc1) {
    return 0x80000000u | (op << 23u) | (sdst << 16u) | (ssrc1 << 8u) | ssrc0;
}

constexpr std::uint32_t Sopp(std::uint32_t op, std::uint32_t simm) {
    return 0xbf800000u | (op << 16u) | (simm & 0xffffu);
}

constexpr std::array<std::uint32_t, 2> Mubuf(std::uint32_t op, bool offen, bool idxen, std::uint32_t offset, std::uint32_t vaddr, std::uint32_t vdata, std::uint32_t resourceSgpr) {
    return {0xe0000000u | (op << 18u) | (idxen ? 1u << 13u : 0u) | (offen ? 1u << 12u : 0u) | offset, (InlineZero << 24u) | ((resourceSgpr / 4u) << 16u) | (vdata << 8u) | vaddr};
}

constexpr std::array<std::uint32_t, 2> Ds(std::uint32_t op, std::uint32_t address, std::uint32_t data, std::uint32_t vdst) {
    return {0xd8000000u | (op << 18u), (vdst << 24u) | (data << 8u) | address};
}

constexpr std::array<std::uint32_t, 2> Smem(std::uint32_t op, std::uint32_t sdata, std::uint32_t sbase, std::uint32_t soffset, std::uint32_t offset) {
    return {0xf4000000u | (op << 18u) | (sdata << 6u) | (sbase / 2u), (soffset << 25u) | offset};
}

constexpr std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address, std::uint32_t stride, std::uint32_t records) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (stride << 16u), records, BufferFields};
}

namespace Op {
inline constexpr std::uint32_t SLoadDword = 0x00u;
inline constexpr std::uint32_t SLoadDwordx2 = 0x01u;
inline constexpr std::uint32_t SLoadDwordx4 = 0x02u;
inline constexpr std::uint32_t SLshlB32 = 0x1eu;
inline constexpr std::uint32_t VMovB32 = 0x01u;
inline constexpr std::uint32_t VReadfirstlaneB32 = 0x02u;
inline constexpr std::uint32_t VMulU32U24 = 0x0bu;
inline constexpr std::uint32_t VLshlrevB32 = 0x1au;
inline constexpr std::uint32_t VXorB32 = 0x1du;
inline constexpr std::uint32_t VAddNcU32 = 0x25u;
inline constexpr std::uint32_t VCmpGtU32 = 0xc4u;
inline constexpr std::uint32_t VCmpxGtU32 = 0xd4u;
inline constexpr std::uint32_t SMovB64 = 0x04u;
inline constexpr std::uint32_t SAndB64 = 0x0fu;
inline constexpr std::uint32_t SOrB64 = 0x11u;
inline constexpr std::uint32_t SCbranchVccz = 0x06u;
inline constexpr std::uint32_t SCbranchExecz = 0x08u;
inline constexpr std::uint32_t SBarrier = 0x0au;
inline constexpr std::uint32_t BufferLoadDword = 0x0cu;
inline constexpr std::uint32_t BufferLoadDwordx2 = 0x0du;
inline constexpr std::uint32_t BufferLoadDwordx4 = 0x0eu;
inline constexpr std::uint32_t BufferLoadDwordx3 = 0x0fu;
inline constexpr std::uint32_t BufferStoreDword = 0x1cu;
inline constexpr std::uint32_t BufferStoreDwordx2 = 0x1du;
inline constexpr std::uint32_t BufferStoreDwordx4 = 0x1eu;
inline constexpr std::uint32_t BufferStoreDwordx3 = 0x1fu;
inline constexpr std::uint32_t DsWriteB32 = 0x0du;
inline constexpr std::uint32_t DsReadB32 = 0x36u;
}

class Assembler {
public:
    void Emit(std::initializer_list<std::uint32_t> words) {
        code.insert(code.end(), words);
    }

    template<std::size_t TCount>
    void Emit(const std::array<std::uint32_t, TCount>& words) {
        code.insert(code.end(), words.begin(), words.end());
    }

    std::size_t BranchPlaceholder(std::uint32_t op) {
        code.push_back(Sopp(op, 0u));
        return code.size() - 1u;
    }

    void BindBranchHere(std::size_t branch) {
        code[branch] = Sopp((code[branch] >> 16u) & 0x7fu, static_cast<std::uint32_t>(code.size() - branch - 1u));
    }

    std::vector<std::uint32_t> code;
};

struct ComputeProgram {
    std::vector<std::uint32_t> code;
    std::vector<std::uint32_t> userData;
    std::array<std::uint32_t, 3> numThreads {64u, 1u, 1u};
    std::uint32_t ldsSizeDwords = 0;

    [[nodiscard]] std::uint32_t ThreadsPerGroup() const {
        return numThreads[0] * numThreads[1] * numThreads[2];
    }

    [[nodiscard]] RecompileRequest Request(std::uint32_t waveSize, std::uint32_t subgroupSize, bool dualLane) const {
        RecompileRequest request {};
        request.shader = {ShaderStage::Compute, CodeAddress, code, 0u, {}};
        request.context.waveSize = waveSize;
        request.context.userDataBaseRegister = 0u;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo {numThreads, ldsSizeDwords, {true, false, false}, false, 3u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
        request.target.maxWorkgroupInvocations = 1024u;
        request.target.maxWorkgroupSharedMemoryBytes = 65536u;
        request.target.dualLaneWave64 = dualLane;
        request.layout.pushConstantSizeBytes = 128u;
        request.useCache = false;
        return request;
    }
};

inline void EmitFlatThreadIndex(Assembler& assembler, const std::array<std::uint32_t, 3>& numThreads) {
    const std::uint32_t threadsPerGroup = numThreads[0] * numThreads[1] * numThreads[2];
    assembler.Emit({Vop2(Op::VMulU32U24, 3u, Literal, 1u), numThreads[0]});
    assembler.Emit({Vop2(Op::VMulU32U24, 4u, Literal, 2u), numThreads[0] * numThreads[1]});
    assembler.Emit({Vop2(Op::VAddNcU32, 3u, Vgpr(0u), 3u)});
    assembler.Emit({Vop2(Op::VAddNcU32, 3u, Vgpr(4u), 3u)});
    assembler.Emit({Vop1(Op::VMovB32, 5u, Literal), threadsPerGroup});
    assembler.Emit({Vop2(Op::VMulU32U24, 5u, 8u, 5u)});
    assembler.Emit({Vop2(Op::VAddNcU32, 3u, Vgpr(5u), 3u)});
}

inline ComputeProgram PerThreadProgram(const std::array<std::uint32_t, 3>& numThreads, std::uint32_t count, std::uint32_t threshold, std::uint64_t input, std::uint64_t output, std::uint32_t records) {
    Assembler assembler;
    EmitFlatThreadIndex(assembler, numThreads);
    assembler.Emit({Vopc(Op::VCmpxGtU32, Literal, 3u), count});
    const auto skipAll = assembler.BranchPlaceholder(Op::SCbranchExecz);
    assembler.Emit(Mubuf(Op::BufferLoadDword, false, true, 0u, 3u, 6u, 0u));
    assembler.Emit({Vop2(Op::VAddNcU32, 6u, Vgpr(3u), 6u)});
    assembler.Emit({Vop2(Op::VXorB32, 6u, Literal, 6u), XorMask});
    assembler.Emit({Vopc(Op::VCmpGtU32, Literal, 3u), threshold});
    assembler.Emit({Sop2(Op::SAndB64, 12u, ExecLo, ExecLo)});
    assembler.Emit({Sop2(Op::SAndB64, ExecLo, ExecLo, VccLo)});
    const auto skipBonus = assembler.BranchPlaceholder(Op::SCbranchExecz);
    assembler.Emit({Vop2(Op::VAddNcU32, 6u, Literal, 6u), BranchBonus});
    assembler.BindBranchHere(skipBonus);
    assembler.Emit({Sop2(Op::SOrB64, ExecLo, ExecLo, 12u)});
    assembler.Emit(Mubuf(Op::BufferStoreDword, false, true, 0u, 3u, 6u, 4u));
    assembler.BindBranchHere(skipAll);
    assembler.Emit({EndProgram});
    const auto in = BufferDescriptor(input, 4u, records);
    const auto out = BufferDescriptor(output, 4u, records);
    return {assembler.code, {in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3]}, numThreads};
}

inline std::uint32_t PerThreadExpected(std::uint32_t index, std::uint32_t input, std::uint32_t threshold) {
    const std::uint32_t mixed = (input + index) ^ XorMask;
    return index < threshold ? mixed + BranchBonus : mixed;
}

inline ComputeProgram VccBranchProgram(const std::array<std::uint32_t, 3>& numThreads, std::uint32_t threshold, std::uint64_t input, std::uint64_t output, std::uint32_t records) {
    Assembler assembler;
    EmitFlatThreadIndex(assembler, numThreads);
    assembler.Emit(Mubuf(Op::BufferLoadDword, false, true, 0u, 3u, 6u, 0u));
    assembler.Emit({Vop2(Op::VAddNcU32, 6u, Vgpr(3u), 6u)});
    assembler.Emit({Vop2(Op::VXorB32, 6u, Literal, 6u), XorMask});
    assembler.Emit({Vopc(Op::VCmpGtU32, Literal, 3u), threshold});
    const auto skipBonus = assembler.BranchPlaceholder(Op::SCbranchVccz);
    assembler.Emit({Vop2(Op::VAddNcU32, 6u, Literal, 6u), BranchBonus});
    assembler.BindBranchHere(skipBonus);
    assembler.Emit(Mubuf(Op::BufferStoreDword, false, true, 0u, 3u, 6u, 4u));
    assembler.Emit({EndProgram});
    const auto in = BufferDescriptor(input, 4u, records);
    const auto out = BufferDescriptor(output, 4u, records);
    return {assembler.code, {in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3]}, numThreads};
}

inline std::uint32_t VccBranchExpected(std::uint32_t index, std::uint32_t input, std::uint32_t threshold, std::uint32_t threadsPerGroup) {
    const std::uint32_t mixed = (input + index) ^ XorMask;
    const std::uint32_t firstLaneOfWave = index - index % threadsPerGroup % 64u;
    return firstLaneOfWave < threshold ? mixed + BranchBonus : mixed;
}

inline ComputeProgram BarrierProgram(std::uint64_t input, std::uint64_t output, std::uint32_t records) {
    Assembler assembler;
    EmitFlatThreadIndex(assembler, {128u, 1u, 1u});
    assembler.Emit(Mubuf(Op::BufferLoadDword, false, true, 0u, 3u, 6u, 0u));
    assembler.Emit({Sopp(Op::SBarrier, 0u)});
    assembler.Emit(Mubuf(Op::BufferStoreDword, false, true, 0u, 3u, 6u, 4u));
    assembler.Emit({EndProgram});
    const auto in = BufferDescriptor(input, 4u, records);
    const auto out = BufferDescriptor(output, 4u, records);
    return {assembler.code, {in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3]}, {128u, 1u, 1u}};
}

inline ComputeProgram ReadFirstLaneProgram(std::uint64_t input, std::uint64_t output, std::uint32_t records) {
    Assembler assembler;
    EmitFlatThreadIndex(assembler, {64u, 1u, 1u});
    assembler.Emit(Mubuf(Op::BufferLoadDword, false, true, 0u, 3u, 6u, 0u));
    assembler.Emit({Vop1(Op::VReadfirstlaneB32, 9u, Vgpr(6u))});
    assembler.Emit({Vop2(Op::VAddNcU32, 6u, 9u, 6u)});
    assembler.Emit(Mubuf(Op::BufferStoreDword, false, true, 0u, 3u, 6u, 4u));
    assembler.Emit({EndProgram});
    const auto in = BufferDescriptor(input, 4u, records);
    const auto out = BufferDescriptor(output, 4u, records);
    return {assembler.code, {in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3]}, {64u, 1u, 1u}};
}

inline ComputeProgram ExecMaskProgram(std::uint64_t input, std::uint64_t output, std::uint32_t records) {
    Assembler assembler;
    EmitFlatThreadIndex(assembler, {64u, 1u, 1u});
    assembler.Emit(Mubuf(Op::BufferLoadDword, false, true, 0u, 3u, 6u, 0u));
    assembler.Emit({Sop1(Op::SMovB64, 10u, ExecLo)});
    assembler.Emit({Vop2(Op::VAddNcU32, 6u, 11u, 6u)});
    assembler.Emit(Mubuf(Op::BufferStoreDword, false, true, 0u, 3u, 6u, 4u));
    assembler.Emit({EndProgram});
    const auto in = BufferDescriptor(input, 4u, records);
    const auto out = BufferDescriptor(output, 4u, records);
    return {assembler.code, {in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3]}, {64u, 1u, 1u}};
}

inline ComputeProgram SharedMemoryProgram(std::uint64_t input, std::uint64_t output, std::uint32_t records) {
    Assembler assembler;
    EmitFlatThreadIndex(assembler, {64u, 1u, 1u});
    assembler.Emit(Mubuf(Op::BufferLoadDword, false, true, 0u, 3u, 6u, 0u));
    assembler.Emit({Vop2(Op::VLshlrevB32, 7u, InlineInteger(2u), 0u)});
    assembler.Emit(Ds(Op::DsWriteB32, 7u, 6u, 0u));
    assembler.Emit(Ds(Op::DsReadB32, 7u, 0u, 6u));
    assembler.Emit(Mubuf(Op::BufferStoreDword, false, true, 0u, 3u, 6u, 4u));
    assembler.Emit({EndProgram});
    const auto in = BufferDescriptor(input, 4u, records);
    const auto out = BufferDescriptor(output, 4u, records);
    ComputeProgram program {assembler.code, {in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3]}, {64u, 1u, 1u}};
    program.ldsSizeDwords = 64u;
    return program;
}

inline constexpr std::uint32_t ScalarAddressOutputDwords = 12u;

inline ComputeProgram ScalarAddressLoadProgram(std::uint64_t left, std::uint64_t right, std::uint64_t output, std::uint32_t records) {
    Assembler assembler;
    EmitFlatThreadIndex(assembler, {64u, 1u, 1u});
    assembler.Emit({Vop1(Op::VReadfirstlaneB32, 10u, Vgpr(0u))});
    assembler.Emit({Sop2(Op::SLshlB32, 11u, 10u, InlineInteger(4u))});
    assembler.Emit(Smem(Op::SLoadDwordx4, 12u, 0u, 11u, 0u));
    assembler.Emit(Smem(Op::SLoadDwordx4, 16u, 2u, 11u, 0u));
    assembler.Emit(Smem(Op::SLoadDwordx2, 20u, 0u, 11u, 16u));
    assembler.Emit(Smem(Op::SLoadDword, 22u, 2u, 11u, 20u));
    assembler.Emit({Vop2(Op::VMulU32U24, 9u, Literal, 3u), ScalarAddressOutputDwords * 4u});
    for (std::uint32_t store = 0; store < 3u; ++store) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            assembler.Emit({Vop1(Op::VMovB32, 12u + component, store == 2u && component == 3u ? 10u : 12u + store * 4u + component)});
        }
        assembler.Emit(Mubuf(Op::BufferStoreDwordx4, true, false, store * 16u, 9u, 12u, 4u));
    }
    assembler.Emit({EndProgram});
    const auto out = BufferDescriptor(output, 0u, records);
    return {assembler.code, {static_cast<std::uint32_t>(left), static_cast<std::uint32_t>(left >> 32u), static_cast<std::uint32_t>(right), static_cast<std::uint32_t>(right >> 32u), out[0], out[1], out[2], out[3]}, {64u, 1u, 1u}};
}

inline std::array<std::uint32_t, ScalarAddressOutputDwords> ScalarAddressLoadExpected(std::uint32_t (*left)(std::size_t), std::uint32_t (*right)(std::size_t)) {
    return {left(0), left(1), left(2), left(3), right(0), right(1), right(2), right(3), left(4), left(5), right(5), 0u};
}

inline ComputeProgram WideLoadProgram(std::uint32_t dwords, const std::array<std::uint32_t, 3>& numThreads, std::uint32_t loadStrideBytes, std::uint32_t instructionOffset, std::uint64_t input, std::uint32_t inputBytes, std::uint64_t output, std::uint32_t outputBytes) {
    const std::uint32_t load = dwords == 2u ? Op::BufferLoadDwordx2 : dwords == 3u ? Op::BufferLoadDwordx3 : Op::BufferLoadDwordx4;
    const std::uint32_t store = dwords == 2u ? Op::BufferStoreDwordx2 : dwords == 3u ? Op::BufferStoreDwordx3 : Op::BufferStoreDwordx4;
    Assembler assembler;
    EmitFlatThreadIndex(assembler, numThreads);
    assembler.Emit({Vop2(Op::VMulU32U24, 10u, Literal, 3u), loadStrideBytes});
    assembler.Emit(Mubuf(load, true, false, instructionOffset, 10u, 12u, 0u));
    assembler.Emit({Vop2(Op::VLshlrevB32, 11u, InlineInteger(4u), 3u)});
    assembler.Emit(Mubuf(store, true, false, 0u, 11u, 12u, 4u));
    assembler.Emit({EndProgram});
    const auto in = BufferDescriptor(input, 0u, inputBytes);
    const auto out = BufferDescriptor(output, 0u, outputBytes);
    return {assembler.code, {in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3]}, numThreads};
}

}

#endif

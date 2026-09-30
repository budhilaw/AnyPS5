#include "prx/libSceAgc/Misc/include/ShaderFusion.hpp"

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

struct SizeAlign {
 uint64_t m_size;
 size_t m_align;
};

namespace {

using ShaderRegs::ShaderBinaryType;

struct FusionLayout {
    ShaderBinaryType fused;
    std::uint32_t frontProgram;
    std::uint32_t resources;
    std::uint32_t checksum;
};

FusionLayout Layout(const Shader* front, const Shader* back, const char* function) {
    if (front == nullptr || back == nullptr) throw std::invalid_argument(std::string(function) + ": shader half is null");
    if (front->type == static_cast<std::uint8_t>(ShaderBinaryType::GsFront) && back->type == static_cast<std::uint8_t>(ShaderBinaryType::GsBack))
        return {ShaderBinaryType::Gs, ShaderRegs::SPI_SHADER_PGM_LO_ES, ShaderRegs::SPI_SHADER_PGM_LO_GS + 2, ShaderRegs::SPI_SHADER_PGM_LO_GS - 8};
    if (front->type == static_cast<std::uint8_t>(ShaderBinaryType::HsFront) && back->type == static_cast<std::uint8_t>(ShaderBinaryType::HsBack))
        return {ShaderBinaryType::Hs, ShaderRegs::SPI_SHADER_PGM_LO_LS, ShaderRegs::SPI_SHADER_PGM_LO_HS + 2, ShaderRegs::SPI_SHADER_PGM_LO_HS - 8};
    throw std::invalid_argument(std::string(function) + ": shader halves are not a front and back pair of the same stage");
}

const ShaderRegister* Find(const ShaderRegister* registers, std::uint32_t count, std::uint32_t offset) {
    for (std::uint32_t index = 0; index < count; ++index)
        if (registers[index].offset == offset) return &registers[index];
    return nullptr;
}

std::uint32_t MergeResources(std::uint32_t back, std::uint32_t front) {
    const auto vgprs = std::max(back & 0x3fu, front & 0x3fu);
    const auto sgprs = std::max((back >> 6u) & 0xfu, (front >> 6u) & 0xfu);
    return (back & ~0x3ffu) | (sgprs << 6u) | vgprs;
}

std::uint32_t Merge(ShaderRegister* output, const ShaderRegister* back, std::uint32_t backCount, const ShaderRegister* front, std::uint32_t frontCount, const FusionLayout& layout, std::uint64_t frontCode) {
    std::vector<std::uint32_t> checksums;
    for (std::uint32_t index = 0; index < frontCount; ++index)
        if (front[index].offset == layout.checksum) checksums.push_back(front[index].value);
    std::size_t nextChecksum = 0;
    std::uint32_t count = 0;
    for (std::uint32_t index = 0; index < backCount; ++index) {
        auto entry = back[index];
        if (entry.offset == layout.frontProgram) entry.value = static_cast<std::uint32_t>(frontCode >> 8u);
        else if (entry.offset == layout.frontProgram + 1) entry.value = static_cast<std::uint32_t>((frontCode >> 40u) & 0xffu);
        else if (entry.offset == layout.checksum && entry.value == 0 && nextChecksum < checksums.size()) entry.value = checksums[nextChecksum++];
        else if (entry.offset == layout.resources) {
            if (const auto* resources = Find(front, frontCount, layout.resources); resources != nullptr) entry.value = MergeResources(entry.value, resources->value);
        }
        output[count++] = entry;
    }
    for (std::uint32_t index = 0; index < frontCount; ++index) {
        const auto& entry = front[index];
        if (entry.offset == layout.checksum || Find(back, backCount, entry.offset) != nullptr) continue;
        output[count++] = entry;
    }
    return count;
}

}

extern "C" {

APS5_EXPORT("fd5Bp5tGTgo", sceAgcUnknownFuseShaderHalves);
int APS5_VABI sceAgcUnknownFuseShaderHalves(Shader* fused_result, const Shader* front, const Shader* back, void* scratch_mem) {
    const auto layout = Layout(front, back, __func__);
    if (fused_result == nullptr || scratch_mem == nullptr) throw std::invalid_argument(std::string(__func__) + ": result or scratch memory is null");
    const auto frontCode = reinterpret_cast<std::uint64_t>(front->code);
    auto* registers = static_cast<ShaderRegister*>(scratch_mem);
    const auto shCount = Merge(registers, back->sh_registers, back->num_sh_registers, front->sh_registers, front->num_sh_registers, layout, frontCode);
    auto* cxRegisters = registers + shCount;
    const auto cxCount = Merge(cxRegisters, back->cx_registers, back->num_cx_registers, front->cx_registers, front->num_cx_registers, layout, frontCode);
    if (shCount > 0xffu || cxCount > 0xffu) throw std::runtime_error(std::string(__func__) + ": fused register list is too long");
    *fused_result = *back;
    fused_result->type = static_cast<std::uint8_t>(layout.fused);
    fused_result->code = front->code;
    fused_result->user_data = nullptr;
    fused_result->input_semantics = front->input_semantics;
    fused_result->num_input_semantics = front->num_input_semantics;
    fused_result->sh_registers = shCount != 0 ? registers : nullptr;
    fused_result->num_sh_registers = static_cast<std::uint8_t>(shCount);
    fused_result->cx_registers = cxCount != 0 ? cxRegisters : nullptr;
    fused_result->num_cx_registers = static_cast<std::uint8_t>(cxCount);
    return 0;
}

APS5_EXPORT("dolOmWH+huQ", sceAgcUnknownGetFusedShaderSize);
int APS5_VABI sceAgcUnknownGetFusedShaderSize(SizeAlign* dst, const Shader* front, const Shader* back) {
    Layout(front, back, __func__);
    if (dst == nullptr) throw std::invalid_argument(std::string(__func__) + ": size is null");
    const auto registers = static_cast<std::uint64_t>(front->num_sh_registers) + back->num_sh_registers + front->num_cx_registers + back->num_cx_registers;
    dst->m_size = registers * sizeof(ShaderRegister);
    dst->m_align = alignof(ShaderRegister);
    return 0;
}

APS5_EXPORT("k0E7vkgqAuE", sceAgcCreateInterpolantMappingVsPs);
int APS5_VABI sceAgcCreateInterpolantMappingVsPs(ShaderRegister* regs, const Shader* vs, const Shader* ps) {
    (void)regs;
    (void)vs;
    (void)ps;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

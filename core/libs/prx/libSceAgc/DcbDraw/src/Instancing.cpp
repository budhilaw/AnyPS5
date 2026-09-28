#include "prx/libSceAgc/Command/include/Draw.hpp"
#include "prx/libSceAgc/DcbDraw/include/Instancing.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetNumInstances(CommandBuffer* buf, std::uint32_t numInstances) {
    return Agc::Command::Emit(buf, 0x2fu, {numInstances}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetNumInstancesGetSize() {
    return 8;
}

uint32_t* APS5_VABI sceAgcDcbSetBaseIndirectArgs(CommandBuffer* buf, uint32_t shader_type, const volatile void* indirect_base_addr) {
    Agc::Command::Require(buf != nullptr && indirect_base_addr != nullptr, __func__, "null command buffer or base address");
    Agc::Command::Require(shader_type <= 1, __func__, "invalid indirect base shader type");
    const auto address = reinterpret_cast<std::uintptr_t>(indirect_base_addr);
    Agc::Command::CheckAddress(address, 8, __func__);
    Agc::Command::Require((address >> 32u) <= 0xffffu, __func__, "indirect base address exceeds 48 bits");
    auto* packet = Agc::Command::Emit(buf, 0x11u, {1u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)}, __func__);
    packet[0] |= shader_type << 1u;
    return packet;
}

}

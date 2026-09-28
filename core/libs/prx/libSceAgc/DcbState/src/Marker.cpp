#include <cstring>
#include "prx/libSceAgc/DcbState/include/Marker.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbPopMarker(CommandBuffer* buf) {
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    auto* packet = Agc::Command::Allocate(buf, 2, __func__);
    packet[0] = Agc::Command::Header(0x10u, 2, 0x0cu << 2u);
    packet[1] = 0;
    return packet;
}

uint32_t* APS5_VABI sceAgcDcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color) {
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    (void)color;
    const char* text = str != nullptr ? str : "";
    const auto length = std::strlen(text) + 1;
    const auto payload = static_cast<std::uint32_t>((length + 3) / 4);
    const auto count = 1u + (payload == 0 ? 1u : payload);
    auto* packet = Agc::Command::Allocate(buf, count, __func__);
    packet[0] = Agc::Command::Header(0x10u, count, 0x0bu << 2u);
    std::memset(packet + 1, 0, static_cast<std::size_t>(count - 1) * sizeof(std::uint32_t));
    std::memcpy(packet + 1, text, length);
    return packet;
}

}

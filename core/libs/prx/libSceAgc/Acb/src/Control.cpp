#include <cstring>
#include "prx/libSceAgc/Acb/include/Control.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

// unknown signature
APS5_EXPORT("gQkqkLttcpw", sceAgcAcb_gQkqkLttcpw);
void* APS5_VABI sceAgcAcb_gQkqkLttcpw (void) {
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

std::uint32_t* APS5_VABI sceAgcAcbJump(CommandBuffer* buf, std::uint8_t cachePolicy, const std::uint32_t* target, std::uint32_t sizeInDwords) {
    Agc::Command::Require(buf != nullptr && target != nullptr, __func__, "null command buffer or target");
    Agc::Command::CheckBits(cachePolicy, 3, __func__);
    Agc::Command::Require(sizeInDwords != 0 && sizeInDwords <= 0xfffffu, __func__, "invalid indirect buffer size");
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    Agc::Command::CheckAddress(address, 4, __func__);
    return Agc::Command::Emit(buf, 0x3fu, {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 0x0f900000u | (static_cast<std::uint32_t>(cachePolicy) << 28u) | sizeInDwords}, __func__);
}

uint32_t APS5_VABI sceAgcAcbJumpGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcAcbResetQueue(CommandBuffer* buf, uint32_t op) {
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    Agc::Command::CheckBits(op, 0x1c2u, __func__);
    auto* packet = Agc::Command::Allocate(buf, 2, __func__);
    packet[0] = Agc::Command::Header(0x10u, 2, 0x09u << 2u);
    packet[1] = 0;
    return packet;
}

std::uint32_t* APS5_VABI sceAgcAcbRewind(CommandBuffer* buf, std::uint32_t initialState) {
    (void)buf;
    (void)initialState;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

std::uint32_t APS5_VABI sceAgcAcbRewindGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcAcbWaitUntilSafeForRendering(CommandBuffer* buf, std::uint32_t videoOutHandle, std::uint32_t displayBufferIndex) {
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    auto* packet = Agc::Command::Allocate(buf, 4, __func__);
    packet[0] = Agc::Command::Header(0x10u, 4, 0x06u << 2u);
    packet[1] = videoOutHandle;
    packet[2] = displayBufferIndex;
    packet[3] = 0;
    return packet;
}

std::uint32_t* APS5_VABI sceAgcAcbSetFlip(CommandBuffer* buf, std::uint32_t videoOutHandle, std::int32_t displayBufferIndex, std::uint32_t flipMode, std::int64_t flipArg) {
    (void)buf;
    (void)videoOutHandle;
    (void)displayBufferIndex;
    (void)flipMode;
    (void)flipArg;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

uint32_t* APS5_VABI sceAgcAcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color) {
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

uint32_t* APS5_VABI sceAgcAcbPopMarker(CommandBuffer* buf) {
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    auto* packet = Agc::Command::Allocate(buf, 2, __func__);
    packet[0] = Agc::Command::Header(0x10u, 2, 0x0cu << 2u);
    packet[1] = 0;
    return packet;
}

uint32_t* APS5_VABI sceAgcAcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

}

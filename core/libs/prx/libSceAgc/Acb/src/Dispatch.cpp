#include "prx/libSceAgc/Acb/include/Dispatch.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcAcbDispatchIndirect(CommandBuffer* buf, const volatile void* indirect_args, uint32_t modifier) {
    Agc::Command::Require(buf != nullptr && indirect_args != nullptr, __func__, "null command buffer or arguments");
    const auto address = reinterpret_cast<std::uintptr_t>(indirect_args);
    Agc::Command::CheckAddress(address, 4, __func__);
    return Agc::Command::Emit(buf, 0x16u, {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), (modifier & 0xa038u) | 0x41u}, __func__);
}

std::uint32_t APS5_VABI sceAgcAcbDispatchIndirectGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

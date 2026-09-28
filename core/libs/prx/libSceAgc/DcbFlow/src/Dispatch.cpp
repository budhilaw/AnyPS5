#include "prx/libSceAgc/DcbFlow/include/Dispatch.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbDispatchIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t flags) {
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    Agc::Command::Require((data_offset_in_bytes & 3u) == 0, __func__, "misaligned indirect arguments offset");
    return Agc::Command::Emit(buf, 0x16u, {data_offset_in_bytes, (flags & 0xa038u) | 0x41u}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbDispatchIndirectGetSize() {
    return 12;
}

}

#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDriverSubmitAcb(uint32_t queue, const Packet* packet) {
    if (queue < 0x20 || queue >= 0x58) {
        throw std::runtime_error(std::string(__func__) + ": unsupported compute queue");
    }
    AgcDriver::Submit(packet, queue);
    return 0;
}

int APS5_VABI sceAgcDriverSubmitMultiAcbs(uint32_t queue, uint32_t* const* acbs, const uint32_t* sizes_in_dwords, uint32_t count) {
    if (queue < 0x20 || queue >= 0x58) {
        throw std::runtime_error(std::string(__func__) + ": unsupported compute queue");
    }
    if (count == 0 || acbs == nullptr || sizes_in_dwords == nullptr) throw std::invalid_argument(std::string(__func__) + ": invalid multi-buffer submission");
    std::vector<Packet> packets(count);
    for (std::uint32_t index = 0; index < count; ++index) packets[index] = Packet{acbs[index], sizes_in_dwords[index], 0, {}};
    AgcDriver::Submit(packets.data(), count, queue);
    return 0;
}

}

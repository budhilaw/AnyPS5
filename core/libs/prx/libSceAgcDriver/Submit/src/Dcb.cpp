#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <vector>

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::vector<Packet> Packets(std::uint32_t* const* addresses, const std::uint32_t* sizes, std::uint32_t count) {
    if (count == 0 || addresses == nullptr || sizes == nullptr) throw std::invalid_argument("AGC driver: invalid multi-buffer submission");
    AgcDriver::GuestMemory::CheckRange(addresses, sizeof(*addresses) * count, alignof(std::uint32_t*));
    AgcDriver::GuestMemory::CheckRange(sizes, sizeof(*sizes) * count, alignof(std::uint32_t));
    std::vector<Packet> packets(count);
    for (std::uint32_t index = 0; index < count; ++index) packets[index] = Packet{addresses[index], sizes[index], 0, {}};
    return packets;
}

}

extern "C" {

std::uint32_t APS5_VABI sceAgcDriverGetWaitRenderingPacketSizeInDwords() {
    return AgcDriver::RenderingWaitPacketWords;
}

std::uint32_t APS5_VABI sceAgcDriverWaitUntilSafeForRendering(std::uint32_t** command,
    std::uint32_t capacity, std::uint32_t mode, std::uint32_t handle, int index) {
    if (!command || capacity < AgcDriver::RenderingWaitPacketWords || mode != 0 || index < 0)
        throw std::invalid_argument("AGC driver: invalid rendering wait arguments");
    AgcDriver::GuestMemory::CheckRange(command, sizeof(*command), alignof(std::uint32_t*), true);
    AgcDriver::GuestMemory::CheckRange(*command, AgcDriver::RenderingWaitPacketWords * sizeof(std::uint32_t), alignof(std::uint32_t), true);
    const std::uint32_t words[] = {AgcDriver::RenderingWaitPacketHeader, handle, static_cast<std::uint32_t>(index), mode};
    std::copy(std::begin(words), std::end(words), *command);
    *command += AgcDriver::RenderingWaitPacketWords;
    return 0;
}

int APS5_VABI sceAgcDriverSubmitDcb(const Packet* packet) {
    AgcDriver::Submit(packet, 0);
    return 0;
}

int APS5_VABI sceAgcDriverSubmitMultiDcbs(uint32_t* const* dcb_gpu_addrs, const uint32_t* dcb_sizes_in_dwords, uint32_t count) {
    AgcDriver::Submit(Packets(dcb_gpu_addrs, dcb_sizes_in_dwords, count).data(), count, 0);
    return 0;
}

int APS5_VABI sceAgcDriverAgrSubmitDcb(const Packet* packet) {
    AgcDriver::Submit(packet, 0);
    return 0;
}

int APS5_VABI sceAgcDriverAgrSubmitMultiDcbs(std::uint32_t* const* dcbGpuAddrs, const std::uint32_t* dcbSizesInDwords, std::uint32_t count) {
    AgcDriver::Submit(Packets(dcbGpuAddrs, dcbSizesInDwords, count).data(), count, 0);
    return 0;
}

}

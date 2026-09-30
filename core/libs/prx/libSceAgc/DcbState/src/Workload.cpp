#include "prx/libSceAgc/DcbState/include/Workload.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::uint32_t* WriteWorkloadMarker(CommandBuffer* buf, std::uint32_t streamId, const std::uint32_t* workloadIds, std::uint32_t workloadCount, const char* function) {
    Agc::Command::Require(workloadCount == 0 || workloadIds != nullptr, function, "null workload list");
    Agc::Command::Require(workloadCount < 0x3ffeu, function, "workload list exceeds packet size");
    auto* packet = Agc::Command::WriteNop(buf, workloadCount + 2u, function);
    packet[1] = streamId;
    std::copy_n(workloadIds, workloadCount, packet + 2);
    return packet;
}

}

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetWorkloadsActive(CommandBuffer* buf, uint32_t stream_id, const uint32_t* workload_ids, uint32_t workload_count) {
    return WriteWorkloadMarker(buf, stream_id, workload_ids, workload_count, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetWorkloadComplete(CommandBuffer* buf, uint32_t stream_id, uint32_t workload_id) {
    return WriteWorkloadMarker(buf, stream_id, &workload_id, 1, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetWorkloadStreamInactive(CommandBuffer* buf, uint32_t stream_id) {
    return WriteWorkloadMarker(buf, stream_id, nullptr, 0, __func__);
}

uint32_t* APS5_VABI sceAgcAcbSetWorkloadsActive(CommandBuffer* buf, uint32_t stream_id, const uint32_t* workload_ids, uint32_t workload_count) {
    return WriteWorkloadMarker(buf, stream_id, workload_ids, workload_count, __func__);
}

uint32_t* APS5_VABI sceAgcAcbSetWorkloadComplete(CommandBuffer* buf, uint32_t stream_id, uint32_t workload_id) {
    return WriteWorkloadMarker(buf, stream_id, &workload_id, 1, __func__);
}

uint32_t* APS5_VABI sceAgcAcbSetWorkloadStreamInactive(CommandBuffer* buf, uint32_t stream_id) {
    return WriteWorkloadMarker(buf, stream_id, nullptr, 0, __func__);
}

}

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

struct PortState {
    float volume = 1.0f;
    bool muted = false;
};

std::mutex portMutex;
std::map<std::uint32_t, PortState> ports;

}

extern "C" {

int APS5_VABI sceVoiceConnectIPortToOPort(uint32_t input_port_id, uint32_t output_port_id) {
 (void)input_port_id;
 (void)output_port_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceCreatePort(uint32_t* port_id, const VoicePortParam* param) {
 (void)port_id;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceDeletePort(uint32_t port_id) {
 (void)port_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceDisconnectIPortFromOPort(uint32_t input_port_id, uint32_t output_port_id) {
 (void)input_port_id;
 (void)output_port_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceEnd_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceGetBitRate(uint32_t port_id, uint32_t* bitrate) {
 (void)port_id;
 (void)bitrate;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceGetPortAttr(uint32_t port_id, int32_t attr, void* value, int32_t size) {
 (void)port_id;
 (void)attr;
 (void)value;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceGetPortInfo(uint32_t port_id, VoicePortInfo* info) {
 (void)port_id;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceGetVolume(uint32_t port_id, float* volume) {
    if (volume == nullptr) throw std::invalid_argument(std::string(__func__) + ": volume is null");
    std::lock_guard lock(portMutex);
    const auto it = ports.find(port_id);
    *volume = it != ports.end() ? it->second.volume : 1.0f;
    return 0;
}

int APS5_VABI sceVoiceInit(VoiceInitParam* param, int32_t version) {
 (void)param;
 (void)version;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceReadFromOPort(uint32_t output_port_id, void* data, uint32_t* size) {
 (void)output_port_id;
 (void)data;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceSetThreadsParams(void* params) {
 (void)params;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceSetVolume(uint32_t port_id, float volume) {
    std::lock_guard lock(portMutex);
    ports[port_id].volume = volume;
    return 0;
}

int APS5_VABI sceVoiceStart(const VoiceStartParam* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceStop(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceWriteToIPort(uint32_t input_port_id, const void* data, uint32_t* size, int16_t frame_gaps) {
 (void)input_port_id;
 (void)data;
 (void)size;
 (void)frame_gaps;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceGetMuteFlag(uint32_t port_id, bool* muted) {
    if (muted == nullptr) throw std::invalid_argument(std::string(__func__) + ": flag is null");
    std::lock_guard lock(portMutex);
    const auto it = ports.find(port_id);
    *muted = it != ports.end() && it->second.muted;
    return 0;
}

int APS5_VABI sceVoiceSetMuteFlag(uint32_t port_id, bool muted) {
    std::lock_guard lock(portMutex);
    ports[port_id].muted = muted;
    return 0;
}

}

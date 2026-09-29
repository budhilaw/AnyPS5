#include <cstdint>
#include <cstddef>
#include <mutex>
#include <map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_NET_CTL_ERROR_ID_NOT_FOUND = static_cast<int>(0x80412104);
constexpr int SCE_NET_CTL_ERROR_INVALID_ADDR = static_cast<int>(0x80412107);
constexpr int SCE_NET_CTL_ERROR_NOT_CONNECTED = static_cast<int>(0x80412108);
constexpr int SCE_NET_CTL_STATE_DISCONNECTED = 0;

std::mutex mutex;
std::map<int, std::pair<NetCtlCallback, void*>> callbacks;
int nextCallback = 1;

}

extern "C" {

int APS5_VABI sceNetCtlInit(void) { return 0; }
void APS5_VABI sceNetCtlTerm(void) { std::lock_guard lock(mutex); callbacks.clear(); }
int APS5_VABI sceNetCtlCheckCallback(void) { return 0; }

int APS5_VABI sceNetCtlGetState(int* state) {
    if (state == nullptr) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *state = SCE_NET_CTL_STATE_DISCONNECTED;
    return 0;
}

int APS5_VABI sceNetCtlGetStateV6(int* state) { return sceNetCtlGetState(state); }

int APS5_VABI sceNetCtlGetInfo(int code, NetCtlInfo* info) {
    (void)code;
    if (info == nullptr) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    return SCE_NET_CTL_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceNetCtlGetNatInfo(NetCtlNatInfo* nat_info) {
    if (nat_info == nullptr) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    return SCE_NET_CTL_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceNetCtlGetResult(int event_type, int* error_code) {
    (void)event_type;
    if (error_code == nullptr) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *error_code = SCE_NET_CTL_ERROR_NOT_CONNECTED;
    return 0;
}

int APS5_VABI sceNetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid) {
    if (func == nullptr || cid == nullptr) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    std::lock_guard lock(mutex);
    *cid = nextCallback++;
    callbacks.emplace(*cid, std::make_pair(func, arg));
    return 0;
}

int APS5_VABI sceNetCtlUnregisterCallback(int cid) {
    std::lock_guard lock(mutex);
    return callbacks.erase(cid) != 0 ? 0 : SCE_NET_CTL_ERROR_ID_NOT_FOUND;
}

}

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>
#include <string>

namespace {

constexpr int SCE_REMOTEPLAY_CONNECTION_STATUS_DISCONNECT = 0;

}

extern "C" {

int APS5_VABI sceRemoteplayGetConnectionStatus(int user_id, int* status) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)user_id;
    if (status == nullptr) throw std::invalid_argument(std::string(__func__) + ": status is null");
    *status = SCE_REMOTEPLAY_CONNECTION_STATUS_DISCONNECT;
    return 0;
}

int APS5_VABI sceRemoteplayInitialize(void* heap, size_t heap_size) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)heap;
    (void)heap_size;
    return 0;
}

int APS5_VABI sceRemoteplayTerminate(void) {
    Aps5TraceCall_nid_no_patch(__func__);
    return 0;
}

}

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceRemoteplayGetConnectionStatus(int user_id, int* status) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)user_id;
 (void)status;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRemoteplayInitialize(void* heap, size_t heap_size) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)heap;
 (void)heap_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceRemoteplayTerminate(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

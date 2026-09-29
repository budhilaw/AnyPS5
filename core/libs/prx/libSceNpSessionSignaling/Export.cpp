#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpSessionSignalingInitialize(void* param) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)param;
    return 0;
}

}

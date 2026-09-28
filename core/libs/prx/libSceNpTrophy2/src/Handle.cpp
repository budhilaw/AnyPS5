#include <stdexcept>
#include <string>

#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2CreateHandle(int* handle) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (handle == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    *handle = NP_TROPHY2_HANDLE_DEFAULT;
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2DestroyHandle(int handle) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2AbortHandle(int handle) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    return SCE_NP_TROPHY2_OK;
}

}

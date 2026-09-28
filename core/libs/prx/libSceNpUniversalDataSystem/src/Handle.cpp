#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateHandle(int* handle) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (handle == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    *handle = NP_UNIVERSAL_DATA_SYSTEM_HANDLE_DEFAULT;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemDestroyHandle(int handle) {
    Aps5TraceCall_nid_no_patch(__func__);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemAbortHandle(int handle) {
    Aps5TraceCall_nid_no_patch(__func__);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}

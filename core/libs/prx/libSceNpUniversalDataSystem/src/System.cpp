#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemInitialize(const NpUniversalDataSystemInitParam* param) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (param == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemTerminate(void) {
    Aps5TraceCall_nid_no_patch(__func__);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemGetMemoryStat(NpUniversalDataSystemMemoryStat* stat) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (stat == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    *stat = {};
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemGetStorageStat(int context, NpUniversalDataSystemStorageStat* stat) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (stat == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    *stat = {};
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}

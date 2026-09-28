#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceSaveDataBackup(const SaveDataBackup* backup) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)backup;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSaveDataGetEventResult(const void* event_param, SaveDataEvent* event) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)event_param;
 (void)event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSaveDataGetSaveDataMemory2(SaveDataMemoryGet2* get_param) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)get_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2* set_param) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)set_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2* setup_param, SaveDataMemorySetupResult* result) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)setup_param;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSaveDataSyncSaveDataMemory(const void* sync_param) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)sync_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSaveDataTransferringMount(const SaveDataTransferringMount* mount, SaveDataMountResult* mount_result) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)mount;
 (void)mount_result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

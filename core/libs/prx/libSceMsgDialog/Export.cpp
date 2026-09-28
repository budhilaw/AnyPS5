#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceMsgDialogClose(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogGetResult(void* result) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogGetStatus(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogInitialize(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogOpen(const void* param) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogProgressBarInc(int target, uint32_t delta) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)target;
 (void)delta;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogProgressBarSetMsg(int target, const char* msg) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)target;
 (void)msg;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogProgressBarSetValue(int target, uint32_t rate) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)target;
 (void)rate;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogTerminate(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogUpdateStatus(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

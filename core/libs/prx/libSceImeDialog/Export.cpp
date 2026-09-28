#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceImeDialogAbort(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogGetPanelPositionAndForm(PositionAndForm* form) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)form;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)param;
 (void)width;
 (void)height;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogGetPanelSizeExtended(const Param* param, const ExtendedParam* extended, uint32_t* width, uint32_t* height) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)param;
 (void)extended;
 (void)width;
 (void)height;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogGetResult(Result* result) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogGetStatus(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogInit(const Param* param, const ExtendedParam* extended) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)param;
 (void)extended;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeDialogTerm(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

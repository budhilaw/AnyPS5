#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"

static bool g_initialized = false;

extern "C" {

int APS5_VABI sceCommonDialogInitialize(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 if (g_initialized) {
  return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
 }
 g_initialized = true;
 return COMMON_DIALOG_OK;
}

bool APS5_VABI sceCommonDialogIsUsed(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 return false;
}

}

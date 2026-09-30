#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpCommerceDialogUpdateStatus(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpCommerceShowPsStoreIcon(int position) {
    (void)position;
    return 0;
}

int APS5_VABI sceNpCommerceHidePsStoreIcon(void) {
    return 0;
}

}

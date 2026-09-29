#include <cstdint>
#include "prx/libc/include/General.hpp"


extern "C" {

int APS5_VABI sceSystemServiceDisableMusicPlayer_nid_postfix() {
    Aps5TraceCall_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceReenableMusicPlayer_nid_postfix() {
    Aps5TraceCall_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceShowControllerSettings_nid_postfix() {
    Aps5TraceCall_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("m5CYKX20wfg", sceSystemServiceUnknown_m5CYKX20wfg);
int APS5_VABI sceSystemServiceUnknown_m5CYKX20wfg() { NotImplemented_nid_no_patch(__func__); return 0; }

APS5_EXPORT("sPuK5ic3GD4", sceSystemServiceUnknown_sPuK5ic3GD4);
int APS5_VABI sceSystemServiceUnknown_sPuK5ic3GD4() { NotImplemented_nid_no_patch(__func__); return 0; }

APS5_EXPORT("uaieF+glFPs", sceSystemServiceUnknown_uaieF_plus_glFPs);
int APS5_VABI sceSystemServiceUnknown_uaieF_plus_glFPs() { NotImplemented_nid_no_patch(__func__); return 0; }

}

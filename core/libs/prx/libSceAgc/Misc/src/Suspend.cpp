#include "prx/libSceAgc/Misc/include/Suspend.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

// A point where the system may suspend the title (once a frame): it does not wait for the GPU on
// the console, and blocking here kept the title's CPU from overlapping the driver and the GPU.
// ANYPS5_SYNC_SUSPEND=1 drains the driver and the GPU at every point (diagnostics).
int APS5_VABI sceAgcSuspendPoint(void) {
    static const bool drain = std::getenv("ANYPS5_SYNC_SUSPEND") != nullptr;
    if (drain) AgcDriverSuspendPoint_nid_postfix();
    return 0;
}

}

#include "prx/libSceAgc/Misc/include/Suspend.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcSuspendPoint(void) {
    static const bool drain = std::getenv("ANYPS5_SYNC_SUSPEND") != nullptr;
    if (drain) AgcDriverSuspendPoint_nid_postfix();
    return 0;
}

}

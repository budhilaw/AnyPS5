#include <cstdint>
#include "prx/libc/include/General.hpp"

// Imports of PS5 titles that this library does not implement yet. Every entry point reports
// itself, so a title's first use is diagnosed instead of silently misbehaving. Names come from
// NID matching against known symbol names; the rest are listed by NID (see docs/TechnicalDebt.md).

extern "C" {

// Called by Unity's user-settings setup right after sceUserServiceGetGamePresets with
// (userId, int32_t* value) and its result checked as an error code: a per-user preference
// getter whose name is unknown. An unset preference reads as 0 on the console.
APS5_EXPORT("O6IW1-Dwm-w", libSceUserServiceUnknown_O6IW1_minus_Dwm_minus_w);
int APS5_VABI libSceUserServiceUnknown_O6IW1_minus_Dwm_minus_w(int user_id, std::int32_t* value) {
    if (user_id != 0x10000000 || value == nullptr) return static_cast<int>(0x80960002); // SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT
    *value = 0;
    return 0;
}

}

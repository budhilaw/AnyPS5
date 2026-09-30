#include <cstdint>
#include "prx/libc/include/General.hpp"


extern "C" {

APS5_EXPORT("O6IW1-Dwm-w", libSceUserServiceUnknown_O6IW1_minus_Dwm_minus_w);
int APS5_VABI libSceUserServiceUnknown_O6IW1_minus_Dwm_minus_w(int user_id, std::int32_t* value) {
    if (user_id != 0x10000000 || value == nullptr) return static_cast<int>(0x80960002);
    *value = 0;
    return 0;
}

}

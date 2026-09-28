#include <cstdint>
#include "prx/libc/include/General.hpp"

// libSceHmd is linked by titles that never reach a head-mounted display on PS5 without PSVR2.
// No import of it has been observed yet; it exists so the loader can satisfy DT_NEEDED.
uint32_t Need_libSceHmd = 1;

extern "C" {
APS5_DUMMY_FUN
}

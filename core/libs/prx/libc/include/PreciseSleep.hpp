#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_PRECISESLEEP_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_PRECISESLEEP_HPP

#include <cstdint>

extern "C" {

void PreciseSleepNanos_nid_no_patch(std::uint64_t nanos);

}

#endif

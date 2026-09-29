#pragma once

#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <cstddef>
#include <cstdint>

inline void PrepareGuestBuffer(const void* buffer, std::size_t bytes, bool writable) {
    if (buffer == nullptr || bytes == 0) return;
    GuestMemoryTracking::GuestMemoryTrackingResolve_nid_postfix(reinterpret_cast<std::uint64_t>(buffer), bytes, writable);
}

#pragma once

#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <cstddef>
#include <cstdint>

// The GPU caches watch guest pages by write- (or read-) protecting them. The host kernel does not
// fault on those pages in read()/write() the way guest code does: it returns EFAULT. Before a
// system call touches a guest buffer, the watches over it are resolved, which lifts the protection.
inline void PrepareGuestBuffer(const void* buffer, std::size_t bytes, bool writable) {
    if (buffer == nullptr || bytes == 0) return;
    GuestMemoryTracking::GuestMemoryTrackingResolve_nid_postfix(reinterpret_cast<std::uint64_t>(buffer), bytes, writable);
}

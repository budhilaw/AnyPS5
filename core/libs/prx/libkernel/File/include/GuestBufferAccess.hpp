#pragma once

#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

inline void PrepareGuestBuffer(const void* buffer, std::size_t bytes, bool writable) {
    if (buffer == nullptr || bytes == 0) return;
    GuestMemoryTracking::GuestMemoryTrackingResolve_nid_postfix(reinterpret_cast<std::uint64_t>(buffer), bytes, writable);
}

template<typename TRead>
auto ReadIntoGuest(void* buffer, std::size_t bytes, TRead read) {
    PrepareGuestBuffer(buffer, bytes, true);
    auto result = read(buffer, bytes);
    if (result >= 0 || errno != EFAULT || bytes == 0) return result;
    std::vector<std::byte> staging(bytes);
    result = read(staging.data(), bytes);
    if (result > 0) std::memcpy(buffer, staging.data(), static_cast<std::size_t>(result));
    return result;
}

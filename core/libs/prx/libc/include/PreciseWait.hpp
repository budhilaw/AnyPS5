#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_PRECISEWAIT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_PRECISEWAIT_HPP

#include "prx/libc/include/PreciseSleep.hpp"
#include <algorithm>
#include <chrono>

namespace PreciseWait {

template <typename Condition, typename Lock, typename Predicate>
bool Until(Condition& condition, Lock& lock, std::chrono::steady_clock::time_point deadline, Predicate predicate) {
#ifdef _WIN32
    constexpr auto coarse = std::chrono::milliseconds(20);
    constexpr auto step = std::chrono::microseconds(500);
    while (!predicate()) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return predicate();
        const auto remaining = deadline - now;
        if (remaining > coarse) {
            condition.wait_for(lock, remaining - coarse);
            continue;
        }
        lock.unlock();
        PreciseSleepNanos_nid_no_patch(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::min<std::chrono::steady_clock::duration>(remaining, step)).count()));
        lock.lock();
    }
    return true;
#else
    return condition.wait_until(lock, deadline, predicate);
#endif
}

template <typename Condition, typename Lock, typename Rep, typename Period, typename Predicate>
bool For(Condition& condition, Lock& lock, std::chrono::duration<Rep, Period> timeout, Predicate predicate) {
    return Until(condition, lock, std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout), predicate);
}

}

#endif

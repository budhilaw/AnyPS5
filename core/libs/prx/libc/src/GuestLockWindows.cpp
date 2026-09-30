#include "prx/libc/include/GuestLock.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

static_assert(sizeof(std::atomic<std::uint32_t>) == sizeof(std::uint32_t) && std::atomic<std::uint32_t>::is_always_lock_free);

namespace {

constexpr int SpinsBeforeWait = 16;

}

extern "C" void GuestLockWait_nid_no_patch(std::atomic<std::uint32_t>* word, std::uint32_t value) {
    for (int spin = 0; spin < SpinsBeforeWait; ++spin) {
        if (word->load(std::memory_order_relaxed) != value) return;
        YieldProcessor();
    }
    WaitOnAddress(word, &value, sizeof(value), INFINITE);
}

extern "C" void GuestLockWake_nid_no_patch(std::atomic<std::uint32_t>* word) {
    WakeByAddressSingle(word);
}

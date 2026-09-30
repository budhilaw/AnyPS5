#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTLOCK_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTLOCK_HPP

#include "prx/libc/include/PreciseSleep.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

class GuestLock {
public:
    bool try_lock() noexcept {
        std::uint32_t expected = 0;
        return word.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
    }
    void lock() noexcept {
        if (try_lock()) return;
        while (word.exchange(2, std::memory_order_acquire) != 0) word.wait(2, std::memory_order_relaxed);
    }
    void unlock() noexcept {
        if (word.exchange(0, std::memory_order_release) == 2) word.notify_one();
    }

    template <typename Rep, typename Period>
    bool try_lock_for(std::chrono::duration<Rep, Period> timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        auto pause = std::chrono::microseconds(1);
        while (!try_lock()) {
            if (std::chrono::steady_clock::now() >= deadline) return false;
            PreciseSleepNanos_nid_no_patch(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(pause).count()));
            pause = std::min(pause * 2, std::chrono::microseconds(500));
        }
        return true;
    }

    template <typename Clock, typename Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& deadline) {
        return try_lock_for(deadline - Clock::now());
    }

private:
    std::atomic<std::uint32_t> word{0};
};

class RecursiveGuestLock {
public:
    bool try_lock() noexcept {
        const auto self = std::this_thread::get_id();
        if (owner.load(std::memory_order_relaxed) == self) { ++depth; return true; }
        if (!exclusive.try_lock()) return false;
        take(self);
        return true;
    }

    void lock() noexcept {
        const auto self = std::this_thread::get_id();
        if (owner.load(std::memory_order_relaxed) == self) { ++depth; return; }
        exclusive.lock();
        take(self);
    }

    void unlock() noexcept {
        if (--depth != 0) return;
        owner.store(std::thread::id{}, std::memory_order_relaxed);
        exclusive.unlock();
    }

    template <typename Clock, typename Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& deadline) {
        const auto self = std::this_thread::get_id();
        if (owner.load(std::memory_order_relaxed) == self) { ++depth; return true; }
        if (!exclusive.try_lock_until(deadline)) return false;
        take(self);
        return true;
    }

private:
    void take(std::thread::id self) noexcept {
        owner.store(self, std::memory_order_relaxed);
        depth = 1;
    }

    GuestLock exclusive;
    std::atomic<std::thread::id> owner{};
    unsigned depth = 0;
};

#endif

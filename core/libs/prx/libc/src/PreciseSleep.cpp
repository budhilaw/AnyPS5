#include "prx/libc/include/PreciseSleep.hpp"
#include <chrono>
#include <cerrno>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <time.h>
#endif

namespace {

constexpr std::uint64_t SpinLimitNanos = 50000ULL;

}

extern "C" void PreciseSleepNanos_nid_no_patch(std::uint64_t nanos) {
    if (nanos == 0) return;
#ifdef _WIN32
    if (nanos <= SpinLimitNanos) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::nanoseconds(nanos);
        while (std::chrono::steady_clock::now() < deadline) YieldProcessor();
        return;
    }
    thread_local HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>((nanos + 99ULL) / 100ULL);
    if (timer != nullptr && SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0) && WaitForSingleObject(timer, INFINITE) == WAIT_OBJECT_0) return;
    Sleep(static_cast<DWORD>((nanos + 999999ULL) / 1000000ULL));
#else
    timespec request{};
    request.tv_sec = static_cast<time_t>(nanos / 1000000000ULL);
    request.tv_nsec = static_cast<long>(nanos % 1000000000ULL);
    while (nanosleep(&request, &request) == -1 && errno == EINTR) {}
#endif
}

extern "C" void GuestSleepNanos_nid_no_patch(std::uint64_t nanos) {
#ifdef _WIN32
    if (nanos != 0 && nanos <= SpinLimitNanos) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::nanoseconds(nanos);
        do {
            if (!SwitchToThread()) YieldProcessor();
        } while (std::chrono::steady_clock::now() < deadline);
        return;
    }
#endif
    PreciseSleepNanos_nid_no_patch(nanos);
}

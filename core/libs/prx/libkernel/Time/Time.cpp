#include "prx/libkernel/Time/include/Time.hpp"

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseSleep.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <thread>
#include <ctime>
#include <chrono>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#endif
#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif
#if defined(__x86_64__)
#include <cpuid.h>
#include <x86intrin.h>
#endif

static std::uint64_t GetMonotonicNanos() {
#if defined(__APPLE__)
    static const mach_timebase_info_data_t timebase = [] {
        mach_timebase_info_data_t info{};
        mach_timebase_info(&info);
        return info;
    }();
    return static_cast<std::uint64_t>(static_cast<unsigned __int128>(mach_absolute_time()) * timebase.numer / timebase.denom);
#elif defined(_WIN32)
    static const std::uint64_t freq = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return static_cast<std::uint64_t>(f.QuadPart);
    }();
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<std::uint64_t>(static_cast<unsigned __int128>(counter.QuadPart) * 1000000000ULL / freq);
#else
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL +
           static_cast<std::uint64_t>(ts.tv_nsec);
#endif
}

static std::uint64_t GetStartNanos() {
    static const std::uint64_t start = GetMonotonicNanos();
    return start;
}

static void SleepNanos(std::uint64_t nanos) {
    GuestSleepNanos_nid_no_patch(nanos);
}

struct TscClock {
    bool calibrated;
    std::uint64_t frequency;
};

static constexpr TscClock LegacyTsc{false, 1000000000ULL};

#if defined(__x86_64__)
static constexpr std::uint64_t MinimumTscFrequency = 100000000ULL;
static constexpr std::uint64_t MaximumTscFrequency = 10000000000ULL;
static constexpr std::uint64_t TscCalibrationNanos = 100000000ULL;

struct TscSample {
    std::uint64_t tsc;
    std::uint64_t nanos;
};

static bool HasInvariantTsc() {
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    return __get_cpuid(0x80000007u, &eax, &ebx, &ecx, &edx) != 0 && (edx & (1u << 8)) != 0;
}

static std::optional<TscSample> SampleTsc() {
    std::optional<TscSample> best;
    std::uint64_t bestWidth = 0;
    for (int attempt = 0; attempt < 16; ++attempt) {
        const std::uint64_t before = __rdtsc();
        const std::uint64_t nanos = GetMonotonicNanos();
        const std::uint64_t after = __rdtsc();
        if (after < before || (best && after - before >= bestWidth)) continue;
        bestWidth = after - before;
        best = TscSample{before + bestWidth / 2, nanos};
    }
    return best;
}

static std::uint64_t CalibrateTscFrequency() {
    const auto first = SampleTsc();
    SleepNanos(TscCalibrationNanos);
    const auto second = SampleTsc();
    if (!first || !second || second->tsc <= first->tsc || second->nanos <= first->nanos) return 0;
    return static_cast<std::uint64_t>(static_cast<unsigned __int128>(second->tsc - first->tsc) * 1000000000ULL / (second->nanos - first->nanos));
}
#endif

static TscClock SelectTscClock() {
    if (std::getenv("ANYPS5_LEGACY_TSC") != nullptr) {
        APS5_LOG_CHARS_OUT("TSC: ANYPS5_LEGACY_TSC is set, reporting the 1 GHz nanosecond clock");
        return LegacyTsc;
    }
#if defined(__x86_64__)
    if (!HasInvariantTsc()) {
        APS5_LOG_CHARS_ERR("TSC: the host CPU reports no invariant TSC, reporting the 1 GHz nanosecond clock; guest RDTSC timing may run at the wrong speed");
        return LegacyTsc;
    }
    const std::uint64_t start = GetMonotonicNanos();
    const std::uint64_t frequency = CalibrateTscFrequency();
    const double milliseconds = static_cast<double>(GetMonotonicNanos() - start) / 1000000.0;
    if (frequency < MinimumTscFrequency || frequency > MaximumTscFrequency) {
        APS5_LOG_ERR("TSC: calibration measured %llu Hz, outside 100 MHz to 10 GHz, reporting the 1 GHz nanosecond clock; guest RDTSC timing may run at the wrong speed", static_cast<unsigned long long>(frequency));
        return LegacyTsc;
    }
    APS5_LOG_OUT("TSC frequency %.3f MHz (calibrated over %.1f ms)", static_cast<double>(frequency) / 1000000.0, milliseconds);
    return TscClock{true, frequency};
#else
    APS5_LOG_CHARS_OUT("TSC: this host has no x86-64 TSC, reporting the 1 GHz nanosecond clock");
    return LegacyTsc;
#endif
}

static const TscClock tscClock = SelectTscClock();

static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;

extern "C" {

std::uint64_t APS5_VABI sceKernelGetProcessTime() {
    return (GetMonotonicNanos() - GetStartNanos()) / 1000ULL;
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounter() {
    return GetMonotonicNanos() - GetStartNanos();
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounterFrequency() {
    return 1000000000ULL;
}

int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds) {
    SleepNanos(static_cast<std::uint64_t>(microseconds) * 1000ULL);
    return 0;
}

int APS5_VABI sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    if (rqtp == nullptr) {
        return -1;
    }
    if (rqtp->tv_sec < 0 || rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000LL) {
        return -1;
    }
    SleepNanos(static_cast<std::uint64_t>(rqtp->tv_sec) * 1000000000ULL +
               static_cast<std::uint64_t>(rqtp->tv_nsec));
    if (rmtp != nullptr) {
        rmtp->tv_sec = 0;
        rmtp->tv_nsec = 0;
    }
    return 0;
}

int APS5_VABI nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return sceKernelNanosleep(rqtp, rmtp);
}

int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp) {
    if (tp == nullptr) {
        APS5_INVALID_ARG_EX;
    }
#ifdef _WIN32
    if (clockId == 0 || clockId == 9 || clockId == 10) {
        FILETIME ft{};
        GetSystemTimePreciseAsFileTime(&ft);
        std::uint64_t t = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        t -= 116444736000000000ULL;
        t *= 100ULL;
        tp->tv_sec = static_cast<std::int64_t>(t / 1000000000ULL);
        tp->tv_nsec = static_cast<std::int64_t>(t % 1000000000ULL);
        return 0;
    }
    if (clockId == 4 || clockId == 1 || clockId == 5 || clockId == 7 || clockId == 8 || clockId == 11 || clockId == 12) {
        std::uint64_t nanos = GetMonotonicNanos();
        tp->tv_sec = static_cast<std::int64_t>(nanos / 1000000000ULL);
        tp->tv_nsec = static_cast<std::int64_t>(nanos % 1000000000ULL);
        return 0;
    }
    throw std::runtime_error(std::string(__func__) + ": unsupported clock_id " + std::to_string(clockId));
#else
#if defined(__APPLE__)
    if (clockId == 4 || clockId == 7 || clockId == 11 || clockId == 5 || clockId == 8 || clockId == 12) {
        const std::uint64_t nanos = GetMonotonicNanos();
        tp->tv_sec = static_cast<std::int64_t>(nanos / 1000000000ULL);
        tp->tv_nsec = static_cast<std::int64_t>(nanos % 1000000000ULL);
        return 0;
    }
#endif
    clockid_t nativeId;
    switch (clockId) {
        case 0:
        case 9:
        case 10:
            nativeId = CLOCK_REALTIME;
            break;
        case 4:
        case 7:
        case 11:
            nativeId = CLOCK_MONOTONIC;
            break;
        case 5:
        case 8:
        case 12:
            nativeId = CLOCK_MONOTONIC;
            break;
        case 14:
            nativeId = CLOCK_THREAD_CPUTIME_ID;
            break;
        case 15:
            nativeId = CLOCK_PROCESS_CPUTIME_ID;
            break;
        default:
            throw std::runtime_error(std::string(__func__) + ": unsupported clock_id " + std::to_string(clockId));
    }
    struct timespec ts{};
    if (clock_gettime(nativeId, &ts) != 0) {
        return -1;
    }
    tp->tv_sec = static_cast<std::int64_t>(ts.tv_sec);
    tp->tv_nsec = static_cast<std::int64_t>(ts.tv_nsec);
    return 0;
#endif
}

int APS5_VABI gettimeofday_nid_postfix(KernelTimeval* tv, KernelTimezone* tz) {
    if (tv == nullptr) {
        APS5_INVALID_ARG_EX;
    }
#ifdef _WIN32
    FILETIME ft{};
    GetSystemTimePreciseAsFileTime(&ft);
    std::uint64_t t = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    t -= 116444736000000000ULL;
    tv->tv_sec = static_cast<std::int64_t>(t / 10000000ULL);
    tv->tv_usec = static_cast<std::int64_t>((t % 10000000ULL) / 10ULL);
#else
    struct timeval native{};
    if (gettimeofday(&native, nullptr) != 0) {
        return -1;
    }
    tv->tv_sec = static_cast<std::int64_t>(native.tv_sec);
    tv->tv_usec = static_cast<std::int64_t>(native.tv_usec);
#endif
    if (tz != nullptr) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
    }
    return 0;
}

int APS5_VABI clock_getres_nid_postfix(int clockId, KernelTimespec* res) {
    if (res == nullptr) {
        APS5_INVALID_ARG_EX;
    }
#ifdef _WIN32
    if (clockId == 0 || clockId == 9) {
        res->tv_sec = 0;
        res->tv_nsec = 100LL;
        return 0;
    }
    if (clockId == 4 || clockId == 1 || clockId == 5 || clockId == 7 || clockId == 8 || clockId == 10 || clockId == 11 || clockId == 12) {
        static const std::uint64_t freq = [] {
            LARGE_INTEGER f{};
            QueryPerformanceFrequency(&f);
            return static_cast<std::uint64_t>(f.QuadPart);
        }();
        std::uint64_t nsPerTick = (1000000000ULL + freq - 1ULL) / freq;
        res->tv_sec = 0;
        res->tv_nsec = static_cast<std::int64_t>(nsPerTick);
        return 0;
    }
    throw std::runtime_error(std::string(__func__) + ": unsupported clock_id " + std::to_string(clockId));
#else
    clockid_t nativeId;
    switch (clockId) {
        case 0:
        case 9:
        case 10:
            nativeId = CLOCK_REALTIME;
            break;
        case 4:
        case 7:
        case 11:
            nativeId = CLOCK_MONOTONIC;
            break;
        case 5:
        case 8:
        case 12:
            nativeId = CLOCK_MONOTONIC;
            break;
        case 14:
            nativeId = CLOCK_THREAD_CPUTIME_ID;
            break;
        case 15:
            nativeId = CLOCK_PROCESS_CPUTIME_ID;
            break;
        default:
            throw std::runtime_error(std::string(__func__) + ": unsupported clock_id " + std::to_string(clockId));
    }
    struct timespec ts{};
    if (clock_getres(nativeId, &ts) != 0) {
        return -1;
    }
    res->tv_sec = static_cast<std::int64_t>(ts.tv_sec);
    res->tv_nsec = static_cast<std::int64_t>(ts.tv_nsec);
    return 0;
#endif
}

// ---------------------------------------------------------------------------
// Moved as-is (not yet implemented) from the monolithic libkernel/Export.cpp.
// ---------------------------------------------------------------------------

int APS5_VABI sceKernelClockGetres(KernelClockid clock_id, KernelTimespec* tp) {
    if (tp == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    if (clock_id != 0 && clock_id != 4 && clock_id != 5 && clock_id != 7 && clock_id != 8 && clock_id != 9 && clock_id != 10 && clock_id != 11 && clock_id != 12 && clock_id != 13 && clock_id != 15) return SCE_KERNEL_ERROR_EINVAL;
    tp->tv_sec = 0;
    tp->tv_nsec = 1;
    return 0;
}

int APS5_VABI sceKernelClockGettime(KernelClockid clock_id, KernelTimespec* tp) {
    if (tp == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    std::chrono::nanoseconds now;
    switch (clock_id) {
        case 0: case 9: case 10: case 13: now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()); break;
        case 4: case 5: case 7: case 8: case 11: case 12: now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()); break;
        case 15: now = std::chrono::nanoseconds(static_cast<std::int64_t>(std::clock()) * (1000000000ll / CLOCKS_PER_SEC)); break;
        default: return SCE_KERNEL_ERROR_EINVAL;
    }
    tp->tv_sec = static_cast<std::int64_t>(now.count() / 1000000000ll);
    tp->tv_nsec = static_cast<std::int64_t>(now.count() % 1000000000ll);
    return 0;
}

int APS5_VABI sceKernelConvertLocaltimeToUtc(int64_t local_time, int64_t reserved, int64_t* utc_time, KernelTimezone* timezone, int32_t* dst_seconds) {
 (void)local_time;
 (void)reserved;
 (void)utc_time;
 (void)timezone;
 (void)dst_seconds;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelConvertUtcToLocaltime(int64_t utc_time, int64_t* local_time, KernelTimesec* st, uint64_t* dst_sec) {
 (void)utc_time;
 (void)local_time;
 (void)st;
 (void)dst_sec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGettimeofday(KernelTimeval* tp) {
    if (tp == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    tp->tv_sec = static_cast<std::int64_t>(now / 1000000);
    tp->tv_usec = static_cast<std::int64_t>(now % 1000000);
    return 0;
}

int APS5_VABI sceKernelGettimezone(KernelTimezone* tz) {
 (void)tz;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t APS5_VABI sceKernelReadTsc(void) {
#if defined(__x86_64__)
    if (tscClock.calibrated) return __rdtsc();
#endif
    return GetMonotonicNanos();
}

uint64_t APS5_VABI sceKernelGetTscFrequency(void) {
    return tscClock.frequency;
}

unsigned int APS5_VABI sceKernelSleep(unsigned int seconds) {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    return 0;
}

}

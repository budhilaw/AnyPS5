#include "prx/libkernel/Time/include/Time.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#if defined(__x86_64__)
#include <cpuid.h>
#include <x86intrin.h>
#endif

namespace {

struct Sample {
    std::uint64_t host;
    std::uint64_t guest;
    std::chrono::steady_clock::time_point time;
};

void Require(bool value) { if (!value) std::abort(); }

std::uint64_t ReadHostTsc() {
#if defined(__x86_64__)
    return __rdtsc();
#else
    return 0;
#endif
}

bool HostHasInvariantTsc() {
#if defined(__x86_64__)
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    return __get_cpuid(0x80000007u, &eax, &ebx, &ecx, &edx) != 0 && (edx & (1u << 8)) != 0;
#else
    return false;
#endif
}

Sample Take() {
    Sample best{};
    auto bestWidth = std::chrono::steady_clock::duration::max();
    for (int attempt = 0; attempt < 16; ++attempt) {
        const auto before = std::chrono::steady_clock::now();
        const auto host = ReadHostTsc();
        const auto guest = sceKernelReadTsc();
        const auto after = std::chrono::steady_clock::now();
        if (after - before >= bestWidth) continue;
        bestWidth = after - before;
        best = Sample{host, guest, before + bestWidth / 2};
    }
    return best;
}

bool MatchesRealTime(std::uint64_t ticks, std::uint64_t frequency, std::chrono::steady_clock::duration elapsed) {
    const double seconds = std::chrono::duration<double>(elapsed).count();
    return std::abs(static_cast<double>(ticks) / static_cast<double>(frequency) - seconds) <= 0.005 * seconds;
}

}

int main(int argc, char** argv) {
    const bool legacy = argc > 1 && std::strcmp(argv[1], "--legacy") == 0;
    const auto firstCall = std::chrono::steady_clock::now();
    const std::uint64_t frequency = sceKernelGetTscFrequency();
    Require(std::chrono::steady_clock::now() - firstCall < std::chrono::milliseconds(50));
    Require(frequency >= 100000000ULL && frequency <= 10000000000ULL);
    for (int call = 0; call < 1000; ++call) Require(sceKernelGetTscFrequency() == frequency);
    auto previous = sceKernelReadTsc();
    for (int read = 0; read < 100000; ++read) {
        const auto current = sceKernelReadTsc();
        Require(current >= previous);
        previous = current;
    }
    const bool calibrated = !legacy && HostHasInvariantTsc();
    Require(calibrated || frequency == 1000000000ULL);
    const auto begin = Take();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto end = Take();
    const auto elapsed = end.time - begin.time;
    Require(MatchesRealTime(end.guest - begin.guest, frequency, elapsed));
    if (calibrated) {
        Require(MatchesRealTime(end.host - begin.host, frequency, elapsed));
        Require(end.guest >= end.host && end.guest - end.host < frequency / 1000);
    }
}

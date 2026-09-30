#include "prx/libSceAgcDriver/Execution/include/DriverThread.hpp"
#include "prx/libc/include/SlowOperation.hpp"
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#else
#include <time.h>
#endif
#ifdef __APPLE__
#include <pthread/qos.h>
#endif

namespace AgcDriver::DriverThread {
namespace {

#ifdef _WIN32
std::uint64_t threadCounter() {
    ULONG64 cycles = 0;
    QueryThreadCycleTime(GetCurrentThread(), &cycles);
    return cycles;
}

std::uint64_t referenceCounter() {
    return __rdtsc();
}

std::uint64_t cpuNanoseconds(std::uint64_t cycles, std::uint64_t referenceCycles, std::uint64_t wallNanoseconds) {
    if (referenceCycles == 0) return 0;
    return static_cast<std::uint64_t>(static_cast<double>(cycles) / static_cast<double>(referenceCycles) * static_cast<double>(wallNanoseconds));
}
#else
std::uint64_t threadCounter() {
    timespec value{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value);
    return static_cast<std::uint64_t>(value.tv_sec) * 1000000000u + static_cast<std::uint64_t>(value.tv_nsec);
}

std::uint64_t referenceCounter() {
    return 0;
}

std::uint64_t cpuNanoseconds(std::uint64_t nanoseconds, std::uint64_t, std::uint64_t) {
    return nanoseconds;
}
#endif

}

void RaisePriority() {
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#elif defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

CpuUsage::CpuUsage(const char* name)
    : name(name), enabled(SlowOperationEnabled_nid_no_patch()), wall(std::chrono::steady_clock::now()), cpu(threadCounter()), reference(referenceCounter()) {}

CpuUsage::Window CpuUsage::Take() {
    const auto now = std::chrono::steady_clock::now();
    const auto used = threadCounter();
    const auto counter = referenceCounter();
    const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - wall).count());
    const Window window{cpuNanoseconds(used - cpu, counter - reference, elapsed), elapsed};
    wall = now;
    cpu = used;
    reference = counter;
    return window;
}

void CpuUsage::report() {
    if (std::chrono::steady_clock::now() - wall < std::chrono::seconds(5)) return;
    const auto window = Take();
    std::fprintf(stderr, "[slow-ops] %s: cpu=%.1f ms wall=%.1f ms (%.1f%%)\n", name, static_cast<double>(window.cpuNanoseconds) / 1e6, static_cast<double>(window.wallNanoseconds) / 1e6, 100.0 * static_cast<double>(window.cpuNanoseconds) / static_cast<double>(window.wallNanoseconds));
}

}

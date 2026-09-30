#include "prx/libc/include/GuestLock.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <x86intrin.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

void Require(bool value, const char* failure) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", failure);
    std::abort();
}

struct alignas(64) PaddedLock {
    GuestLock lock;
};

struct WaitOutcome {
    double cpuShare = 0.0;
    std::chrono::microseconds wakeLatency{};
};

#ifdef _WIN32
std::uint64_t ThreadCycles() {
    ULONG64 cycles = 0;
    Require(QueryThreadCycleTime(GetCurrentThread(), &cycles) != 0, "QueryThreadCycleTime failed");
    return cycles;
}
#endif

WaitOutcome MeasureBlockedWaiter(int neighbourThreads) {
    PaddedLock locks[2];
    std::atomic<bool> waiting{false};
    std::atomic<bool> stopNeighbours{false};
    std::atomic<std::uint64_t> neighbourRounds{0};
    Clock::time_point acquired{};
    double cpuShare = 0.0;
    locks[0].lock.lock();
    std::thread waiter([&] {
#ifdef _WIN32
        const auto cycles = ThreadCycles();
        const auto ticks = __rdtsc();
#endif
        waiting.store(true);
        locks[0].lock.lock();
        acquired = Clock::now();
#ifdef _WIN32
        cpuShare = static_cast<double>(ThreadCycles() - cycles) / static_cast<double>(__rdtsc() - ticks);
#endif
        locks[0].lock.unlock();
    });
    std::vector<std::thread> neighbours;
    for (int index = 0; index < neighbourThreads; ++index) {
        neighbours.emplace_back([&] {
            while (!stopNeighbours.load(std::memory_order_relaxed)) {
                locks[1].lock.lock();
                neighbourRounds.fetch_add(1, std::memory_order_relaxed);
                locks[1].lock.unlock();
            }
        });
    }
    while (!waiting.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto released = Clock::now();
    locks[0].lock.unlock();
    waiter.join();
    stopNeighbours.store(true);
    for (auto& neighbour : neighbours) neighbour.join();
    Require(acquired >= released, "a GuestLock waiter entered while the lock was held");
    Require(neighbourThreads == 0 || neighbourRounds.load() > 0, "the neighbouring lock saw no traffic");
    return {cpuShare, std::chrono::duration_cast<std::chrono::microseconds>(acquired - released)};
}

void CheckMutualExclusion() {
    GuestLock plain;
    RecursiveGuestLock recursive;
    std::uint64_t plainCount = 0;
    std::uint64_t recursiveCount = 0;
    std::vector<std::thread> threads;
    for (int index = 0; index < 4; ++index) {
        threads.emplace_back([&] {
            for (int round = 0; round < 20000; ++round) {
                plain.lock();
                ++plainCount;
                plain.unlock();
                recursive.lock();
                recursive.lock();
                ++recursiveCount;
                recursive.unlock();
                recursive.unlock();
            }
        });
    }
    for (auto& thread : threads) thread.join();
    Require(plainCount == 80000 && recursiveCount == 80000, "GuestLock lost an update under contention");
    Require(plain.try_lock() && recursive.try_lock(), "a released GuestLock could not be taken");
    plain.unlock();
    recursive.unlock();
}

void CheckTimedLock() {
    GuestLock lock;
    lock.lock();
    bool acquired = true;
    Clock::duration waited{};
    std::thread expiring([&] {
        const auto start = Clock::now();
        acquired = lock.try_lock_for(std::chrono::milliseconds(30));
        waited = Clock::now() - start;
    });
    expiring.join();
    Require(!acquired && waited >= std::chrono::milliseconds(30) && waited < std::chrono::seconds(1), "a timed GuestLock wait did not expire at its deadline");
    std::thread succeeding([&] {
        acquired = lock.try_lock_for(std::chrono::seconds(10));
        if (acquired) lock.unlock();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    lock.unlock();
    succeeding.join();
    Require(acquired, "a timed GuestLock wait missed the release");
}

void CheckSleepDurations() {
    Require(sceKernelUsleep_nid_postfix(0) == 0, "sceKernelUsleep(0) failed");
    for (const KernelUseconds micros : {1u, 20u, 50u}) {
        const auto start = Clock::now();
        Require(sceKernelUsleep_nid_postfix(micros) == 0, "sceKernelUsleep failed");
        Require(Clock::now() - start >= std::chrono::microseconds(micros), "a short sceKernelUsleep returned early");
    }
    Require(sceKernelUsleep_nid_postfix(200) == 0, "sceKernelUsleep(200) failed");
    const KernelTimespec zero{0, 0};
    const KernelTimespec brief{0, 30000};
    KernelTimespec remaining{1, 1};
    Require(sceKernelNanosleep(&zero, &remaining) == 0 && remaining.tv_sec == 0 && remaining.tv_nsec == 0, "sceKernelNanosleep(0) failed");
    const auto start = Clock::now();
    Require(nanosleep_nid_postfix(&brief, nullptr) == 0 && Clock::now() - start >= std::chrono::microseconds(30), "a short nanosleep returned early");
}

#ifdef _WIN32
template <typename TSleep>
void CheckSleepYields(bool yields, const char* failure, TSleep sleep) {
    const DWORD_PTR mask = DWORD_PTR{1} << GetCurrentProcessorNumber();
    const DWORD_PTR previous = SetThreadAffinityMask(GetCurrentThread(), mask);
    Require(previous != 0, "SetThreadAffinityMask failed");
    std::atomic<bool> started{false};
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> rounds{0};
    std::thread helper([&] {
        Require(SetThreadAffinityMask(GetCurrentThread(), mask) != 0, "SetThreadAffinityMask failed");
        started.store(true);
        while (!stop.load(std::memory_order_relaxed)) rounds.fetch_add(1, std::memory_order_relaxed);
    });
    while (!started.load()) Sleep(1);
    int progressed = 0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        const auto before = rounds.load();
        sleep();
        if (rounds.load() != before) ++progressed;
    }
    stop.store(true);
    helper.join();
    SetThreadAffinityMask(GetCurrentThread(), previous);
    Require(yields ? progressed != 0 : progressed == 0, failure);
}
#endif

}

int main() {
    CheckMutualExclusion();
    CheckTimedLock();
    CheckSleepDurations();
    const auto idle = MeasureBlockedWaiter(0);
    const auto contendedNeighbour = MeasureBlockedWaiter(3);
    Require(idle.wakeLatency < std::chrono::milliseconds(100), "a GuestLock waiter was not woken promptly");
    Require(contendedNeighbour.wakeLatency < std::chrono::milliseconds(100), "a GuestLock waiter was not woken promptly next to a contended lock");
#ifdef _WIN32
    Require(idle.cpuShare < 0.025, "a blocked GuestLock waiter used more than 2.5% of a CPU");
    Require(contendedNeighbour.cpuShare < 0.025, "a blocked GuestLock waiter used more than 2.5% of a CPU next to a contended lock");
    CheckSleepYields(false, "sceKernelUsleep(0) gave up its CPU", [] { sceKernelUsleep_nid_postfix(0); });
    CheckSleepYields(false, "sceKernelNanosleep(0) gave up its CPU", [] {
        const KernelTimespec zero{0, 0};
        sceKernelNanosleep(&zero, nullptr);
    });
    CheckSleepYields(true, "sceKernelUsleep(20) did not yield", [] { sceKernelUsleep_nid_postfix(20); });
    CheckSleepYields(true, "a timed GuestLock wait did not yield", [] {
        GuestLock lock;
        lock.lock();
        Require(!lock.try_lock_for(std::chrono::microseconds(20)), "a held GuestLock was taken");
        lock.unlock();
    });
#endif
    std::printf("GuestLock waiter CPU %.3f%% (woken after %lld us), next to a contended lock %.3f%% (woken after %lld us)\n", idle.cpuShare * 100.0, static_cast<long long>(idle.wakeLatency.count()),
        contendedNeighbour.cpuShare * 100.0, static_cast<long long>(contendedNeighbour.wakeLatency.count()));
    return 0;
}

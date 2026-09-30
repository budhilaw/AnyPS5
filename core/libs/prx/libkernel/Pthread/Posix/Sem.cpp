#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {

constexpr int GuestEinval = 22;
constexpr int GuestEagain = 35;
constexpr int GuestEtimedout = 60;
constexpr int GuestEoverflow = 84;
constexpr unsigned SemaphoreValueMax = 0x7fffffffu;

struct Semaphore {
    std::mutex mutex;
    std::condition_variable available;
    unsigned value = 0;
};

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

Semaphore* From(void* sem) { return sem == nullptr ? nullptr : *static_cast<Semaphore**>(sem); }

template<typename Wait>
int Acquire(void* sem, Wait wait) {
    auto* semaphore = From(sem);
    if (semaphore == nullptr) return Fail(GuestEinval);
    std::unique_lock lock(semaphore->mutex);
    if (!wait(lock, *semaphore)) return Fail(GuestEtimedout);
    --semaphore->value;
    return 0;
}

}

extern "C" {

int APS5_VABI sem_init_nid_postfix(void* sem, int pshared, unsigned int value) {
    (void)pshared;
    if (sem == nullptr || value > SemaphoreValueMax) return Fail(GuestEinval);
    auto* semaphore = new Semaphore;
    semaphore->value = value;
    *static_cast<Semaphore**>(sem) = semaphore;
    return 0;
}

int APS5_VABI sem_destroy_nid_postfix(void* sem) {
    auto* semaphore = From(sem);
    if (semaphore == nullptr) return Fail(GuestEinval);
    delete semaphore;
    *static_cast<Semaphore**>(sem) = nullptr;
    return 0;
}

int APS5_VABI sem_wait_nid_postfix(void* sem) {
    return Acquire(sem, [](auto& lock, Semaphore& semaphore) {
        semaphore.available.wait(lock, [&] { return semaphore.value > 0; });
        return true;
    });
}

int APS5_VABI sem_trywait_nid_postfix(void* sem) {
    auto* semaphore = From(sem);
    if (semaphore == nullptr) return Fail(GuestEinval);
    std::lock_guard lock(semaphore->mutex);
    if (semaphore->value == 0) return Fail(GuestEagain);
    --semaphore->value;
    return 0;
}

int APS5_VABI sem_timedwait_nid_postfix(void* sem, const KernelTimespec* abstime) {
    if (abstime == nullptr || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) return Fail(GuestEinval);
    const auto deadline = std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds(abstime->tv_sec) + std::chrono::nanoseconds(abstime->tv_nsec)));
    return Acquire(sem, [&](auto& lock, Semaphore& semaphore) {
        return semaphore.available.wait_until(lock, deadline, [&] { return semaphore.value > 0; });
    });
}

int APS5_VABI sem_reltimedwait_np_nid_postfix(void* sem, uint32_t usec) {
    return Acquire(sem, [&](auto& lock, Semaphore& semaphore) {
        return semaphore.available.wait_for(lock, std::chrono::microseconds(usec), [&] { return semaphore.value > 0; });
    });
}

int APS5_VABI sem_post_nid_postfix(void* sem) {
    auto* semaphore = From(sem);
    if (semaphore == nullptr) return Fail(GuestEinval);
    {
        std::lock_guard lock(semaphore->mutex);
        if (semaphore->value == SemaphoreValueMax) return Fail(GuestEoverflow);
        ++semaphore->value;
    }
    semaphore->available.notify_one();
    return 0;
}

int APS5_VABI sem_getvalue_nid_postfix(void* sem, int* value) {
    auto* semaphore = From(sem);
    if (semaphore == nullptr || value == nullptr) return Fail(GuestEinval);
    std::lock_guard lock(semaphore->mutex);
    *value = static_cast<int>(semaphore->value);
    return 0;
}

}

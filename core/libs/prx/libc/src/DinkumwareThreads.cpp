#include "prx/libc/include/General.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include "prx/libc/include/GuestLock.hpp"

namespace {

enum ThreadResult : int { Success = 0, NoMemory = 1, TimedOut = 2, Busy = 3, Error = 4 };
constexpr int MutexRecursive = 0x100;

struct Xtime { std::int64_t seconds; std::int32_t nanoseconds; };

struct Mutex {
    bool recursive;
    RecursiveGuestLock recursiveMutex;
    GuestLock plainMutex;
    explicit Mutex(bool isRecursive) : recursive(isRecursive) {}
    void lock() { recursive ? recursiveMutex.lock() : plainMutex.lock(); }
    void unlock() { recursive ? recursiveMutex.unlock() : plainMutex.unlock(); }
    bool try_lock() { return recursive ? recursiveMutex.try_lock() : plainMutex.try_lock(); }
    template<class TClock, class TDuration> bool try_lock_until(const std::chrono::time_point<TClock, TDuration>& deadline) { return recursive ? recursiveMutex.try_lock_until(deadline) : plainMutex.try_lock_until(deadline); }
};

struct Condition { std::condition_variable_any variable; };

std::chrono::system_clock::time_point deadline(const Xtime* time) {
    if (time == nullptr) throw std::invalid_argument("xtime: null");
    return std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds(time->seconds) + std::chrono::nanoseconds(time->nanoseconds)));
}

Mutex& mutexOf(void* handle) {
    if (handle == nullptr || *static_cast<void**>(handle) == nullptr) throw std::invalid_argument("_Mtx: null mutex");
    return **static_cast<Mutex**>(handle);
}
Condition& conditionOf(void* handle) {
    if (handle == nullptr || *static_cast<void**>(handle) == nullptr) throw std::invalid_argument("_Cnd: null condition");
    return **static_cast<Condition**>(handle);
}

}

extern "C" {

int APS5_VABI _Mtx_init_nid_postfix(void** mutex, int type) {
    if (mutex == nullptr) throw std::invalid_argument("_Mtx_init: null handle");
    *mutex = new (std::nothrow) Mutex((type & MutexRecursive) != 0);
    return *mutex != nullptr ? Success : NoMemory;
}

void APS5_VABI _Mtx_destroy_nid_postfix(void* mutex) {
    if (mutex == nullptr) return;
    auto*& handle = *static_cast<Mutex**>(mutex);
    delete handle;
    handle = nullptr;
}
int APS5_VABI _Mtx_lock_nid_postfix(void* mutex) { mutexOf(mutex).lock(); return Success; }
int APS5_VABI _Mtx_unlock_nid_postfix(void* mutex) { mutexOf(mutex).unlock(); return Success; }
int APS5_VABI _Mtx_trylock_nid_postfix(void* mutex) { return mutexOf(mutex).try_lock() ? Success : Busy; }
int APS5_VABI _Mtx_timedlock_nid_postfix(void* mutex, const Xtime* time) { return mutexOf(mutex).try_lock_until(deadline(time)) ? Success : TimedOut; }
int APS5_VABI _Mtx_current_owns_nid_postfix(void* mutex) {
    (void)mutex;
    throw std::runtime_error("_Mtx_current_owns is not supported");
}

int APS5_VABI _Cnd_init_nid_postfix(void** condition) {
    if (condition == nullptr) throw std::invalid_argument("_Cnd_init: null handle");
    *condition = new (std::nothrow) Condition();
    return *condition != nullptr ? Success : NoMemory;
}

void APS5_VABI _Cnd_destroy_nid_postfix(void* condition) {
    if (condition == nullptr) return;
    auto*& handle = *static_cast<Condition**>(condition);
    delete handle;
    handle = nullptr;
}
int APS5_VABI _Cnd_signal_nid_postfix(void* condition) { conditionOf(condition).variable.notify_one(); return Success; }
int APS5_VABI _Cnd_broadcast_nid_postfix(void* condition) { conditionOf(condition).variable.notify_all(); return Success; }

int APS5_VABI _Cnd_wait_nid_postfix(void* condition, void* mutex) {
    std::unique_lock<Mutex> lock(mutexOf(mutex), std::adopt_lock);
    conditionOf(condition).variable.wait(lock);
    lock.release();
    return Success;
}

int APS5_VABI _Cnd_timedwait_nid_postfix(void* condition, void* mutex, const Xtime* time) {
    std::unique_lock<Mutex> lock(mutexOf(mutex), std::adopt_lock);
    const auto status = conditionOf(condition).variable.wait_until(lock, deadline(time));
    lock.release();
    return status == std::cv_status::timeout ? TimedOut : Success;
}

int APS5_VABI _Thrd_sleep_nid_postfix(const Xtime* time) { std::this_thread::sleep_until(deadline(time)); return Success; }
int APS5_VABI _Thrd_yield_nid_postfix() { std::this_thread::yield(); return Success; }

std::int64_t APS5_VABI _Xtime_get_ticks_nid_postfix() {
    return std::chrono::duration_cast<std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>>(std::chrono::system_clock::now().time_since_epoch()).count();
}

}

#include "../include/Pthread.hpp"
#include <mutex>
#include <thread>
#include <string>
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = 0x80020062;

extern "C" {

int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr) {
    if (!attr) throw std::runtime_error("scePthreadCondattrInit: null attr");
    auto* p = new (std::nothrow) PthreadCondattrPrivate{0};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadCondattrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char*) {
    if (!cond) throw std::runtime_error("scePthreadCondInit: null cond");
    auto* p = new (std::nothrow) PthreadCondPrivate{};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    if (attr && *attr) p->_clockid = (*attr)->_clockid;
    *cond = p;
    return SCE_OK;
}

int APS5_VABI scePthreadCondDestroy(PthreadCond* cond) {
    if (!cond || !*cond) throw std::runtime_error("scePthreadCondDestroy: null cond");
    delete *cond;
    *cond = nullptr;
    return SCE_OK;
}

// PTHREAD_COND_INITIALIZER is a null handle; the first use creates the condition variable.
static PthreadCondPrivate* EnsureCond(PthreadCond* cond, const char* caller) {
    if (!cond) throw std::runtime_error(std::string(caller) + ": null cond");
    if (*cond) return *cond;
    static std::mutex initialization;
    std::lock_guard lock(initialization);
    if (!*cond) {
        auto* p = new (std::nothrow) PthreadCondPrivate{};
        if (!p) throw std::runtime_error(std::string(caller) + ": cannot initialize a static cond");
        *cond = p;
    }
    return *cond;
}

int APS5_VABI scePthreadCondSignal(PthreadCond* cond) {
    EnsureCond(cond, "scePthreadCondSignal")->_cv.notify_one();
    return SCE_OK;
}

int APS5_VABI scePthreadCondBroadcast(PthreadCond* cond) {
    EnsureCond(cond, "scePthreadCondBroadcast")->_cv.notify_all();
    return SCE_OK;
}

// The wait releases the mutex entirely (a recursive one at every depth) and takes it back for
// this thread with its depth.
namespace {
struct HeldMutex {
    PthreadMutexPrivate* mutex;
    int depth = 0;
    void unlock() {
        depth = mutex->_count;
        mutex->_count = 0;
        mutex->_owner.store(std::thread::id{}, std::memory_order_relaxed);
        MutexClearOwner(mutex);
        mutex->_lock.unlock();
    }
    void lock() {
        mutex->_lock.lock();
        mutex->_owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
        mutex->_count = depth;
        MutexNoteOwner(mutex);
    }
};
}

int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, unsigned int usec) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadCondTimedwait: null mutex");
    auto* c = EnsureCond(cond, "scePthreadCondTimedwait");
    HeldMutex held{*mutex};
    std::unique_lock<HeldMutex> lk(held, std::adopt_lock);
    const auto res = c->_cv.wait_for(lk, std::chrono::microseconds(usec));
    lk.release();
    return res == std::cv_status::timeout ? SCE_KERNEL_ERROR_ETIMEDOUT : SCE_OK;
}

int APS5_VABI scePthreadCondSignalto(PthreadCond* cond, Pthread thread) {
 (void)cond;
 (void)thread;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadCondWait: null mutex");
    auto* c = EnsureCond(cond, "scePthreadCondWait");
    HeldMutex held{*mutex};
    std::unique_lock<HeldMutex> lk(held, std::adopt_lock);
    c->_cv.wait(lk);
    lk.release();
    return SCE_OK;
}

}

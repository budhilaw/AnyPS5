#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <condition_variable>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

struct PthreadRwlockPrivate {
    std::mutex mutex;
    std::condition_variable readersReady;
    std::condition_variable writersReady;
    unsigned readers = 0;
    unsigned waitingWriters = 0;
    std::thread::id writer{};
};

struct PthreadRwlockattrPrivate {
    int type = 1;
};

namespace {

constexpr int SCE_OK = 0;
constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
constexpr int SCE_KERNEL_ERROR_EDEADLK = 0x8002000B;
constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;

unsigned& HeldReads(PthreadRwlockPrivate* lock) {
    thread_local std::unordered_map<PthreadRwlockPrivate*, unsigned> held;
    return held[lock];
}

PthreadRwlockPrivate* Ensure(PthreadRwlock* rwlock, const char* caller) {
    if (!rwlock) throw std::runtime_error(std::string(caller) + ": null rwlock");
    if (*rwlock) return *rwlock;
    static std::mutex initialization;
    std::lock_guard guard(initialization);
    if (!*rwlock) {
        auto* p = new (std::nothrow) PthreadRwlockPrivate();
        if (!p) throw std::runtime_error(std::string(caller) + ": cannot initialize a static rwlock");
        *rwlock = p;
    }
    return *rwlock;
}

bool ReadAvailable(const PthreadRwlockPrivate& lock, unsigned heldReads) {
    return lock.writer == std::thread::id{} && (lock.waitingWriters == 0 || heldReads != 0);
}

bool WriteAvailable(const PthreadRwlockPrivate& lock) {
    return lock.writer == std::thread::id{} && lock.readers == 0;
}

int ReadLock(PthreadRwlock* rwlock, const char* caller, bool blocking) {
    auto* p = Ensure(rwlock, caller);
    auto& heldReads = HeldReads(p);
    std::unique_lock guard(p->mutex);
    if (p->writer == std::this_thread::get_id()) return SCE_KERNEL_ERROR_EDEADLK;
    if (!ReadAvailable(*p, heldReads)) {
        if (!blocking) return SCE_KERNEL_ERROR_EBUSY;
        p->readersReady.wait(guard, [&] { return ReadAvailable(*p, heldReads); });
    }
    ++p->readers;
    ++heldReads;
    return SCE_OK;
}

int WriteLock(PthreadRwlock* rwlock, const char* caller, bool blocking) {
    auto* p = Ensure(rwlock, caller);
    std::unique_lock guard(p->mutex);
    if (p->writer == std::this_thread::get_id()) return SCE_KERNEL_ERROR_EDEADLK;
    if (!WriteAvailable(*p)) {
        if (!blocking) return SCE_KERNEL_ERROR_EBUSY;
        ++p->waitingWriters;
        p->writersReady.wait(guard, [&] { return WriteAvailable(*p); });
        --p->waitingWriters;
    }
    p->writer = std::this_thread::get_id();
    return SCE_OK;
}

}

extern "C" {

int APS5_VABI scePthreadRwlockInit(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char*) {
    if (!rwlock) throw std::runtime_error("scePthreadRwlockInit: null rwlock");
    if (attr && !*attr) return SCE_KERNEL_ERROR_EINVAL;
    auto* p = new (std::nothrow) PthreadRwlockPrivate();
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *rwlock = p;
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockDestroy(PthreadRwlock* rwlock) {
    if (!rwlock || !*rwlock) throw std::runtime_error("scePthreadRwlockDestroy: null rwlock");
    {
        std::lock_guard guard((*rwlock)->mutex);
        if ((*rwlock)->readers != 0 || (*rwlock)->writer != std::thread::id{} || (*rwlock)->waitingWriters != 0) return SCE_KERNEL_ERROR_EBUSY;
    }
    delete *rwlock;
    *rwlock = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockRdlock(PthreadRwlock* rwlock) { return ReadLock(rwlock, "scePthreadRwlockRdlock", true); }
int APS5_VABI scePthreadRwlockWrlock(PthreadRwlock* rwlock) { return WriteLock(rwlock, "scePthreadRwlockWrlock", true); }
int APS5_VABI scePthreadRwlockTryrdlock(PthreadRwlock* rwlock) { return ReadLock(rwlock, "scePthreadRwlockTryrdlock", false); }
int APS5_VABI scePthreadRwlockTrywrlock(PthreadRwlock* rwlock) { return WriteLock(rwlock, "scePthreadRwlockTrywrlock", false); }

int APS5_VABI scePthreadRwlockUnlock(PthreadRwlock* rwlock) {
    auto* p = Ensure(rwlock, "scePthreadRwlockUnlock");
    auto& heldReads = HeldReads(p);
    std::unique_lock guard(p->mutex);
    if (p->writer == std::this_thread::get_id()) {
        p->writer = std::thread::id{};
    } else if (heldReads != 0) {
        --heldReads;
        --p->readers;
        if (p->readers != 0) return SCE_OK;
    } else {
        return SCE_KERNEL_ERROR_EPERM;
    }
    guard.unlock();
    p->writersReady.notify_one();
    p->readersReady.notify_all();
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockattrInit(PthreadRwlockattr* attr) {
    if (!attr) throw std::runtime_error("scePthreadRwlockattrInit: null attr");
    auto* p = new (std::nothrow) PthreadRwlockattrPrivate();
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockattrDestroy(PthreadRwlockattr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadRwlockattrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockattrSettype(PthreadRwlockattr* attr, int type) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadRwlockattrSettype: null attr");
    if (type != 1) throw std::runtime_error("scePthreadRwlockattrSettype: unsupported type " + std::to_string(type));
    (*attr)->type = type;
    return SCE_OK;
}

}

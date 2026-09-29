#include "../include/Pthread.hpp"
#include <mutex>
#include <string>
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <cstdlib>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_EDEADLK = 0x80020023;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = 0x8002003C;

extern "C" Pthread APS5_VABI scePthreadSelf();

static bool TraceLocks() {
    static const bool enabled = std::getenv("ANYPS5_TRACE_LOCKS") != nullptr;
    return enabled;
}

static const char* ThreadName(PthreadPrivate* thread) {
    return thread && !thread->name.empty() ? thread->name.c_str() : "?";
}

template <typename Mutex>
static void LockReporting(Mutex& native, PthreadMutexPrivate* m) {
    if (!TraceLocks()) {
        native.lock();
        return;
    }
    if (native.try_lock_for(std::chrono::seconds(5))) return;
    auto* self = scePthreadSelf();
    APS5_LOG_OUT("mutex %p (type %d, count %d): thread '%s' (%p) has waited 5 s; held by '%s' (%p)", static_cast<void*>(m), static_cast<int>(m->_type), m->_count, ThreadName(self), static_cast<void*>(self),
        ThreadName(m->_ownerThread.load(std::memory_order_acquire)), static_cast<void*>(m->_ownerThread.load(std::memory_order_acquire)));
    native.lock();
    APS5_LOG_OUT("mutex %p: thread '%s' acquired it", static_cast<void*>(m), ThreadName(self));
}

void MutexNoteOwner(PthreadMutexPrivate* m) {
    if (TraceLocks()) m->_ownerThread.store(scePthreadSelf(), std::memory_order_release);
}

void MutexClearOwner(PthreadMutexPrivate* m) {
    if (TraceLocks()) m->_ownerThread.store(nullptr, std::memory_order_release);
}

extern "C" {

int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr) {
    if (!attr) throw std::runtime_error("scePthreadMutexattrInit: null attr");
    auto* p = new (std::nothrow) PthreadMutexattrPrivate{MutexType::Normal};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadMutexattrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadMutexattrSettype: null attr");
    if (TraceLocks()) APS5_LOG_OUT("settype trace: type %d caller %p", type, __builtin_return_address(0));
    switch (type) {
    case 1: (*attr)->type = MutexType::ErrorCheck; break;
    case 2: (*attr)->type = MutexType::Recursive; break;
    case 3: (*attr)->type = MutexType::Normal; break;
    case 4: (*attr)->type = MutexType::Adaptive; break;
    default: throw std::runtime_error("scePthreadMutexattrSettype: invalid type " + std::to_string(type));
    }
    return SCE_OK;
}

int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char*) {
    if (!mutex) throw std::runtime_error("scePthreadMutexInit: null mutex");
    MutexType t = MutexType::Normal;
    if (attr && *attr) t = (*attr)->type;
    auto* p = new (std::nothrow) PthreadMutexPrivate();
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    p->_type = t;
    *mutex = p;
    if (TraceLocks()) APS5_LOG_OUT("init trace: mutex %p type %d attr %d caller %p", static_cast<void*>(p), static_cast<int>(t), attr && *attr ? 1 : 0, __builtin_return_address(0));
    return SCE_OK;
}

int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadMutexDestroy: null mutex");
    delete *mutex;
    *mutex = nullptr;
    return SCE_OK;
}

static PthreadMutexPrivate* EnsureMutex(PthreadMutex* mutex, const char* caller) {
    if (!mutex) throw std::runtime_error(std::string(caller) + ": null mutex");
    if (*mutex) return *mutex;
    static std::mutex initialization;
    std::lock_guard lock(initialization);
    if (!*mutex) {
        auto* p = new (std::nothrow) PthreadMutexPrivate();
        if (!p) throw std::runtime_error(std::string(caller) + ": cannot initialize a static mutex");
        *mutex = p;
    }
    return *mutex;
}

int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex) {
    auto* m = EnsureMutex(mutex, "scePthreadMutexLock");
    const auto tid = std::this_thread::get_id();
    if (m->_type == MutexType::Recursive) {
        if (m->_owner.load(std::memory_order_relaxed) == tid) {
            ++m->_count;
            return SCE_OK;
        }
        LockReporting(m->_lock, m);
        m->_owner.store(tid, std::memory_order_relaxed);
        MutexNoteOwner(m);
        m->_count = 1;
        return SCE_OK;
    }
    if (m->_type == MutexType::ErrorCheck) {
        if (m->_owner.load(std::memory_order_acquire) == tid) return SCE_KERNEL_ERROR_EDEADLK;
    }
    LockReporting(m->_lock, m);
    m->_owner.store(tid, std::memory_order_relaxed);
    MutexNoteOwner(m);
    return SCE_OK;
}

int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadMutexUnlock: null mutex");
    auto* m = *mutex;
    if (m->_owner.load(std::memory_order_relaxed) != std::this_thread::get_id()) return SCE_KERNEL_ERROR_EPERM;
    if (m->_type == MutexType::Recursive && --m->_count != 0) return SCE_OK;
    m->_owner.store(std::thread::id{}, std::memory_order_relaxed);
    MutexClearOwner(m);
    m->_lock.unlock();
    return SCE_OK;
}

int APS5_VABI scePthreadMutexTimedlock(PthreadMutex* mutex, KernelUseconds usec) {
    auto* m = EnsureMutex(mutex, "scePthreadMutexTimedlock");
    const auto tid = std::this_thread::get_id();
    if (m->_type == MutexType::Recursive && m->_owner.load(std::memory_order_relaxed) == tid) {
        ++m->_count;
        return SCE_OK;
    }
    if (m->_type == MutexType::ErrorCheck && m->_owner.load(std::memory_order_acquire) == tid) return SCE_KERNEL_ERROR_EDEADLK;
    if (!m->_lock.try_lock_for(std::chrono::microseconds(usec))) return SCE_KERNEL_ERROR_ETIMEDOUT;
    m->_owner.store(tid, std::memory_order_relaxed);
    MutexNoteOwner(m);
    if (m->_type == MutexType::Recursive) m->_count = 1;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex) {
    auto* m = EnsureMutex(mutex, "scePthreadMutexTrylock");
    const auto tid = std::this_thread::get_id();
    if (m->_type == MutexType::Recursive && m->_owner.load(std::memory_order_relaxed) == tid) {
        ++m->_count;
        return SCE_OK;
    }
    if (m->_type == MutexType::ErrorCheck && m->_owner.load(std::memory_order_acquire) == tid) return SCE_KERNEL_ERROR_EDEADLK;
    if (!m->_lock.try_lock()) return SCE_KERNEL_ERROR_EBUSY;
    m->_owner.store(tid, std::memory_order_relaxed);
    MutexNoteOwner(m);
    if (m->_type == MutexType::Recursive) m->_count = 1;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadMutexattrSetprotocol: null attr");
    if (protocol != 0 && protocol != 1) throw std::runtime_error("scePthreadMutexattrSetprotocol: unsupported protocol " + std::to_string(protocol));
    (*attr)->protocol = protocol;
    return SCE_OK;
}

}

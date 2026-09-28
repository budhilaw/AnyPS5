#include <cstdio>
#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>

// POSIX pthread entry points of libScePosix, implemented over the scePthread* functions. POSIX
// returns errno values where the Sony API returns SCE_KERNEL_ERROR codes (0x8002 << 16 | errno).
extern "C" {
int APS5_VABI scePthreadMutexInit(PthreadMutex*, const PthreadMutexattr*, const char*);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex*);
int APS5_VABI scePthreadMutexLock(PthreadMutex*);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex*);
int APS5_VABI scePthreadMutexTrylock(PthreadMutex*);
int APS5_VABI scePthreadMutexTimedlock(PthreadMutex*, KernelUseconds);
int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr*);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr*);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr*, int);
int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr*, int);
int APS5_VABI scePthreadCondInit(PthreadCond*, const PthreadCondattr*, const char*);
int APS5_VABI scePthreadCondDestroy(PthreadCond*);
int APS5_VABI scePthreadCondSignal(PthreadCond*);
int APS5_VABI scePthreadCondBroadcast(PthreadCond*);
int APS5_VABI scePthreadCondWait(PthreadCond*, PthreadMutex*);
int APS5_VABI scePthreadCondTimedwait(PthreadCond*, PthreadMutex*, unsigned int);
int APS5_VABI scePthreadCondattrInit(PthreadCondattr*);
int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr*);
int APS5_VABI scePthreadRwlockInit(PthreadRwlock*, const PthreadRwlockattr*, const char*);
int APS5_VABI scePthreadRwlockDestroy(PthreadRwlock*);
int APS5_VABI scePthreadRwlockRdlock(PthreadRwlock*);
int APS5_VABI scePthreadRwlockWrlock(PthreadRwlock*);
int APS5_VABI scePthreadRwlockTryrdlock(PthreadRwlock*);
int APS5_VABI scePthreadRwlockTrywrlock(PthreadRwlock*);
int APS5_VABI scePthreadRwlockUnlock(PthreadRwlock*);
int APS5_VABI scePthreadAttrInit(PthreadAttr*);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr*);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr*, std::size_t);
int APS5_VABI scePthreadAttrSetdetachstate(PthreadAttr*, int);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr*, const KernelSchedParam*);
int APS5_VABI scePthreadCreate(Pthread*, const PthreadAttr*, PthreadEntry, void*, const char*);
int APS5_VABI scePthreadJoin(Pthread, void**);
int APS5_VABI scePthreadDetach(Pthread);
void APS5_VABI scePthreadExit(void*);
Pthread APS5_VABI scePthreadSelf();
}

namespace {

constexpr int EInval = 22, ESrch = 3;

int ToErrno(int sce) { return sce == 0 ? 0 : sce & 0xff; }

// Microseconds until an absolute time on the given clock (0 realtime, 4 monotonic); 0 when past.
unsigned int Remaining(const KernelTimespec* absolute, int clock) {
    if (absolute == nullptr || absolute->tv_nsec < 0 || absolute->tv_nsec >= 1000000000) throw std::invalid_argument("pthread: invalid absolute time");
    const auto target = std::chrono::seconds(absolute->tv_sec) + std::chrono::nanoseconds(absolute->tv_nsec);
    const auto now = clock == 4 ? std::chrono::steady_clock::now().time_since_epoch() : std::chrono::system_clock::now().time_since_epoch();
    if (target <= now) return 0;
    const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(target - now).count();
    return remaining > 0xffffffffll ? 0xffffffffu : static_cast<unsigned int>(remaining);
}

PthreadAttrPrivate* Attr(const PthreadAttr* attr, const char* caller) {
    if (!attr || !*attr) throw std::runtime_error(std::string(caller) + ": null attr");
    return *attr;
}

}

extern "C" {

// Mutexes
int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr) { return ToErrno(scePthreadMutexInit(mutex, attr, nullptr)); }
int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex) { return ToErrno(scePthreadMutexDestroy(mutex)); }
int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex) { return ToErrno(scePthreadMutexLock(mutex)); }
int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex) { return ToErrno(scePthreadMutexUnlock(mutex)); }
int APS5_VABI pthread_mutex_trylock_nid_postfix(PthreadMutex* mutex) { return ToErrno(scePthreadMutexTrylock(mutex)); }
int APS5_VABI pthread_mutex_timedlock_nid_postfix(PthreadMutex* mutex, const KernelTimespec* absolute) { return ToErrno(scePthreadMutexTimedlock(mutex, Remaining(absolute, 0))); }
int APS5_VABI pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr) { return ToErrno(scePthreadMutexattrInit(attr)); }
int APS5_VABI pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr) { return ToErrno(scePthreadMutexattrDestroy(attr)); }
int APS5_VABI pthread_mutexattr_settype_nid_postfix(PthreadMutexattr* attr, int type) { return ToErrno(scePthreadMutexattrSettype(attr, type)); }
int APS5_VABI pthread_mutexattr_setprotocol_nid_postfix(PthreadMutexattr* attr, int protocol) { return ToErrno(scePthreadMutexattrSetprotocol(attr, protocol)); }

// Condition variables
int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) { return ToErrno(scePthreadCondInit(cond, attr, nullptr)); }
int APS5_VABI pthread_cond_destroy_nid_postfix(PthreadCond* cond) { return ToErrno(scePthreadCondDestroy(cond)); }
int APS5_VABI pthread_cond_signal_nid_postfix(PthreadCond* cond) { return ToErrno(scePthreadCondSignal(cond)); }
int APS5_VABI pthread_cond_broadcast_nid_postfix(PthreadCond* cond) { return ToErrno(scePthreadCondBroadcast(cond)); }
int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) { return ToErrno(scePthreadCondWait(cond, mutex)); }
int APS5_VABI pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* absolute) {
    const int clock = cond && *cond ? (*cond)->_clockid : 0;
    return ToErrno(scePthreadCondTimedwait(cond, mutex, Remaining(absolute, clock)));
}
int APS5_VABI pthread_condattr_init_nid_postfix(PthreadCondattr* attr) { return ToErrno(scePthreadCondattrInit(attr)); }
int APS5_VABI pthread_condattr_destroy_nid_postfix(PthreadCondattr* attr) { return ToErrno(scePthreadCondattrDestroy(attr)); }
int APS5_VABI pthread_condattr_setclock_nid_postfix(PthreadCondattr* attr, KernelClockid clock) {
    if (!attr || !*attr) throw std::runtime_error("pthread_condattr_setclock: null attr");
    if (clock != 0 && clock != 4) return EInval; // CLOCK_REALTIME, CLOCK_MONOTONIC
    (*attr)->_clockid = clock;
    return 0;
}

// Reader-writer locks
int APS5_VABI pthread_rwlock_init_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) { return ToErrno(scePthreadRwlockInit(rwlock, attr, nullptr)); }
int APS5_VABI pthread_rwlock_destroy_nid_postfix(PthreadRwlock* rwlock) { return ToErrno(scePthreadRwlockDestroy(rwlock)); }
int APS5_VABI pthread_rwlock_rdlock_nid_postfix(PthreadRwlock* rwlock) { return ToErrno(scePthreadRwlockRdlock(rwlock)); }
int APS5_VABI pthread_rwlock_wrlock_nid_postfix(PthreadRwlock* rwlock) { return ToErrno(scePthreadRwlockWrlock(rwlock)); }
int APS5_VABI pthread_rwlock_tryrdlock_nid_postfix(PthreadRwlock* rwlock) { return ToErrno(scePthreadRwlockTryrdlock(rwlock)); }
int APS5_VABI pthread_rwlock_trywrlock_nid_postfix(PthreadRwlock* rwlock) { return ToErrno(scePthreadRwlockTrywrlock(rwlock)); }
int APS5_VABI pthread_rwlock_unlock_nid_postfix(PthreadRwlock* rwlock) { return ToErrno(scePthreadRwlockUnlock(rwlock)); }

// Thread attributes
int APS5_VABI pthread_attr_init_nid_postfix(PthreadAttr* attr) { return ToErrno(scePthreadAttrInit(attr)); }
int APS5_VABI pthread_attr_destroy_nid_postfix(PthreadAttr* attr) { return ToErrno(scePthreadAttrDestroy(attr)); }
int APS5_VABI pthread_attr_setstacksize_nid_postfix(PthreadAttr* attr, std::size_t size) { return ToErrno(scePthreadAttrSetstacksize(attr, size)); }
int APS5_VABI pthread_attr_getstacksize_nid_postfix(const PthreadAttr* attr, std::size_t* size) { if (!size) return EInval; *size = Attr(attr, "pthread_attr_getstacksize")->_stacksize; return 0; }
int APS5_VABI pthread_attr_setdetachstate_nid_postfix(PthreadAttr* attr, int state) { return ToErrno(scePthreadAttrSetdetachstate(attr, state)); }
int APS5_VABI pthread_attr_getdetachstate_nid_postfix(const PthreadAttr* attr, int* state) { if (!state) return EInval; *state = Attr(attr, "pthread_attr_getdetachstate")->_detachstate; return 0; }
int APS5_VABI pthread_attr_setguardsize_nid_postfix(PthreadAttr* attr, std::size_t size) { Attr(attr, "pthread_attr_setguardsize")->_guardsize = size; return 0; }
int APS5_VABI pthread_attr_getguardsize_nid_postfix(const PthreadAttr* attr, std::size_t* size) { if (!size) return EInval; *size = Attr(attr, "pthread_attr_getguardsize")->_guardsize; return 0; }
int APS5_VABI pthread_attr_setschedparam_nid_postfix(PthreadAttr* attr, const KernelSchedParam* param) { return ToErrno(scePthreadAttrSetschedparam(attr, param)); }
int APS5_VABI pthread_attr_getschedparam_nid_postfix(const PthreadAttr* attr, KernelSchedParam* param) { if (!param) return EInval; param->sched_priority = Attr(attr, "pthread_attr_getschedparam")->_schedpriority; return 0; }
int APS5_VABI pthread_attr_setschedpolicy_nid_postfix(PthreadAttr* attr, int policy) {
    if (policy < 0 || policy > 3) return EInval; // SCHED_FIFO, SCHED_OTHER, SCHED_RR on the console's numbering
    Attr(attr, "pthread_attr_setschedpolicy")->_schedpolicy = policy;
    return 0;
}
int APS5_VABI pthread_attr_getschedpolicy_nid_postfix(const PthreadAttr* attr, int* policy) { if (!policy) return EInval; *policy = Attr(attr, "pthread_attr_getschedpolicy")->_schedpolicy; return 0; }
int APS5_VABI pthread_attr_setinheritsched_nid_postfix(PthreadAttr* attr, int inherit) {
    if (inherit != 0 && inherit != 4) return EInval; // PTHREAD_INHERIT_SCHED (4) and PTHREAD_EXPLICIT_SCHED (0) on FreeBSD
    Attr(attr, "pthread_attr_setinheritsched")->_inheritsched = inherit;
    return 0;
}
int APS5_VABI pthread_attr_getstack_nid_postfix(const PthreadAttr* attr, void** address, std::size_t* size) {
    if (!address || !size) return EInval;
    auto* p = Attr(attr, "pthread_attr_getstack");
    *address = p->stackAddress;
    *size = p->_stacksize;
    return 0;
}
int APS5_VABI pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr) {
    if (!thread) return ESrch;
    auto* p = Attr(attr, "pthread_attr_get_np");
    p->stackAddress = thread->stackAddress;
    p->_stacksize = thread->stackSize;
    p->_detachstate = thread->_detached ? 1 : 0;
    p->_schedpriority = thread->schedPriority;
    p->_schedpolicy = thread->schedPolicy;
    return 0;
}

// Threads
int APS5_VABI pthread_create_nid_postfix(Pthread* thread, const PthreadAttr* attr, pthread_entry_func_t entry, void* arg) { return ToErrno(scePthreadCreate(thread, attr, reinterpret_cast<PthreadEntry>(entry), arg, nullptr)); }
int APS5_VABI pthread_create_name_np_nid_postfix(Pthread* thread, const PthreadAttr* attr, pthread_entry_func_t entry, void* arg, const char* name) { return ToErrno(scePthreadCreate(thread, attr, reinterpret_cast<PthreadEntry>(entry), arg, name)); }
int APS5_VABI pthread_join_nid_postfix(Pthread thread, void** value) { return ToErrno(scePthreadJoin(thread, value)); }
int APS5_VABI pthread_detach_nid_postfix(Pthread thread) { return ToErrno(scePthreadDetach(thread)); }
void APS5_VABI pthread_exit_nid_postfix(void* value) { scePthreadExit(value); }
Pthread APS5_VABI pthread_self_nid_postfix() { return scePthreadSelf(); }
int APS5_VABI pthread_equal_nid_postfix(Pthread a, Pthread b) { return a == b ? 1 : 0; }
int APS5_VABI pthread_yield_nid_postfix() { std::this_thread::yield(); return 0; }
int APS5_VABI sched_yield_nid_postfix() { std::this_thread::yield(); return 0; }
int APS5_VABI pthread_rename_np_nid_postfix(Pthread thread, const char* name) {
    if (!thread) return ESrch;
    if (!name) return EInval;
    thread->name = name;
    return 0;
}
int APS5_VABI pthread_getname_np_nid_postfix(Pthread thread, char* name, std::size_t size) {
    if (!thread) return ESrch;
    if (!name || size == 0) return EInval;
    std::snprintf(name, size, "%s", thread->name.c_str());
    return 0;
}
int APS5_VABI pthread_setschedparam_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param) {
    if (!thread) return ESrch;
    if (!param || policy < 0 || policy > 3) return EInval;
    thread->schedPolicy = policy;
    thread->schedPriority = param->sched_priority;
    return 0;
}
int APS5_VABI pthread_getschedparam_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param) {
    if (!thread) return ESrch;
    if (!policy || !param) return EInval;
    *policy = thread->schedPolicy;
    param->sched_priority = thread->schedPriority;
    return 0;
}
int APS5_VABI pthread_setprio_nid_postfix(Pthread thread, int priority) {
    if (!thread) return ESrch;
    thread->schedPriority = priority;
    return 0;
}
int APS5_VABI pthread_setcancelstate_nid_postfix(int state, int* previous) {
    if (state != 0 && state != 1) return EInval;
    auto* self = scePthreadSelf();
    if (self == nullptr) throw std::runtime_error("pthread_setcancelstate: no current guest thread");
    if (previous) *previous = self->cancelState;
    self->cancelState = state;
    return 0;
}


}

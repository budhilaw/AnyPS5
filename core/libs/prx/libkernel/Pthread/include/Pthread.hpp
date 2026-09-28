#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP

#include <sched.h>
#include "SceTypes.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#ifndef _WIN32
#include <pthread.h>
#endif

enum class MutexType : std::uint32_t {
    ErrorCheck = 1,
    Recursive = 2,
    Normal = 3,
};

struct PthreadMutexattrPrivate {
    MutexType type;
    int protocol = 0; // PTHREAD_PRIO_NONE; priority inheritance is a scheduling hint the host cannot honour
};

struct PthreadPrivate;
struct PthreadMutexPrivate {
    std::recursive_timed_mutex _rmtx;
    std::timed_mutex _mtx;
    MutexType _type;
    std::atomic<std::thread::id> _owner;
    std::atomic<PthreadPrivate*> _ownerThread{nullptr}; // for the ANYPS5_TRACE_LOCKS stall report only
    int _count;

    PthreadMutexPrivate() : _type(MutexType::Normal), _count(0) {}
};

struct PthreadCondattrPrivate {
    int _clockid;
};

struct PthreadCondPrivate {
    std::condition_variable_any _cv;
    int _clockid = 0; // CLOCK_REALTIME; pthread_condattr_setclock may select CLOCK_MONOTONIC (4)
};

struct PthreadAttrPrivate {
    void* stackAddress = nullptr;
    std::size_t _stacksize;
    int _detachstate;
    int _schedpriority;
    int _schedpolicy;
    int _inheritsched;
    std::size_t _guardsize = 0x4000;
    std::uint64_t _affinity = 0x7f;
    int _solosched = 0;
};

struct PthreadPrivate {
#ifdef _WIN32
    void* nativeHandle = nullptr;
    std::thread::id threadId;
    std::atomic<unsigned> references{2};
#else
    pthread_t native{};     // host thread; valid until joined or detached
    bool joinable = false;
#endif
    void* stackAddress = nullptr;
    std::size_t stackSize = 0;
    std::atomic<bool> _finished;
    void* _retval;
    bool _detached;
    std::mutex _join_mtx;
    std::condition_variable _join_cv;
    std::string name;
    int schedPolicy = 2;      // SCHED_OTHER on the console's FreeBSD numbering
    int schedPriority = 700;  // SCE_KERNEL_PRIO_FIFO_DEFAULT
    int cancelState = 0;      // PTHREAD_CANCEL_ENABLE; cancellation itself is not supported
    int cancelType = 0;
    std::uint64_t affinity = 0x7f; // all seven title cores; a placement hint the host scheduler does not take
    std::uint64_t threadId = 0;
    std::atomic<std::uint32_t> pendingSignals{0}; // guest signals raised at this thread, not yet delivered

    PthreadPrivate() : _finished(false), _retval(nullptr), _detached(false) {}
};

// Ownership bookkeeping for the ANYPS5_TRACE_LOCKS stall report (no-ops when tracing is off).
void MutexNoteOwner(PthreadMutexPrivate* mutex);
void MutexClearOwner(PthreadMutexPrivate* mutex);

#endif

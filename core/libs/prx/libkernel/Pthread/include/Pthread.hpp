#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP

#include <sched.h>
#include "SceTypes.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include "prx/libc/include/GuestLock.hpp"
#include <string>
#include <thread>
#ifndef _WIN32
#include <pthread.h>
#endif

enum class MutexType : std::uint32_t {
    ErrorCheck = 1,
    Recursive = 2,
    Normal = 3,
    Adaptive = 4,
};

struct PthreadMutexattrPrivate {
    MutexType type;
    int protocol = 0;
};

struct PthreadPrivate;

struct PthreadMutexPrivate {
    GuestLock _lock;
    MutexType _type;
    std::atomic<std::thread::id> _owner;
    std::atomic<PthreadPrivate*> _ownerThread{nullptr};
    int _count;

    PthreadMutexPrivate() : _type(MutexType::Normal), _count(0) {}
};

struct PthreadCondattrPrivate {
    int _clockid;
};

struct PthreadCondPrivate {
    std::condition_variable_any _cv;
    int _clockid = 0;
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
    pthread_t native{};
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
    int schedPolicy = 2;
    int schedPriority = 700;
    int cancelState = 0;
    int cancelType = 0;
    std::uint64_t affinity = 0x7f;
    std::uint64_t threadId = 0;
    std::atomic<std::uint32_t> pendingSignals{0};

    PthreadPrivate() : _finished(false), _retval(nullptr), _detached(false) {}
};

void MutexNoteOwner(PthreadMutexPrivate* mutex);
void MutexClearOwner(PthreadMutexPrivate* mutex);

#endif

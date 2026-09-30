#include "../include/Pthread.hpp"
#include "../include/GuestTls.hpp"
#include <cstdio>
static constexpr int SCE_KERNEL_ERROR_ESRCH = 0x80020003;
#include "prx/libc/include/General.hpp"
#include <cstdlib>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <system_error>

#ifndef _WIN32
#include <pthread.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#endif

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;

static constexpr std::size_t DEFAULT_STACK_SIZE = 1u << 20;
static constexpr int DETACH_DETACHED = 1;

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <limits>
#endif

struct ThreadArgs {
    PthreadEntry entry;
    void* arg;
    PthreadPrivate* self;
};

static void FinishThread(PthreadPrivate* self, void* retval) {
    {
        std::unique_lock<std::mutex> lk(self->_join_mtx);
        self->_retval = retval;
        self->_finished.store(true, std::memory_order_release);
    }
    self->_join_cv.notify_all();
}

static thread_local PthreadPrivate* currentThread = nullptr;

#ifndef _WIN32
static void RecordHostStack(PthreadPrivate* self) {
#if defined(__APPLE__)
    const auto high = reinterpret_cast<std::uintptr_t>(pthread_get_stackaddr_np(pthread_self()));
    const auto size = pthread_get_stacksize_np(pthread_self());
    self->stackSize = size;
    self->stackAddress = reinterpret_cast<void*>(high - size);
    std::uint64_t id = 0;
    pthread_threadid_np(nullptr, &id);
    self->threadId = id;
#else
    pthread_attr_t attr;
    if (pthread_getattr_np(pthread_self(), &attr) == 0) {
        void* address = nullptr;
        std::size_t size = 0;
        pthread_attr_getstack(&attr, &address, &size);
        pthread_attr_destroy(&attr);
        self->stackAddress = address;
        self->stackSize = size;
    }
    self->threadId = static_cast<std::uint64_t>(syscall(SYS_gettid));
#endif
}
#endif

static void RunThread(std::unique_ptr<ThreadArgs> args) {
    APS5_LOG_OUT("RunThread entry=0x%llx arg=%p self=%p", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(args->entry)), args->arg, static_cast<void*>(args->self));
    const auto entry = args->entry;
    void* arg = args->arg;
    PthreadPrivate* self = args->self;
    args.reset();
#ifdef __APPLE__
    GuestTls::InstallCurrentThread();
    if (!self->name.empty() && std::getenv("ANYPS5_NO_THREAD_NAMES") == nullptr) pthread_setname_np(self->name.c_str());
#endif
#ifndef _WIN32
    RecordHostStack(self);
    currentThread = self;
#endif
    FinishThread(self, entry(arg));
#ifndef _WIN32
    currentThread = nullptr;
#endif
}

#ifndef _WIN32
struct PosixThreadArgs {
    std::unique_ptr<ThreadArgs> guest;
    std::future<bool> ready;
};

static void* StartPosixThread(void* opaque) {
    std::unique_ptr<PosixThreadArgs> args(static_cast<PosixThreadArgs*>(opaque));
    if (args->ready.get()) RunThread(std::move(args->guest));
    return nullptr;
}
#endif

#ifdef _WIN32

static int HostThreadPriority(int guestPriority) {
    if (guestPriority <= 300) return THREAD_PRIORITY_ABOVE_NORMAL;
    if (guestPriority <= 700) return THREAD_PRIORITY_NORMAL;
    return THREAD_PRIORITY_LOWEST;
}

static void ReleaseThread(PthreadPrivate* thread) {
    if (thread->references.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
    if (!CloseHandle(thread->nativeHandle))
        throw std::system_error(GetLastError(), std::system_category(), "Closing guest thread handle");
    delete thread;
}

struct NativeThreadArgs {
    std::unique_ptr<ThreadArgs> guest;
    std::future<bool> start;
    std::promise<void> initialized;
};

static unsigned __stdcall StartNativeThread(void* opaque) {
    std::unique_ptr<NativeThreadArgs> args(static_cast<NativeThreadArgs*>(opaque));
    auto* self = args->guest->self;
    try {
        ULONG_PTR low = 0;
        ULONG_PTR high = 0;
        GetCurrentThreadStackLimits(&low, &high);
        if (high <= low || high - low < self->stackSize)
            throw std::runtime_error("Cannot query guest thread stack");
        self->stackAddress = reinterpret_cast<void*>(high - self->stackSize);
        for (auto cursor = high - self->stackSize; cursor < high;) {
            MEMORY_BASIC_INFORMATION memory{};
            if (VirtualQuery(reinterpret_cast<void*>(cursor), &memory, sizeof(memory)) != sizeof(memory) || memory.State != MEM_COMMIT || memory.Protect != PAGE_READWRITE || memory.RegionSize == 0)
                throw std::runtime_error("Guest thread stack is not fully committed");
            cursor = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
        }
        self->threadId = GetCurrentThreadId();
        currentThread = self;
        args->initialized.set_value();
    } catch (...) {
        args->initialized.set_exception(std::current_exception());
        return 0;
    }
    if (!args->start.get())
        return 0;
    auto guest = std::move(args->guest);
    args.reset();
    RunThread(std::move(guest));
    currentThread = nullptr;
    ReleaseThread(self);
    return 0;
}
#endif

void ApplyHostThreadPriority(PthreadPrivate* thread) {
#ifdef _WIN32
    if (thread->nativeHandle != nullptr && !SetThreadPriority(thread->nativeHandle, HostThreadPriority(thread->schedPriority)))
        throw std::system_error(GetLastError(), std::system_category(), "Setting guest thread priority");
#else
    static_cast<void>(thread);
#endif
}

extern "C" {

int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    if (!thread || !entry) throw std::runtime_error("scePthreadCreate: null arg");
    if (attr && !*attr) throw std::runtime_error("scePthreadCreate: null attributes");
    auto p = std::make_unique<PthreadPrivate>();
    if (name) p->name = name;
    bool detached = false;
    if (attr && *attr) detached = ((*attr)->_detachstate == DETACH_DETACHED);
    p->_detached = detached;
    p->stackSize = attr ? (*attr)->_stacksize : DEFAULT_STACK_SIZE;
    if (attr && *attr) p->schedPriority = (*attr)->_schedpriority;
    APS5_LOG_OUT("thread '%s' created with priority %d", name != nullptr ? name : "", attr && *attr ? (*attr)->_schedpriority : -1);
    std::promise<bool> start;
    auto args = std::make_unique<ThreadArgs>(ThreadArgs{entry, arg, p.get()});
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    if (p->stackSize < 16384 || p->stackSize % system.dwPageSize != 0 || p->stackSize > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("scePthreadCreate: invalid Windows stack size");
    auto native = std::make_unique<NativeThreadArgs>(NativeThreadArgs{std::move(args), start.get_future(), {}});
    auto initialized = native->initialized.get_future();
    const auto handle = _beginthreadex(nullptr, static_cast<unsigned>(p->stackSize), StartNativeThread, native.get(), 0, nullptr);
    if (handle == 0)
        throw std::system_error(errno, std::generic_category(), "Creating guest thread");
    p->nativeHandle = reinterpret_cast<void*>(handle);
    native.release();
    ApplyHostThreadPriority(p.get());
    try {
        initialized.get();
    } catch (...) {
        start.set_value(false);
        WaitForSingleObject(p->nativeHandle, INFINITE);
        CloseHandle(p->nativeHandle);
        throw;
    }
    auto* published = p.release();
    *thread = published;
    start.set_value(true);
    if (detached)
        ReleaseThread(published);
#else
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    std::size_t stackSize = (p->stackSize + page - 1) / page * page;
    if (stackSize < static_cast<std::size_t>(PTHREAD_STACK_MIN)) stackSize = static_cast<std::size_t>(PTHREAD_STACK_MIN);
    pthread_attr_t nativeAttr;
    if (pthread_attr_init(&nativeAttr) != 0) throw std::runtime_error("scePthreadCreate: pthread_attr_init failed");
    if (pthread_attr_setstacksize(&nativeAttr, stackSize) != 0) {
        pthread_attr_destroy(&nativeAttr);
        throw std::runtime_error("scePthreadCreate: invalid stack size " + std::to_string(stackSize));
    }
#if defined(__APPLE__)
    if (p->schedPriority <= 300) pthread_attr_set_qos_class_np(&nativeAttr, QOS_CLASS_USER_INTERACTIVE, 0);
#endif
    auto native = std::make_unique<PosixThreadArgs>(PosixThreadArgs{std::move(args), start.get_future()});
    const int created = pthread_create(&p->native, &nativeAttr, StartPosixThread, native.get());
    pthread_attr_destroy(&nativeAttr);
    if (created != 0) throw std::system_error(created, std::generic_category(), "Creating guest thread");
    native.release();
    p->joinable = true;
    if (detached) {
        pthread_detach(p->native);
        p->joinable = false;
    }
    *thread = p.release();
    start.set_value(true);
#endif
    return SCE_OK;
}

int APS5_VABI scePthreadJoin(Pthread thread, void** retval) {
    if (!thread) throw std::runtime_error("scePthreadJoin: null thread");
    if (thread->_detached) return SCE_KERNEL_ERROR_EINVAL;
#ifdef _WIN32
    if (thread == currentThread)
        throw std::runtime_error("scePthreadJoin: cannot join current thread");
    if (WaitForSingleObject(thread->nativeHandle, INFINITE) != WAIT_OBJECT_0)
        throw std::system_error(GetLastError(), std::system_category(), "Joining guest thread");
    if (retval) *retval = thread->_retval;
    ReleaseThread(thread);
#else
    if (thread == currentThread) throw std::runtime_error("scePthreadJoin: cannot join current thread");
    if (thread->joinable) {
        const int error = pthread_join(thread->native, nullptr);
        if (error != 0) throw std::system_error(error, std::generic_category(), "Joining guest thread");
        thread->joinable = false;
    }
    if (retval) *retval = thread->_retval;
    delete thread;
#endif
    return SCE_OK;
}

int APS5_VABI scePthreadDetach(Pthread thread) {
    if (!thread) throw std::runtime_error("scePthreadDetach: null thread");
    if (thread->_detached) return SCE_KERNEL_ERROR_EINVAL;
    thread->_detached = true;
#ifdef _WIN32
    ReleaseThread(thread);
#else
    if (thread->joinable) {
        pthread_detach(thread->native);
        thread->joinable = false;
    }
#endif
    return SCE_OK;
}

void APS5_VABI scePthreadExit(void* retval) {
#ifdef _WIN32
    if (!currentThread)
        throw std::runtime_error("scePthreadExit: current thread is not registered");
    auto* self = currentThread;
    FinishThread(self, retval);
    currentThread = nullptr;
    ReleaseThread(self);
    _endthreadex(0);
#else
    if (auto* self = currentThread) {
        FinishThread(self, retval);
        currentThread = nullptr;
    }
    pthread_exit(retval);
#endif
    __builtin_unreachable();
}

int APS5_VABI scePthreadGetschedparam(Pthread thread, int* policy, KernelSchedParam* param) {
    if (!thread || !policy || !param) return SCE_KERNEL_ERROR_EINVAL;
    *policy = thread->schedPolicy;
    param->sched_priority = thread->schedPriority;
    return SCE_OK;
}

int APS5_VABI scePthreadSetschedparam(Pthread thread, int policy, const KernelSchedParam* param) {
    if (!thread || !param) return SCE_KERNEL_ERROR_EINVAL;
    if (policy < 1 || policy > 3) return SCE_KERNEL_ERROR_EINVAL;
    if (param->sched_priority < 256 || param->sched_priority > 767) return SCE_KERNEL_ERROR_EINVAL;
    thread->schedPolicy = policy;
    thread->schedPriority = param->sched_priority;
    ApplyHostThreadPriority(thread);
    return SCE_OK;
}

extern "C" Pthread KernelCurrentThreadRecord() {
    return currentThread;
}

Pthread APS5_VABI scePthreadSelf() {
    if (currentThread == nullptr) {
        auto* adopted = new PthreadPrivate();
        adopted->_detached = true;
#ifdef _WIN32
        ULONG_PTR low = 0;
        ULONG_PTR high = 0;
        GetCurrentThreadStackLimits(&low, &high);
        adopted->stackAddress = reinterpret_cast<void*>(low);
        adopted->stackSize = static_cast<std::size_t>(high - low);
        adopted->threadId = GetCurrentThreadId();
#else
        adopted->native = pthread_self();
        RecordHostStack(adopted);
#endif
        currentThread = adopted;
    }
    return currentThread;
}

void APS5_VABI scePthreadYield() {
    std::this_thread::yield();
}

int APS5_VABI scePthreadCancel(Pthread thread) {
 (void)thread;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadEqual(Pthread thread1, Pthread thread2) {
    return thread1 == thread2 ? 1 : 0;
}

int APS5_VABI scePthreadGetaffinity(Pthread thread, KernelCpumask* mask) {
    if (!thread) return SCE_KERNEL_ERROR_ESRCH;
    if (!mask) return SCE_KERNEL_ERROR_EINVAL;
    *mask = thread->affinity;
    return SCE_OK;
}

int APS5_VABI scePthreadGetname(Pthread thread, char* name) {
    if (!thread) return SCE_KERNEL_ERROR_ESRCH;
    if (!name) return SCE_KERNEL_ERROR_EINVAL;
    std::snprintf(name, 32, "%s", thread->name.c_str());
    return SCE_OK;
}

int APS5_VABI scePthreadGetprio(Pthread thread, int* prio) {
    if (!thread) return SCE_KERNEL_ERROR_ESRCH;
    if (!prio) return SCE_KERNEL_ERROR_EINVAL;
    *prio = thread->schedPriority;
    return SCE_OK;
}

int APS5_VABI scePthreadGetthreadid(void) {
#ifdef _WIN32
    return static_cast<int>(GetCurrentThreadId());
#elif defined(__APPLE__)
    std::uint64_t id = 0;
    pthread_threadid_np(nullptr, &id);
    return static_cast<int>(id);
#else
    return static_cast<int>(syscall(SYS_gettid));
#endif
}

int APS5_VABI scePthreadRename(Pthread thread, const char* name) {
    if (!thread) return SCE_KERNEL_ERROR_ESRCH;
    if (!name) return SCE_KERNEL_ERROR_EINVAL;
    thread->name = name;
#ifdef __APPLE__
    if (thread == currentThread) pthread_setname_np(name);
#endif
    return SCE_OK;
}

int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask) {
    if (!thread) return SCE_KERNEL_ERROR_ESRCH;
    if (mask == 0) return SCE_KERNEL_ERROR_EINVAL;
    thread->affinity = mask;
    return SCE_OK;
}

int APS5_VABI scePthreadSetcancelstate(int state, int* old_state) {
    if (state != 0 && state != 1) return SCE_KERNEL_ERROR_EINVAL;
    auto* self = scePthreadSelf();
    if (!self) throw std::runtime_error("scePthreadSetcancelstate: no current guest thread");
    if (old_state) *old_state = self->cancelState;
    self->cancelState = state;
    return SCE_OK;
}

int APS5_VABI scePthreadSetcanceltype(int type, int* old_type) {
    if (type != 0 && type != 1) return SCE_KERNEL_ERROR_EINVAL;
    auto* self = scePthreadSelf();
    if (!self) throw std::runtime_error("scePthreadSetcanceltype: no current guest thread");
    if (old_type) *old_type = self->cancelType;
    self->cancelType = type;
    return SCE_OK;
}

int APS5_VABI scePthreadSetprio(Pthread thread, int prio) {
    if (!thread) return SCE_KERNEL_ERROR_ESRCH;
    thread->schedPriority = prio;
    ApplyHostThreadPriority(thread);
    return SCE_OK;
}

}

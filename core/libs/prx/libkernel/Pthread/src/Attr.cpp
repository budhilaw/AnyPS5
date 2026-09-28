#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;

static constexpr std::size_t DEFAULT_STACK_SIZE = 1u << 20;
static constexpr int DETACH_JOINABLE = 0;
static constexpr int DETACH_DETACHED = 1;
static constexpr int SCHED_FIFO_PS5 = 1;

#ifdef _WIN32
#include <windows.h>
#include <limits>
#endif

extern "C" {

int APS5_VABI scePthreadAttrInit(PthreadAttr* attr) {
    if (!attr) throw std::runtime_error("scePthreadAttrInit: null attr");
    auto* p = new (std::nothrow) PthreadAttrPrivate{};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    p->_stacksize = DEFAULT_STACK_SIZE;
    p->_detachstate = DETACH_JOINABLE;
    p->_schedpriority = 700;
    p->_schedpolicy = SCHED_FIFO_PS5;
    p->_inheritsched = 4;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadAttrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetdetachstate(PthreadAttr* attr, int detachstate) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadAttrSetdetachstate: null attr");
    if (detachstate != DETACH_JOINABLE && detachstate != DETACH_DETACHED)
        throw std::runtime_error("scePthreadAttrSetdetachstate: invalid state");
    (*attr)->_detachstate = detachstate;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param) {
    if (!attr || !*attr || !param)
        throw std::runtime_error("scePthreadAttrSetschedparam: null arg");
    (*attr)->_schedpriority = param->sched_priority;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stacksize) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadAttrSetstacksize: null attr");
    if (stacksize < 16384) throw std::runtime_error("scePthreadAttrSetstacksize: too small");
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    if (stacksize % system.dwPageSize != 0 || stacksize > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("scePthreadAttrSetstacksize: invalid Windows stack size");
#endif
    (*attr)->_stacksize = stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stackaddr, std::size_t* stacksize) {
    if (!attr || !*attr || !stackaddr || !stacksize)
        throw std::runtime_error("scePthreadAttrGetstack: null arg");
    *stackaddr = (*attr)->stackAddress;
    *stacksize = (*attr)->_stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr) {
    if (!thread || !attr || !*attr)
        throw std::runtime_error("scePthreadAttrGet: null arg");
    (*attr)->_stacksize = thread->stackSize;
    (*attr)->stackAddress = thread->stackAddress;
    (*attr)->_detachstate = thread->_detached ? DETACH_DETACHED : DETACH_JOINABLE;
    (*attr)->_schedpriority = 700;
    (*attr)->_schedpolicy = SCHED_FIFO_PS5;
    (*attr)->_inheritsched = 4;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask) {
    if (!attr || !*attr || !mask) return SCE_KERNEL_ERROR_EINVAL;
    *mask = (*attr)->_affinity;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state) {
    if (!attr || !*attr || !state) return SCE_KERNEL_ERROR_EINVAL;
    *state = (*attr)->_detachstate;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, size_t* guard_size) {
    if (!attr || !*attr || !guard_size) return SCE_KERNEL_ERROR_EINVAL;
    *guard_size = (*attr)->_guardsize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param) {
    if (!attr || !*attr || !param) return SCE_KERNEL_ERROR_EINVAL;
    param->sched_priority = (*attr)->_schedpriority;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetsolosched(const PthreadAttr* attr, int* solosched) {
    if (!attr || !*attr || !solosched) return SCE_KERNEL_ERROR_EINVAL;
    *solosched = (*attr)->_solosched;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetstackaddr(const PthreadAttr* attr, void** stack_addr) {
    if (!attr || !*attr || !stack_addr) return SCE_KERNEL_ERROR_EINVAL;
    *stack_addr = (*attr)->stackAddress;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, size_t* stack_size) {
    if (!attr || !*attr || !stack_size) return SCE_KERNEL_ERROR_EINVAL;
    *stack_size = (*attr)->_stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    if (mask == 0) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->_affinity = mask;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, size_t guard_size) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->_guardsize = guard_size;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inherit_sched) {
    if (!attr || !*attr || (inherit_sched != 0 && inherit_sched != 4)) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->_inheritsched = inherit_sched;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy) {
    if (!attr || !*attr || policy < 0 || policy > 3) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->_schedpolicy = policy;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetsolosched(PthreadAttr* attr, int solosched) {
    if (!attr || !*attr || (solosched != 0 && solosched != 1)) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->_solosched = solosched; // a core-exclusivity hint the host scheduler does not take
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* addr, size_t size) {
    if (!attr || !*attr || !addr || size < 16384) return SCE_KERNEL_ERROR_EINVAL;
    // Caller-provided stacks are recorded for queries; threads still run on host-allocated stacks
    // of the requested size, since guest memory cannot back a host thread's stack portably.
    (*attr)->stackAddress = addr;
    (*attr)->_stacksize = size;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetstackaddr(PthreadAttr* attr, void* addr) {
    if (!attr || !*attr || !addr) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->stackAddress = addr;
    return SCE_OK;
}

}

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;

extern "C" {

void APS5_VABI sceKernelRtldSetApplicationHeapAPI(void* api[]) {
    ApplicationHeapRegister_nid_no_patch(api);
}

int APS5_VABI sceKernelRtldThreadAtexitDecrement(uint64_t* c) {
    if (c == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    __atomic_sub_fetch(c, 1, __ATOMIC_SEQ_CST);
    return 0;
}

int APS5_VABI sceKernelRtldThreadAtexitIncrement(uint64_t* c) {
    if (c == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    __atomic_add_fetch(c, 1, __ATOMIC_SEQ_CST);
    return 0;
}

void APS5_VABI sceKernelSetThreadAtexitCount(get_thread_atexit_count_func_t func) {
    static get_thread_atexit_count_func_t registered = nullptr;
    registered = func;
}

void APS5_VABI sceKernelSetThreadAtexitReport(thread_atexit_report_func_t func) {
    static thread_atexit_report_func_t registered = nullptr;
    registered = func;
}

void APS5_VABI sceKernelSetThreadDtors(thread_dtors_func_t dtors) {
    static thread_dtors_func_t registered = nullptr;
    registered = dtors;
}

}

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
constexpr int SCE_KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010);
constexpr int SCE_KERNEL_ERROR_EACCES = static_cast<int>(0x8002000D);
constexpr int SCE_KERNEL_ERROR_EPERM = static_cast<int>(0x80020001);
constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);
constexpr int SCE_KERNEL_ERROR_ECANCELED = static_cast<int>(0x80020055);

constexpr std::uint32_t WAITMODE_AND = 0x01;
constexpr std::uint32_t WAITMODE_OR = 0x02;
constexpr std::uint32_t WAITMODE_CLEAR_ALL = 0x10;
constexpr std::uint32_t WAITMODE_CLEAR_PAT = 0x20;
constexpr std::uint32_t ATTR_SINGLE = 0x10;

bool Satisfied(std::uint64_t pattern, std::uint64_t bits, std::uint32_t mode) {
    if ((mode & WAITMODE_AND) != 0) return (pattern & bits) == bits;
    return (pattern & bits) != 0;
}

}

struct KernelEventFlagPrivate {
    std::mutex mutex;
    std::condition_variable condition;
    std::string name;
    std::uint64_t pattern = 0;
    bool single = false;
    int waiters = 0;
    bool deleted = false;
    std::uint64_t cancelGeneration = 0;
};

extern "C" {

int APS5_VABI sceKernelCreateEventFlag(KernelEventFlag* ef, const char* name, uint32_t attr, uint64_t init_pattern, const void* param) {
    (void)param;
    if (ef == nullptr || name == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    const auto threadMode = attr & 0x30;
    if (threadMode != 0 && threadMode != ATTR_SINGLE && threadMode != 0x20) return SCE_KERNEL_ERROR_EINVAL;
    auto* flag = new KernelEventFlagPrivate();
    flag->name = name;
    flag->pattern = init_pattern;
    flag->single = threadMode == ATTR_SINGLE;
    *ef = flag;
    return 0;
}

int APS5_VABI sceKernelDeleteEventFlag(KernelEventFlag ef) {
    if (ef == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    {
        std::unique_lock lock(ef->mutex);
        if (ef->deleted) return SCE_KERNEL_ERROR_EINVAL;
        ef->deleted = true;
        ef->condition.notify_all();
        ef->condition.wait(lock, [&] { return ef->waiters == 0; });
    }
    delete ef;
    return 0;
}

int APS5_VABI sceKernelSetEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
    if (ef == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(ef->mutex);
    ef->pattern |= bit_pattern;
    ef->condition.notify_all();
    return 0;
}

int APS5_VABI sceKernelClearEventFlag(KernelEventFlag ef, uint64_t bit_pattern) {
    if (ef == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(ef->mutex);
    ef->pattern &= bit_pattern;
    return 0;
}

int APS5_VABI sceKernelCancelEventFlag(KernelEventFlag ef, uint64_t set_pattern, int* num_wait_threads) {
    if (ef == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(ef->mutex);
    if (num_wait_threads != nullptr) *num_wait_threads = ef->waiters;
    ef->pattern = set_pattern;
    ++ef->cancelGeneration;
    ef->condition.notify_all();
    return 0;
}

int APS5_VABI sceKernelPollEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat) {
    if (ef == nullptr || bit_pattern == 0) return SCE_KERNEL_ERROR_EINVAL;
    const auto match = wait_mode & (WAITMODE_AND | WAITMODE_OR);
    if (match != WAITMODE_AND && match != WAITMODE_OR) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(ef->mutex);
    if (result_pat != nullptr) *result_pat = ef->pattern;
    if (!Satisfied(ef->pattern, bit_pattern, wait_mode)) return SCE_KERNEL_ERROR_EBUSY;
    if ((wait_mode & WAITMODE_CLEAR_ALL) != 0) ef->pattern = 0;
    else if ((wait_mode & WAITMODE_CLEAR_PAT) != 0) ef->pattern &= ~bit_pattern;
    return 0;
}

int APS5_VABI sceKernelWaitEventFlag(KernelEventFlag ef, uint64_t bit_pattern, uint32_t wait_mode, uint64_t* result_pat, KernelUseconds* timeout) {
    if (ef == nullptr || bit_pattern == 0) return SCE_KERNEL_ERROR_EINVAL;
    const auto match = wait_mode & (WAITMODE_AND | WAITMODE_OR);
    if (match != WAITMODE_AND && match != WAITMODE_OR) return SCE_KERNEL_ERROR_EINVAL;
    std::unique_lock lock(ef->mutex);
    if (ef->deleted) return SCE_KERNEL_ERROR_EACCES;
    if (ef->single && ef->waiters > 0) return SCE_KERNEL_ERROR_EPERM;
    const auto generation = ef->cancelGeneration;
    const auto start = std::chrono::steady_clock::now();
    const auto wake = [&] { return ef->deleted || ef->cancelGeneration != generation || Satisfied(ef->pattern, bit_pattern, wait_mode); };
    ++ef->waiters;
    bool woken = true;
    if (timeout == nullptr) ef->condition.wait(lock, wake);
    else woken = ef->condition.wait_for(lock, std::chrono::microseconds(*timeout), wake);
    --ef->waiters;
    if (timeout != nullptr) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        *timeout = elapsed >= static_cast<long long>(*timeout) ? 0 : *timeout - static_cast<KernelUseconds>(elapsed);
    }
    if (result_pat != nullptr) *result_pat = ef->pattern;
    if (ef->deleted) {
        ef->condition.notify_all();
        return SCE_KERNEL_ERROR_EACCES;
    }
    if (ef->cancelGeneration != generation) return SCE_KERNEL_ERROR_ECANCELED;
    if (!woken) return SCE_KERNEL_ERROR_ETIMEDOUT;
    if ((wait_mode & WAITMODE_CLEAR_ALL) != 0) ef->pattern = 0;
    else if ((wait_mode & WAITMODE_CLEAR_PAT) != 0) ef->pattern &= ~bit_pattern;
    return 0;
}

}

#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"

#ifndef _WIN32
#include <pthread.h>
#include <sys/ucontext.h>
#endif

extern "C" Pthread KernelCurrentThreadRecord();

namespace {

constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
constexpr int SCE_KERNEL_ERROR_ESRCH = static_cast<int>(0x80020003);
constexpr int SCE_KERNEL_ERROR_EAGAIN = static_cast<int>(0x80020023);
constexpr int SignalCount = 32;

struct GuestMcontext {
    std::uint64_t onstack, rdi, rsi, rdx, rcx, r8, r9, rax, rbx, rbp, r10, r11, r12, r13, r14, r15;
    std::int32_t trapno;
    std::uint16_t fs, gs;
    std::uint64_t addr;
    std::int32_t flags;
    std::uint16_t es, ds;
    std::uint64_t err, rip, cs, rflags;
    std::uint64_t reserved[8];
    std::uint64_t rsp, ss, len, fpformat, ownedfp, lbrfrom, lbrto, aux1, aux2;
    std::uint64_t fpstate[104];
    std::uint64_t fsbase, gsbase;
    std::uint64_t spare[6];
};
static_assert(offsetof(GuestMcontext, rip) == 0xa0 && offsetof(GuestMcontext, rsp) == 0xf8);

struct GuestUcontext {
    GuestMcontext mcontext;
    std::uint64_t link;
    std::uint64_t stack[3];
    std::int32_t flags;
    std::int32_t spare[4];
};

using GuestHandler = void (APS5_VABI *)(int, void*);

std::mutex handlerMutex;
std::array<std::atomic<GuestHandler>, SignalCount> handlers{};

#if !defined(_WIN32) && defined(__x86_64__)
std::once_flag hostHandlerInstalled;
constexpr int HostDeliverySignal = SIGUSR1;

GuestUcontext fromHost(const ucontext_t* host) {
    GuestUcontext context{};
    auto& m = context.mcontext;
    m.len = sizeof(GuestMcontext);
#if defined(__APPLE__)
    const auto& ss = host->uc_mcontext->__ss;
    m.rdi = ss.__rdi; m.rsi = ss.__rsi; m.rdx = ss.__rdx; m.rcx = ss.__rcx; m.r8 = ss.__r8; m.r9 = ss.__r9;
    m.rax = ss.__rax; m.rbx = ss.__rbx; m.rbp = ss.__rbp; m.r10 = ss.__r10; m.r11 = ss.__r11;
    m.r12 = ss.__r12; m.r13 = ss.__r13; m.r14 = ss.__r14; m.r15 = ss.__r15;
    m.rip = ss.__rip; m.rsp = ss.__rsp; m.rflags = ss.__rflags; m.cs = ss.__cs & 0xffffu;
    m.fs = static_cast<std::uint16_t>(ss.__fs); m.gs = static_cast<std::uint16_t>(ss.__gs);
#else
    const auto* g = host->uc_mcontext.gregs;
    m.rdi = g[REG_RDI]; m.rsi = g[REG_RSI]; m.rdx = g[REG_RDX]; m.rcx = g[REG_RCX]; m.r8 = g[REG_R8]; m.r9 = g[REG_R9];
    m.rax = g[REG_RAX]; m.rbx = g[REG_RBX]; m.rbp = g[REG_RBP]; m.r10 = g[REG_R10]; m.r11 = g[REG_R11];
    m.r12 = g[REG_R12]; m.r13 = g[REG_R13]; m.r14 = g[REG_R14]; m.r15 = g[REG_R15];
    m.rip = g[REG_RIP]; m.rsp = g[REG_RSP]; m.rflags = g[REG_EFL];
#endif
    return context;
}

void toHost(ucontext_t* host, const GuestUcontext& context) {
    const auto& m = context.mcontext;
#if defined(__APPLE__)
    auto& ss = host->uc_mcontext->__ss;
    ss.__rdi = m.rdi; ss.__rsi = m.rsi; ss.__rdx = m.rdx; ss.__rcx = m.rcx; ss.__r8 = m.r8; ss.__r9 = m.r9;
    ss.__rax = m.rax; ss.__rbx = m.rbx; ss.__rbp = m.rbp; ss.__r10 = m.r10; ss.__r11 = m.r11;
    ss.__r12 = m.r12; ss.__r13 = m.r13; ss.__r14 = m.r14; ss.__r15 = m.r15;
    ss.__rip = m.rip; ss.__rsp = m.rsp; ss.__rflags = m.rflags;
#else
    auto* g = host->uc_mcontext.gregs;
    g[REG_RDI] = m.rdi; g[REG_RSI] = m.rsi; g[REG_RDX] = m.rdx; g[REG_RCX] = m.rcx; g[REG_R8] = m.r8; g[REG_R9] = m.r9;
    g[REG_RAX] = m.rax; g[REG_RBX] = m.rbx; g[REG_RBP] = m.rbp; g[REG_R10] = m.r10; g[REG_R11] = m.r11;
    g[REG_R12] = m.r12; g[REG_R13] = m.r13; g[REG_R14] = m.r14; g[REG_R15] = m.r15;
    g[REG_RIP] = m.rip; g[REG_RSP] = m.rsp; g[REG_EFL] = m.rflags;
#endif
}

void hostHandler(int, siginfo_t*, void* opaque) {
    const int savedErrno = errno;
    auto* self = KernelCurrentThreadRecord();
    if (self != nullptr) {
        auto* host = static_cast<ucontext_t*>(opaque);
        for (;;) {
            const auto pending = self->pendingSignals.load(std::memory_order_acquire);
            if (pending == 0) break;
            const int signum = __builtin_ctz(pending);
            self->pendingSignals.fetch_and(~(1u << signum), std::memory_order_acq_rel);
            const auto handler = handlers[static_cast<std::size_t>(signum)].load(std::memory_order_acquire);
            if (handler == nullptr) continue;
            GuestUcontext context = fromHost(host);
            const GuestMcontext before = context.mcontext;
            handler(signum, &context);
            if (std::memcmp(&before, &context.mcontext, offsetof(GuestMcontext, reserved)) != 0 || before.rsp != context.mcontext.rsp) toHost(host, context);
        }
    }
    errno = savedErrno;
}

void installHostHandler() {
    struct sigaction action{};
    action.sa_sigaction = hostHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(HostDeliverySignal, &action, nullptr) != 0) throw std::runtime_error("sceKernelRaiseException: cannot install the host signal handler");
}

bool deliverFault(int signal, siginfo_t* info, void* opaque) {
    const int signum = signal == SIGBUS ? 10 : signal == SIGSEGV ? 11 : signal == SIGFPE ? 8 : signal == SIGILL ? 4 : 0;
    if (signum == 0) return false;
    const auto handler = handlers[static_cast<std::size_t>(signum)].load(std::memory_order_acquire);
    if (handler == nullptr) return false;
    auto* host = static_cast<ucontext_t*>(opaque);
    GuestUcontext context = fromHost(host);
    context.mcontext.addr = reinterpret_cast<std::uint64_t>(info->si_addr);
    context.mcontext.trapno = signal == SIGSEGV || signal == SIGBUS ? 12 : 0;
    const GuestMcontext before = context.mcontext;
    handler(signum, &context);
    if (std::memcmp(&before, &context.mcontext, offsetof(GuestMcontext, reserved)) == 0 && before.rsp == context.mcontext.rsp) return false;
    toHost(host, context);
    return true;
}

void deliverToSelf(int signum, GuestHandler handler) {
    GuestUcontext context{};
    auto& m = context.mcontext;
    m.len = sizeof(GuestMcontext);
    std::uint64_t rsp, rbp, rbx, r12, r13, r14, r15;
    __asm__ volatile("movq %%rsp, %0\n\tmovq %%rbp, %1\n\tmovq %%rbx, %2\n\tmovq %%r12, %3\n\tmovq %%r13, %4\n\tmovq %%r14, %5\n\tmovq %%r15, %6"
                     : "=r"(rsp), "=r"(rbp), "=r"(rbx), "=r"(r12), "=r"(r13), "=r"(r14), "=r"(r15));
    m.rsp = rsp; m.rbp = rbp; m.rbx = rbx; m.r12 = r12; m.r13 = r13; m.r14 = r14; m.r15 = r15;
    m.rip = reinterpret_cast<std::uint64_t>(__builtin_return_address(0));
    handler(signum, &context);
}
#endif

}

extern "C" {

int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler) {
    if (signum <= 0 || signum >= SignalCount || handler == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(handlerMutex);
    if (handlers[static_cast<std::size_t>(signum)].load() != nullptr) return SCE_KERNEL_ERROR_EAGAIN;
    handlers[static_cast<std::size_t>(signum)].store(reinterpret_cast<GuestHandler>(handler));
    APS5_LOG_OUT("guest exception handler %p installed for signal %d", handler, signum);
#if !defined(_WIN32) && defined(__x86_64__)
    GuestMemoryTracking::GuestMemoryTrackingSetFaultHandler_nid_postfix(deliverFault);
#endif
    return 0;
}

int APS5_VABI sceKernelRemoveExceptionHandler(int signum) {
    if (signum <= 0 || signum >= SignalCount) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(handlerMutex);
    handlers[static_cast<std::size_t>(signum)].store(nullptr);
    return 0;
}

int APS5_VABI sceKernelRaiseException(Pthread thread, int signum) {
    if (thread == nullptr) return SCE_KERNEL_ERROR_ESRCH;
    if (signum <= 0 || signum >= SignalCount) return SCE_KERNEL_ERROR_EINVAL;
    const auto handler = handlers[static_cast<std::size_t>(signum)].load(std::memory_order_acquire);
    if (handler == nullptr) return SCE_KERNEL_ERROR_EINVAL;
#if defined(_WIN32) || !defined(__x86_64__)
    throw std::runtime_error("sceKernelRaiseException: guest signal delivery is only implemented for x86-64 POSIX hosts");
#else
    if (thread == KernelCurrentThreadRecord()) {
        deliverToSelf(signum, handler);
        return 0;
    }
    std::call_once(hostHandlerInstalled, installHostHandler);
    thread->pendingSignals.fetch_or(1u << signum, std::memory_order_acq_rel);
    const int error = pthread_kill(thread->native, HostDeliverySignal);
    if (error != 0) {
        thread->pendingSignals.fetch_and(~(1u << signum), std::memory_order_acq_rel);
        return SCE_KERNEL_ERROR_ESRCH;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
    while ((thread->pendingSignals.load(std::memory_order_acquire) & (1u << signum)) != 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::microseconds(50));
    return 0;
#endif
}

void APS5_VABI sceKernelDebugRaiseException(int c1, int c2) {
    std::fprintf(stderr, "sceKernelDebugRaiseException: title requested an abort (0x%x, 0x%x)\n", c1, c2);
    std::fflush(stderr);
    std::abort();
}

void APS5_VABI sceKernelDebugRaiseExceptionOnReleaseMode(int c1, int c2) {
    std::fprintf(stderr, "sceKernelDebugRaiseExceptionOnReleaseMode: title requested an abort (0x%x, 0x%x)\n", c1, c2);
    std::fflush(stderr);
    std::abort();
}

}

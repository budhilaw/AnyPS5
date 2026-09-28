#include <cstdlib>
#include <cstdio>
#include <execinfo.h>
#include "prx/libc/include/MemoryTrackingPlatform.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <csignal>
#include <atomic>
#include <exception>
#include <stdexcept>
#include <system_error>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <unistd.h>

namespace GuestMemoryTracking::Platform {
namespace {

FaultHandler faultHandler = nullptr;
// XNU reports a protection violation on a mapped page as SIGBUS and an unmapped page as SIGSEGV,
// so both signals are hooked and both previous dispositions are preserved for chaining.
struct sigaction previousSegv{};
struct sigaction previousBus{};

struct FaultKind {
    bool data;
    bool write;
};

FaultKind classify(const ucontext_t* context) {
#if defined(__arm64__) || defined(__aarch64__)
    const auto esr = context->uc_mcontext->__es.__esr;
    const auto exceptionClass = esr >> 26u;
    const bool data = exceptionClass == 0x24 || exceptionClass == 0x25;
    return {data, data && (esr & (1u << 6u)) != 0};
#elif defined(__x86_64__)
    const auto error = context->uc_mcontext->__es.__err;
    return {(error & 16) == 0, (error & 2) != 0};
#else
#error "unsupported Darwin architecture"
#endif
}

// Async-signal-safe report of a fault nobody handles, so an unhandled guest crash leaves a trace
// even when no crash report is written for a signal re-raised from inside a handler.
void writeHex(std::uint64_t value) {
    char buffer[19] = "0x";
    static const char digits[] = "0123456789abcdef";
    for (int i = 0; i < 16; ++i) buffer[2 + i] = digits[(value >> (60 - 4 * i)) & 0xf];
    (void)!write(STDERR_FILENO, buffer, 18);
}

void report(int signal, const siginfo_t* info, const ucontext_t* context, FaultKind kind) {
    const char* name = signal == SIGBUS ? "unhandled SIGBUS" : "unhandled SIGSEGV";
    (void)!write(STDERR_FILENO, name, signal == SIGBUS ? 16 : 17);
    (void)!write(STDERR_FILENO, kind.write ? " write at " : " read at ", kind.write ? 10 : 9);
    writeHex(reinterpret_cast<std::uint64_t>(info->si_addr));
    (void)!write(STDERR_FILENO, " pc=", 4);
#if defined(__x86_64__)
    writeHex(context->uc_mcontext->__ss.__rip);
    (void)!write(STDERR_FILENO, " sp=", 4);
    writeHex(context->uc_mcontext->__ss.__rsp);
#else
    writeHex(context->uc_mcontext->__ss.__pc);
#endif
    {
        // si_code and the page's current protection tell a protection fault from a paging failure.
        (void)!write(STDERR_FILENO, " code=", 6);
        writeHex(static_cast<std::uint64_t>(info->si_code));
        mach_vm_address_t region = reinterpret_cast<mach_vm_address_t>(info->si_addr);
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t basic{};
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        if (mach_vm_region(mach_task_self(), &region, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&basic), &count, &object) == KERN_SUCCESS) {
            (void)!write(STDERR_FILENO, " region=", 8);
            writeHex(region);
            (void)!write(STDERR_FILENO, " prot=", 6);
            writeHex(static_cast<std::uint64_t>(basic.protection));
            (void)!write(STDERR_FILENO, " shared=", 8);
            writeHex(static_cast<std::uint64_t>(basic.shared));
        }
    }
    (void)!write(STDERR_FILENO, "\n", 1);
#if defined(__x86_64__)
    // The process is about to die: naming the image is worth the non-signal-safe lookup.
    Dl_info image{};
    const auto pc = context->uc_mcontext->__ss.__rip;
    if (dladdr(reinterpret_cast<const void*>(pc), &image) != 0 && image.dli_fname != nullptr) {
        (void)!write(STDERR_FILENO, "  in ", 5);
        (void)!write(STDERR_FILENO, image.dli_fname, std::strlen(image.dli_fname));
        (void)!write(STDERR_FILENO, " + ", 3);
        writeHex(pc - reinterpret_cast<std::uint64_t>(image.dli_fbase));
        (void)!write(STDERR_FILENO, "\n", 1);
    }
    // The host frames above the fault (frame-pointer walk through the signal trampoline).
    {
        void* frames[32];
        const auto count = ::backtrace(frames, 32);
        ::backtrace_symbols_fd(frames, count, STDERR_FILENO);
    }
    GuestMemoryTracking::GuestMemoryTrackingDescribe_nid_postfix(reinterpret_cast<std::uint64_t>(info->si_addr) & ~0xffffull, 0x20000);
    // General registers: a call through a null pointer shows which one carried it.
    {
        const auto& ss = context->uc_mcontext->__ss;
        const std::uint64_t values[] = {ss.__rax, ss.__rbx, ss.__rcx, ss.__rdx, ss.__rsi, ss.__rdi, ss.__rbp, ss.__r8, ss.__r9, ss.__r10, ss.__r11, ss.__r12, ss.__r13, ss.__r14, ss.__r15};
        const char* names[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "r8 ", "r9 ", "r10", "r11", "r12", "r13", "r14", "r15"};
        for (int index = 0; index < 15; ++index) {
            (void)!write(STDERR_FILENO, index % 4 == 0 ? "\n  " : "  ", index % 4 == 0 ? 3 : 2);
            (void)!write(STDERR_FILENO, names[index], 3);
            (void)!write(STDERR_FILENO, "=", 1);
            writeHex(values[index]);
        }
        (void)!write(STDERR_FILENO, "\n", 1);
    }
    // The top stack words: after a call through a bad pointer the first one is the return address.
    const auto* stack = reinterpret_cast<const std::uint64_t*>(context->uc_mcontext->__ss.__rsp);
    for (int index = 0; index < 12; ++index) {
        Dl_info owner{};
        const auto value = stack[index];
        (void)!write(STDERR_FILENO, "  [sp+", 6);
        const char offset[] = {static_cast<char>('0' + index * 8 / 10), static_cast<char>('0' + index * 8 % 10), ']', ' '};
        if (index * 8 >= 100) break;
        (void)!write(STDERR_FILENO, offset, 4);
        writeHex(value);
        if (dladdr(reinterpret_cast<const void*>(value), &owner) != 0 && owner.dli_fname != nullptr) {
            (void)!write(STDERR_FILENO, " = ", 3);
            (void)!write(STDERR_FILENO, owner.dli_fname, std::strlen(owner.dli_fname));
            (void)!write(STDERR_FILENO, " + ", 3);
            writeHex(value - reinterpret_cast<std::uint64_t>(owner.dli_fbase));
        }
        (void)!write(STDERR_FILENO, "\n", 1);
    }
#endif
}

// A guest exception handler (sceKernelInstallExceptionHandler) receives the faults that are not
// memory-tracking events, as the console delivers hardware faults to titles: il2cpp turns a null
// dereference into a managed NullReferenceException this way.
std::atomic<bool (*)(int, siginfo_t*, void*)> guestFaultHandler{nullptr};

void chain(const struct sigaction& previous, int signal, siginfo_t* info, void* context) {
    if (const auto deliver = guestFaultHandler.load(std::memory_order_acquire); deliver != nullptr && deliver(signal, info, context)) return;
    if (previous.sa_handler == SIG_DFL || previous.sa_handler == SIG_IGN) {
        report(signal, info, static_cast<const ucontext_t*>(context), classify(static_cast<const ucontext_t*>(context)));
        // ANYPS5_HANG_ON_CRASH=1 keeps the crashed process alive so a debugger can be attached
        // to the faulting thread with its registers and the other threads intact.
        static const bool hang = std::getenv("ANYPS5_HANG_ON_CRASH") != nullptr;
        if (hang) {
            (void)!write(STDERR_FILENO, "crash: hanging for a debugger (ANYPS5_HANG_ON_CRASH)\n", 53);
            for (;;) pause();
        }
        if (sigaction(signal, &previous, nullptr) != 0) std::terminate();
        if (raise(signal) != 0) std::terminate();
        return;
    }
    if ((previous.sa_flags & SA_SIGINFO) != 0) previous.sa_sigaction(signal, info, context);
    else previous.sa_handler(signal);
}

// True when the page now permits the access: another thread changed its protection (a watch
// destroyed or resolved) between the fault and this handler taking the registry lock, so the
// instruction can simply run again.
bool accessibleNow(std::uintptr_t address, bool write) {
    mach_vm_address_t region = address;
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t basic{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    if (mach_vm_region(mach_task_self(), &region, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&basic), &count, &object) != KERN_SUCCESS) return false;
    if (region > address || address - region >= size) return false;
    const vm_prot_t needed = write ? (VM_PROT_READ | VM_PROT_WRITE) : VM_PROT_READ;
    return (basic.protection & needed) == needed;
}

thread_local std::uintptr_t retriedAddress = 0;
thread_local unsigned retries = 0;
std::atomic<unsigned> staleFaults{0};

void handleFault(int signal, siginfo_t* info, void* context) {
    const auto kind = classify(static_cast<const ucontext_t*>(context));
    if (kind.data) {
        const auto address = reinterpret_cast<std::uintptr_t>(info->si_addr);
        try {
            if (faultHandler(address, kind.write)) { retries = 0; return; }
        } catch (...) {
            std::terminate();
        }
        // A bounded number of retries: a fault that keeps recurring on an accessible page is real.
        if (address != 0 && accessibleNow(address, kind.write)) {
            if (retriedAddress != address) { retriedAddress = address; retries = 0; }
            if (++retries <= 16) {
                const auto total = staleFaults.fetch_add(1, std::memory_order_relaxed) + 1;
                if ((total & (total - 1)) == 0) {
                    (void)!write(STDERR_FILENO, "guest memory: retried a fault on an accessible page at ", 55);
                    writeHex(address);
                    (void)!write(STDERR_FILENO, "\n", 1);
                }
                return;
            }
        }
    }
    chain(signal == SIGBUS ? previousBus : previousSegv, signal, info, context);
}

void protect(std::uint64_t address, std::size_t bytes, int protection) {
    if (mprotect(reinterpret_cast<void*>(address), bytes, protection) != 0) throw std::system_error(errno, std::generic_category(), "guest memory tracking mprotect failed");
}

bool TraceProtect() {
    static const bool enabled = std::getenv("ANYPS5_TRACE_PROTECT") != nullptr;
    return enabled;
}

int hostProtection(vm_prot_t protection) {
    return ((protection & VM_PROT_READ) != 0 ? PROT_READ : 0) | ((protection & VM_PROT_WRITE) != 0 ? PROT_WRITE : 0) | ((protection & VM_PROT_EXECUTE) != 0 ? PROT_EXEC : 0);
}

}

std::size_t PageSize() {
    static const auto size = [] {
        const auto result = sysconf(_SC_PAGESIZE);
        if (result <= 0) throw std::runtime_error("invalid native page size");
        return static_cast<std::size_t>(result);
    }();
    return size;
}

void Install(FaultHandler handler) {
    if (handler == nullptr || faultHandler != nullptr) throw std::runtime_error("invalid guest memory fault handler installation");
    faultHandler = handler;
    struct sigaction action{};
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = handleFault;
    if (sigemptyset(&action.sa_mask) != 0 || sigaction(SIGSEGV, &action, &previousSegv) != 0) {
        const auto error = errno;
        faultHandler = nullptr;
        throw std::system_error(error, std::generic_category(), "guest memory fault handler installation failed");
    }
    if (sigaction(SIGBUS, &action, &previousBus) != 0) {
        const auto error = errno;
        if (sigaction(SIGSEGV, &previousSegv, nullptr) != 0) std::terminate();
        faultHandler = nullptr;
        throw std::system_error(error, std::generic_category(), "guest memory fault handler installation failed");
    }
}

std::vector<Region> Query(std::uint64_t address, std::size_t bytes) {
    std::vector<Region> regions;
    const auto end = address + bytes;
    while (address < end) {
        mach_vm_address_t first = address;
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        const auto result = mach_vm_region(mach_task_self(), &first, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object);
        if (object != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object);
        if (result != KERN_SUCCESS || first > address) throw std::runtime_error("tracked guest memory range is not mapped");
        if (info.protection != (VM_PROT_READ | VM_PROT_WRITE)) throw std::runtime_error("tracked render memory must be mapped, writable and non-executable");
        const auto last = static_cast<std::uint64_t>(first) + size;
        if (last <= address) throw std::runtime_error("invalid tracked guest memory mapping");
        const auto next = std::min(end, last);
        regions.push_back({address, static_cast<std::size_t>(next - address), static_cast<std::uint64_t>(hostProtection(info.protection))});
        address = next;
    }
    return regions;
}

void Protect(std::uint64_t address, std::size_t bytes, Protection protection) {
    if (protection != Protection::None && protection != Protection::Read) throw std::invalid_argument("invalid tracked page protection");
    if (TraceProtect()) std::fprintf(stderr, "[tracking] protect 0x%llx+0x%zx %s\n", static_cast<unsigned long long>(address), bytes, protection == Protection::None ? "none" : "read");
    protect(address, bytes, protection == Protection::None ? PROT_NONE : PROT_READ);
}

void Restore(const std::vector<Region>& regions) {
    for (const auto& region : regions) {
        if (TraceProtect()) std::fprintf(stderr, "[tracking] restore 0x%llx+0x%zx prot %d\n", static_cast<unsigned long long>(region.address), region.bytes, static_cast<int>(region.protection));
        protect(region.address, region.bytes, static_cast<int>(region.protection));
    }
}

}

extern "C" void GuestMemoryTrackingSetFaultHandler_nid_postfix(bool (*handler)(int, siginfo_t*, void*)) {
    GuestMemoryTracking::Platform::guestFaultHandler.store(handler, std::memory_order_release);
}

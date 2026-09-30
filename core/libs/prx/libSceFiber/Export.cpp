#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_FIBER_ERROR_NULL = static_cast<std::int32_t>(0x80590001);
constexpr std::int32_t SCE_FIBER_ERROR_ALIGNMENT = static_cast<std::int32_t>(0x80590002);
constexpr std::int32_t SCE_FIBER_ERROR_RANGE = static_cast<std::int32_t>(0x80590003);
constexpr std::int32_t SCE_FIBER_ERROR_INVALID = static_cast<std::int32_t>(0x80590004);
constexpr std::int32_t SCE_FIBER_ERROR_PERMISSION = static_cast<std::int32_t>(0x80590005);
constexpr std::int32_t SCE_FIBER_ERROR_STATE = static_cast<std::int32_t>(0x80590006);

constexpr std::uint32_t SignatureStart = 0xdef1649c;
constexpr std::uint32_t SignatureEnd = 0xb37592a0;
constexpr std::uint32_t OptSignature = 0xbb40e64d;
constexpr std::uint64_t StackSignature = 0x7149f2ca7149f2caull;
constexpr std::uint64_t StackFill = 0xdeadbeefdeadbeefull;
constexpr std::uint64_t ContextMinimumSize = 512;
constexpr std::uint64_t ThreadStackReserve = 1024;
constexpr std::uint32_t FpuBuildVersion = 0x3500000;

enum State : std::uint32_t { Idle = 1, Running = 2, Finalized = 3 };
enum Flags : std::uint32_t { SetFpuRegisters = 1, ContextSizeCheck = 2 };

struct Run {
    std::uint64_t threadStack;
    std::uint64_t threadFramePointer;
    std::uint64_t stackTop;
    FiberObject* current;
    FiberObject* previous;
    std::uint64_t argOnRunTo;
    std::uint64_t argOnReturn;
    bool entryReturned;
};

std::atomic<bool> contextSizeCheck{false};
thread_local Run* activeRun = nullptr;

[[gnu::noinline]] Run* ActiveRun() {
    __asm__ volatile("" ::: "memory");
    return activeRun;
}

[[gnu::noinline]] void SetActiveRun(Run* run) {
    __asm__ volatile("" ::: "memory");
    activeRun = run;
}

std::atomic_ref<std::uint32_t> StateOf(FiberObject* fiber) { return std::atomic_ref<std::uint32_t>(fiber->state); }

bool Claim(FiberObject* fiber) {
    std::uint32_t expected = Idle;
    return StateOf(fiber).compare_exchange_strong(expected, Running, std::memory_order_acq_rel);
}

void Release(FiberObject* fiber) { StateOf(fiber).store(Idle, std::memory_order_release); }

std::int32_t Check(const FiberObject* fiber) {
    if (fiber == nullptr) return SCE_FIBER_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(fiber) & 7) != 0) return SCE_FIBER_ERROR_ALIGNMENT;
    if (fiber->magic_start != SignatureStart || fiber->magic_end != SignatureEnd) return SCE_FIBER_ERROR_INVALID;
    return 0;
}

void FinishSwitch(Run* run) {
    if (run->previous == nullptr) return;
    Release(run->previous);
    run->previous = nullptr;
}

void CheckStackOverflow(const FiberObject* fiber) {
    if (fiber->addr_context != nullptr && *static_cast<const std::uint64_t*>(fiber->addr_context) != StackSignature)
        throw std::runtime_error(std::string("fiber stack overflow in ") + fiber->name);
}

#if defined(__x86_64__) && !defined(_WIN32)

[[gnu::naked]] void SwapStack(std::uint64_t*, std::uint64_t) {
    __asm__(
        "pushq %rbp\n\t"
        "pushq %rbx\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "subq $8, %rsp\n\t"
        "stmxcsr (%rsp)\n\t"
        "fnstcw 4(%rsp)\n\t"
        "movq %rsp, (%rdi)\n\t"
        "movq %rsi, %rsp\n\t"
        "ldmxcsr (%rsp)\n\t"
        "fldcw 4(%rsp)\n\t"
        "addq $8, %rsp\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %rbx\n\t"
        "popq %rbp\n\t"
        "retq");
}

[[gnu::naked]] void StartTrampoline() {
    __asm__(
        "movq %r12, %rdi\n\t"
        "callq *%r13\n\t"
        "ud2");
}

std::uint64_t StackPointer() {
    std::uint64_t pointer;
    __asm__ volatile("movq %%rsp, %0" : "=r"(pointer));
    return pointer;
}

[[noreturn]] void FiberStart(FiberObject* fiber) {
    Run* run = ActiveRun();
    FinishSwitch(run);
    fiber->entry(fiber->arg_on_initialize, run->argOnRunTo);
    run = ActiveRun();
    run->entryReturned = true;
    std::uint64_t discarded = 0;
    SwapStack(&discarded, run->threadStack);
    __builtin_unreachable();
}

std::uint64_t PrepareStack(std::uint64_t top, FiberObject* fiber) {
    auto* frame = reinterpret_cast<std::uint64_t*>((top & ~std::uint64_t{15}) - 16 - 64);
    std::uint32_t mxcsr = 0x9fc0;
    std::uint16_t controlWord = 0x037f;
    if ((fiber->flags & SetFpuRegisters) == 0) {
        __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
        __asm__ volatile("fnstcw %0" : "=m"(controlWord));
    }
    frame[0] = mxcsr | (static_cast<std::uint64_t>(controlWord) << 32);
    frame[1] = 0;
    frame[2] = 0;
    frame[3] = reinterpret_cast<std::uint64_t>(&FiberStart);
    frame[4] = reinterpret_cast<std::uint64_t>(fiber);
    frame[5] = 0;
    frame[6] = 0;
    frame[7] = reinterpret_cast<std::uint64_t>(&StartTrampoline);
    return reinterpret_cast<std::uint64_t>(frame);
}

#elif defined(__x86_64__) && defined(_WIN32)

[[gnu::naked]] void SwapStack(std::uint64_t*, std::uint64_t) {
    __asm__(
        "pushq %rbp\n\t"
        "pushq %rbx\n\t"
        "pushq %rdi\n\t"
        "pushq %rsi\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "pushq %gs:0x8\n\t"
        "pushq %gs:0x10\n\t"
        "subq $168, %rsp\n\t"
        "movups %xmm6, 0(%rsp)\n\t"
        "movups %xmm7, 16(%rsp)\n\t"
        "movups %xmm8, 32(%rsp)\n\t"
        "movups %xmm9, 48(%rsp)\n\t"
        "movups %xmm10, 64(%rsp)\n\t"
        "movups %xmm11, 80(%rsp)\n\t"
        "movups %xmm12, 96(%rsp)\n\t"
        "movups %xmm13, 112(%rsp)\n\t"
        "movups %xmm14, 128(%rsp)\n\t"
        "movups %xmm15, 144(%rsp)\n\t"
        "stmxcsr 160(%rsp)\n\t"
        "fnstcw 164(%rsp)\n\t"
        "movq %rsp, (%rcx)\n\t"
        "movq %rdx, %rsp\n\t"
        "movups 0(%rsp), %xmm6\n\t"
        "movups 16(%rsp), %xmm7\n\t"
        "movups 32(%rsp), %xmm8\n\t"
        "movups 48(%rsp), %xmm9\n\t"
        "movups 64(%rsp), %xmm10\n\t"
        "movups 80(%rsp), %xmm11\n\t"
        "movups 96(%rsp), %xmm12\n\t"
        "movups 112(%rsp), %xmm13\n\t"
        "movups 128(%rsp), %xmm14\n\t"
        "movups 144(%rsp), %xmm15\n\t"
        "ldmxcsr 160(%rsp)\n\t"
        "fldcw 164(%rsp)\n\t"
        "addq $168, %rsp\n\t"
        "popq %rax\n\t"
        "movq %rax, %gs:0x10\n\t"
        "popq %rax\n\t"
        "movq %rax, %gs:0x8\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %rsi\n\t"
        "popq %rdi\n\t"
        "popq %rbx\n\t"
        "popq %rbp\n\t"
        "retq");
}

[[gnu::naked]] void StartTrampoline() {
    __asm__(
        "subq $32, %rsp\n\t"
        "movq %r12, %rcx\n\t"
        "callq *%r13\n\t"
        "ud2");
}

std::uint64_t StackPointer() {
    std::uint64_t pointer;
    __asm__ volatile("movq %%rsp, %0" : "=r"(pointer));
    return pointer;
}

[[noreturn]] void FiberStart(FiberObject* fiber) {
    Run* run = ActiveRun();
    FinishSwitch(run);
    fiber->entry(fiber->arg_on_initialize, run->argOnRunTo);
    run = ActiveRun();
    run->entryReturned = true;
    std::uint64_t discarded = 0;
    SwapStack(&discarded, run->threadStack);
    __builtin_unreachable();
}

std::uint64_t PrepareStack(std::uint64_t top, FiberObject* fiber) {
    auto* frame = reinterpret_cast<std::uint64_t*>((top & ~std::uint64_t{15}) - 256 - 32);
    std::uint32_t mxcsr = 0x9fc0;
    std::uint16_t controlWord = 0x037f;
    if ((fiber->flags & SetFpuRegisters) == 0) {
        __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
        __asm__ volatile("fnstcw %0" : "=m"(controlWord));
    }
    std::uint64_t stackBase = 0;
    std::uint64_t stackLimit = 0;
    if (fiber->addr_context != nullptr) {
        stackBase = top;
        stackLimit = reinterpret_cast<std::uint64_t>(fiber->addr_context);
    } else {
        __asm__ volatile("movq %%gs:0x8, %0" : "=r"(stackBase));
        __asm__ volatile("movq %%gs:0x10, %0" : "=r"(stackLimit));
    }
    std::memset(frame, 0, 256);
    frame[20] = mxcsr | (static_cast<std::uint64_t>(controlWord) << 32);
    frame[21] = stackLimit;
    frame[22] = stackBase;
    frame[26] = reinterpret_cast<std::uint64_t>(fiber);
    frame[25] = reinterpret_cast<std::uint64_t>(&FiberStart);
    frame[31] = reinterpret_cast<std::uint64_t>(&StartTrampoline);
    return reinterpret_cast<std::uint64_t>(frame);
}

#else

[[noreturn]] void SwapStack(std::uint64_t*, std::uint64_t) { throw std::runtime_error("fiber stack switching is only implemented for x86-64 hosts"); }
std::uint64_t StackPointer() { return reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)); }
std::uint64_t PrepareStack(std::uint64_t, FiberObject*) { return 0; }

#endif

void Transfer(std::uint64_t* save, std::uint64_t target) {
    __asm__ volatile("" ::: "memory");
    SwapStack(save, target);
    __asm__ volatile("" ::: "memory");
}

std::uint64_t TargetStack(const Run* run, FiberObject* fiber) {
    if (fiber->saved_stack != 0) return fiber->saved_stack;
    const auto top = fiber->addr_context != nullptr ? reinterpret_cast<std::uint64_t>(fiber->addr_context) + fiber->size_context : run->stackTop;
    return PrepareStack(top, fiber);
}

}

extern "C" {

int32_t APS5_VABI _sceFiberInitializeImpl_nid_postfix(FiberObject* fiber, const char* name, FiberEntry entry, uint64_t arg_on_initialize, void* addr_context, uint64_t size_context, const FiberOptParam* opt_param, uint32_t build_version) {
    if (fiber == nullptr || name == nullptr || entry == nullptr) return SCE_FIBER_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(fiber) & 7) != 0 || (reinterpret_cast<std::uintptr_t>(addr_context) & 15) != 0) return SCE_FIBER_ERROR_ALIGNMENT;
    if (opt_param != nullptr && (reinterpret_cast<std::uintptr_t>(opt_param) & 7) != 0) return SCE_FIBER_ERROR_ALIGNMENT;
    if (size_context != 0 && size_context < ContextMinimumSize) return SCE_FIBER_ERROR_RANGE;
    if ((size_context & 15) != 0 || (addr_context == nullptr) != (size_context == 0)) return SCE_FIBER_ERROR_INVALID;
    if (opt_param != nullptr && opt_param->magic != OptSignature) return SCE_FIBER_ERROR_INVALID;
    std::uint32_t flags = build_version >= FpuBuildVersion ? SetFpuRegisters : 0;
    if (contextSizeCheck.load()) flags |= ContextSizeCheck;
    std::strncpy(fiber->name, name, FIBER_MAX_NAME_LENGTH);
    fiber->name[FIBER_MAX_NAME_LENGTH] = '\0';
    fiber->entry = entry;
    fiber->arg_on_initialize = arg_on_initialize;
    fiber->addr_context = addr_context;
    fiber->size_context = size_context;
    fiber->saved_stack = 0;
    fiber->flags = flags;
    if (addr_context != nullptr) {
        auto* words = static_cast<std::uint64_t*>(addr_context);
        words[0] = StackSignature;
        if ((flags & ContextSizeCheck) != 0)
            for (std::uint64_t index = 1; index < size_context / sizeof(std::uint64_t); ++index) words[index] = StackFill;
    }
    fiber->state = Idle;
    fiber->magic_start = SignatureStart;
    fiber->magic_end = SignatureEnd;
    return 0;
}

int32_t APS5_VABI sceFiberOptParamInitialize(FiberOptParam* opt_param) {
    if (opt_param == nullptr) return SCE_FIBER_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(opt_param) & 7) != 0) return SCE_FIBER_ERROR_ALIGNMENT;
    opt_param->magic = OptSignature;
    return 0;
}

int32_t APS5_VABI sceFiberFinalize(FiberObject* fiber) {
    if (const auto error = Check(fiber); error != 0) return error;
    std::uint32_t expected = Idle;
    if (!StateOf(fiber).compare_exchange_strong(expected, Finalized, std::memory_order_acq_rel)) return SCE_FIBER_ERROR_STATE;
    fiber->magic_start = 0;
    fiber->magic_end = 0;
    return 0;
}

int32_t APS5_VABI sceFiberRun_nid_postfix(FiberObject* fiber, uint64_t arg_on_run, uint64_t* arg_on_return) {
    if (const auto error = Check(fiber); error != 0) return error;
    if (ActiveRun() != nullptr) return SCE_FIBER_ERROR_PERMISSION;
    if (!Claim(fiber)) return SCE_FIBER_ERROR_STATE;
    Run run{};
    run.threadFramePointer = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0));
    run.stackTop = (StackPointer() - ThreadStackReserve) & ~std::uint64_t{15};
    run.current = fiber;
    run.argOnRunTo = arg_on_run;
    SetActiveRun(&run);
    Transfer(&run.threadStack, TargetStack(&run, fiber));
    SetActiveRun(nullptr);
    Release(run.current);
    if (run.entryReturned) throw std::runtime_error(std::string("fiber entry returned in ") + run.current->name);
    if (arg_on_return != nullptr) *arg_on_return = run.argOnReturn;
    return 0;
}

int32_t APS5_VABI sceFiberSwitch(FiberObject* fiber, uint64_t arg_on_run_to, uint64_t* arg_on_run) {
    if (const auto error = Check(fiber); error != 0) return error;
    Run* run = ActiveRun();
    if (run == nullptr) return SCE_FIBER_ERROR_PERMISSION;
    if (!Claim(fiber)) return SCE_FIBER_ERROR_STATE;
    FiberObject* self = run->current;
    CheckStackOverflow(self);
    run->previous = self;
    run->current = fiber;
    run->argOnRunTo = arg_on_run_to;
    std::uint64_t discarded = 0;
    Transfer(self->addr_context != nullptr ? &self->saved_stack : &discarded, TargetStack(run, fiber));
    run = ActiveRun();
    FinishSwitch(run);
    if (arg_on_run != nullptr) *arg_on_run = run->argOnRunTo;
    return 0;
}

int32_t APS5_VABI sceFiberReturnToThread(uint64_t arg_on_return, uint64_t* arg_on_run) {
    Run* run = ActiveRun();
    if (run == nullptr) return SCE_FIBER_ERROR_PERMISSION;
    FiberObject* self = run->current;
    CheckStackOverflow(self);
    run->argOnReturn = arg_on_return;
    std::uint64_t discarded = 0;
    Transfer(self->addr_context != nullptr ? &self->saved_stack : &discarded, run->threadStack);
    run = ActiveRun();
    FinishSwitch(run);
    if (arg_on_run != nullptr) *arg_on_run = run->argOnRunTo;
    return 0;
}

int32_t APS5_VABI sceFiberGetSelf(FiberObject** fiber) {
    if (fiber == nullptr) return SCE_FIBER_ERROR_NULL;
    const Run* run = ActiveRun();
    if (run == nullptr) return SCE_FIBER_ERROR_PERMISSION;
    *fiber = run->current;
    return 0;
}

int32_t APS5_VABI sceFiberGetInfo(FiberObject* fiber, FiberInfo* fiber_info) {
    if (fiber == nullptr || fiber_info == nullptr) return SCE_FIBER_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(fiber) & 7) != 0 || (reinterpret_cast<std::uintptr_t>(fiber_info) & 7) != 0) return SCE_FIBER_ERROR_ALIGNMENT;
    if (fiber_info->size != sizeof(FiberInfo)) return SCE_FIBER_ERROR_INVALID;
    if (const auto error = Check(fiber); error != 0) return error;
    fiber_info->entry = fiber->entry;
    fiber_info->arg_on_initialize = fiber->arg_on_initialize;
    fiber_info->addr_context = fiber->addr_context;
    fiber_info->size_context = fiber->size_context;
    std::memcpy(fiber_info->name, fiber->name, sizeof(fiber_info->name));
    fiber_info->size_context_margin = ~std::uint64_t{0};
    if ((fiber->flags & ContextSizeCheck) != 0 && fiber->addr_context != nullptr) {
        const auto* words = static_cast<const std::uint64_t*>(fiber->addr_context);
        const auto count = fiber->size_context / sizeof(std::uint64_t);
        std::uint64_t index = 1;
        while (index < count && words[index] == StackFill) ++index;
        fiber_info->size_context_margin = words[0] == StackSignature ? (index - 1) * sizeof(std::uint64_t) : 0;
    }
    return 0;
}

int32_t APS5_VABI sceFiberGetThreadFramePointerAddress(uint64_t* addr_frame_pointer) {
    if (addr_frame_pointer == nullptr) return SCE_FIBER_ERROR_NULL;
    Run* run = ActiveRun();
    if (run == nullptr) return SCE_FIBER_ERROR_PERMISSION;
    *addr_frame_pointer = reinterpret_cast<std::uint64_t>(&run->threadFramePointer);
    return 0;
}

int32_t APS5_VABI sceFiberRename(FiberObject* fiber, const char* name) {
    if (fiber == nullptr || name == nullptr) return SCE_FIBER_ERROR_NULL;
    if (const auto error = Check(fiber); error != 0) return error;
    std::strncpy(fiber->name, name, FIBER_MAX_NAME_LENGTH);
    fiber->name[FIBER_MAX_NAME_LENGTH] = '\0';
    return 0;
}

int32_t APS5_VABI sceFiberStartContextSizeCheck(uint32_t flags) {
    if (flags != 0) return SCE_FIBER_ERROR_INVALID;
    bool expected = false;
    return contextSizeCheck.compare_exchange_strong(expected, true) ? 0 : SCE_FIBER_ERROR_STATE;
}

int32_t APS5_VABI sceFiberStopContextSizeCheck(void) {
    bool expected = true;
    return contextSizeCheck.compare_exchange_strong(expected, false) ? 0 : SCE_FIBER_ERROR_STATE;
}

}

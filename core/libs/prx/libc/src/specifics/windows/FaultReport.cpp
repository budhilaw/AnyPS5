#include "prx/libc/include/specifics/windows/FaultReport.hpp"
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <typeinfo>
#include <unwind.h>

namespace {

std::atomic<int> reports{0};
thread_local bool reporting = false;

void write(const char* format, ...) {
    char buffer[512];
    va_list arguments;
    va_start(arguments, format);
    int length = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (length <= 0) return;
    if (length >= static_cast<int>(sizeof(buffer))) length = sizeof(buffer) - 1;
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), buffer, static_cast<DWORD>(length), &written, nullptr);
}

bool moduleOffset(std::uint64_t address, const char*& name, std::uint64_t& offset, char (&path)[MAX_PATH]) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &module) || module == nullptr) return false;
    if (GetModuleFileNameA(module, path, MAX_PATH) == 0) return false;
    name = path;
    for (const char* cursor = path; *cursor != 0; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') name = cursor + 1;
    }
    offset = address - reinterpret_cast<std::uint64_t>(module);
    return true;
}

bool readable(std::uint64_t address, std::size_t bytes) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info)) return false;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
    return address + bytes <= reinterpret_cast<std::uint64_t>(info.BaseAddress) + info.RegionSize;
}

bool executable(std::uint64_t address) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT) return false;
    const auto protection = info.Protect & 0xffu;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

void describeRegion(std::uint64_t address) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info)) {
        write("  region of 0x%016llx cannot be queried\n", static_cast<unsigned long long>(address));
        return;
    }
    write("  region 0x%016llx+0x%llx allocation 0x%016llx state 0x%lx protect 0x%lx type 0x%lx\n", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(info.BaseAddress)), static_cast<unsigned long long>(info.RegionSize), static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(info.AllocationBase)), info.State, info.Protect, info.Type);
}

void describe(const char* label, std::uint64_t address) {
    char path[MAX_PATH];
    const char* name = nullptr;
    std::uint64_t offset = 0;
    if (moduleOffset(address, name, offset, path)) write("  %s 0x%016llx %s+0x%llx\n", label, static_cast<unsigned long long>(address), name, static_cast<unsigned long long>(offset));
    else write("  %s 0x%016llx\n", label, static_cast<unsigned long long>(address));
}

}

void reportCxxThrow(const EXCEPTION_POINTERS* exception);

void ReportFatalException(const EXCEPTION_POINTERS* exception) {
    const auto* record = exception->ExceptionRecord;
    if (record->ExceptionCode == 0x20474343u) {
        reportCxxThrow(exception);
        return;
    }
    switch (record->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_IN_PAGE_ERROR:
        break;
    default:
        return;
    }
    if (reporting || reports.fetch_add(1) >= 4) return;
    reporting = true;
    const auto* context = exception->ContextRecord;
    write("fatal exception 0x%08lx on thread %lu\n", record->ExceptionCode, GetCurrentThreadId());
    describe("rip", context->Rip);
    if ((record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) && record->NumberParameters >= 2) {
        const auto access = record->ExceptionInformation[0];
        write("  %s at 0x%016llx\n", access == 0 ? "read" : access == 1 ? "write" : "execute", static_cast<unsigned long long>(record->ExceptionInformation[1]));
        describeRegion(record->ExceptionInformation[1]);
    }
    write("  rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx\n", context->Rax, context->Rbx, context->Rcx, context->Rdx);
    write("  rsi=%016llx rdi=%016llx rbp=%016llx rsp=%016llx\n", context->Rsi, context->Rdi, context->Rbp, context->Rsp);
    write("  r8 =%016llx r9 =%016llx r10=%016llx r11=%016llx\n", context->R8, context->R9, context->R10, context->R11);
    write("  r12=%016llx r13=%016llx r14=%016llx r15=%016llx\n", context->R12, context->R13, context->R14, context->R15);
    int found = 0;
    for (std::uint64_t slot = context->Rsp; found < 48 && slot < context->Rsp + 8192; slot += 8) {
        if (!readable(slot, 8)) break;
        const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
        if (value < 0x10000 || !executable(value)) continue;
        char label[32];
        std::snprintf(label, sizeof(label), "[rsp+0x%03llx]", static_cast<unsigned long long>(slot - context->Rsp));
        describe(label, value);
        ++found;
    }
    if (record->ExceptionCode != EXCEPTION_STACK_OVERFLOW) {
        std::fflush(stdout);
        std::fflush(stderr);
    }
    reporting = false;
}

namespace {

constexpr DWORD GccThrow = 0x20474343u;
constexpr std::size_t CxaHeaderBytes = 80;

void describeCxxException(const EXCEPTION_RECORD* record) {
    if (record->NumberParameters < 1) return;
    const auto unwind = static_cast<std::uint64_t>(record->ExceptionInformation[0]);
    if (unwind < CxaHeaderBytes || !readable(unwind - CxaHeaderBytes, CxaHeaderBytes + sizeof(_Unwind_Exception))) return;
    const auto* type = *reinterpret_cast<const std::type_info* const*>(unwind - CxaHeaderBytes);
    if (type == nullptr || !readable(reinterpret_cast<std::uint64_t>(type), sizeof(void*) * 2)) return;
    write("  C++ exception of type %s\n", type->name());
}

LONG WINAPI unhandledFilter(EXCEPTION_POINTERS* exception) {
    const auto* record = exception->ExceptionRecord;
    std::fflush(stdout);
    std::fflush(stderr);
    write("unhandled exception 0x%08lx on thread %lu\n", record->ExceptionCode, GetCurrentThreadId());
    if (record->ExceptionCode == GccThrow) describeCxxException(record);
    const auto* context = exception->ContextRecord;
    describe("rip", context->Rip);
    int found = 0;
    for (std::uint64_t slot = context->Rsp; found < 48 && slot < context->Rsp + 16384; slot += 8) {
        if (!readable(slot, 8)) break;
        const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
        if (value < 0x10000 || !executable(value)) continue;
        char label[32];
        std::snprintf(label, sizeof(label), "[rsp+0x%04llx]", static_cast<unsigned long long>(slot - context->Rsp));
        describe(label, value);
        ++found;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

const auto previousUnhandledFilter = SetUnhandledExceptionFilter(unhandledFilter);

}

bool firstThrowFrom(std::uint64_t site) {
    static std::atomic<std::uint64_t> seen[512]{};
    if (site == 0) return false;
    for (std::size_t probe = 0; probe < 512; ++probe) {
        auto& slot = seen[(site * 0x9e3779b97f4a7c15ull >> 55) + probe & 511];
        auto expected = slot.load(std::memory_order_relaxed);
        if (expected == site) return false;
        if (expected == 0 && slot.compare_exchange_strong(expected, site)) return true;
        if (expected == site) return false;
    }
    return false;
}

void reportCxxThrow(const EXCEPTION_POINTERS* exception) {
    if (reporting) return;
    const auto* context = exception->ContextRecord;
    std::uint64_t site = 0;
    for (std::uint64_t slot = context->Rsp; slot < context->Rsp + 4096; slot += 8) {
        if (!readable(slot, 8)) break;
        const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
        if (value < 0x10000 || !executable(value)) continue;
        char path[MAX_PATH];
        const char* name = nullptr;
        std::uint64_t offset = 0;
        if (!moduleOffset(value, name, offset, path)) continue;
        if (std::strstr(name, "libgcc") != nullptr || std::strstr(name, "libstdc++") != nullptr || std::strstr(name, "KERNELBASE") != nullptr || std::strstr(name, "ntdll") != nullptr) continue;
        site = value;
        break;
    }
    if (!firstThrowFrom(site)) return;
    reporting = true;
    write("C++ throw on thread %lu\n", GetCurrentThreadId());
    describeCxxException(exception->ExceptionRecord);
    int found = 0;
    for (std::uint64_t slot = context->Rsp; found < 6 && slot < context->Rsp + 4096; slot += 8) {
        if (!readable(slot, 8)) break;
        const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
        if (value < 0x10000 || !executable(value)) continue;
        char label[32];
        std::snprintf(label, sizeof(label), "[rsp+0x%03llx]", static_cast<unsigned long long>(slot - context->Rsp));
        describe(label, value);
        ++found;
    }
    reporting = false;
}

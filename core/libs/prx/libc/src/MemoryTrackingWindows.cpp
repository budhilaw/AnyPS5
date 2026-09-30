#include "prx/libc/include/MemoryTrackingPlatform.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include "prx/libc/include/SlowOperation.hpp"
#include "prx/libc/include/specifics/windows/NativeProtection.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace GuestMemoryTracking::Platform {
namespace {

FaultHandler faultHandler = nullptr;

struct FaultSite {
    std::uint64_t count = 0;
    std::uint64_t lastAddress = 0;
    bool write = false;
};

void describeCode(std::uint64_t address, char* text, std::size_t size) {
    HMODULE module = nullptr;
    char path[MAX_PATH];
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &module) || module == nullptr || GetModuleFileNameA(module, path, MAX_PATH) == 0) {
        std::snprintf(text, size, "?+0x%llx", static_cast<unsigned long long>(address));
        return;
    }
    const char* name = path;
    for (const char* cursor = path; *cursor != 0; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') name = cursor + 1;
    }
    std::snprintf(text, size, "%s+0x%llx", name, static_cast<unsigned long long>(address - reinterpret_cast<std::uintptr_t>(module)));
}

void recordFault(std::uint64_t instruction, std::uint64_t address, bool write) {
    static std::mutex mutex;
    static auto* sites = new std::unordered_map<std::uint64_t, FaultSite>;
    static auto* seen = new std::unordered_set<std::uint64_t>;
    static auto reported = std::chrono::steady_clock::now();
    bool first = false;
    {
        std::lock_guard lock(mutex);
        first = seen->insert(instruction).second;
    }
    if (first) {
        char where[160];
        describeCode(instruction, where, sizeof(where));
        std::fprintf(stderr, "[faults] first %s at %s touching 0x%llx\n", write ? "write" : "read", where, static_cast<unsigned long long>(address));
        std::lock_guard trackingLock(GuestMemoryTrackingMutex_nid_postfix());
        GuestMemoryTrackingDescribe_nid_postfix(address, 1);
    }
    std::lock_guard lock(mutex);
    auto& site = (*sites)[instruction];
    ++site.count;
    site.lastAddress = address;
    site.write = write;
    const auto now = std::chrono::steady_clock::now();
    if (now - reported < std::chrono::seconds(5)) return;
    reported = now;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> sorted;
    for (const auto& [code, entry] : *sites) sorted.emplace_back(entry.count, code);
    std::sort(sorted.rbegin(), sorted.rend());
    for (std::size_t index = 0; index < sorted.size() && index < 12; ++index) {
        const auto& entry = sites->at(sorted[index].second);
        char where[160];
        describeCode(sorted[index].second, where, sizeof(where));
        std::fprintf(stderr, "[faults] %llu %s %s last 0x%llx\n", static_cast<unsigned long long>(entry.count), entry.write ? "write" : "read", where, static_cast<unsigned long long>(entry.lastAddress));
    }
    sites->clear();
}

LONG CALLBACK handleException(EXCEPTION_POINTERS* exception) {
    const auto* record = exception->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2 || record->ExceptionInformation[0] > 1) return EXCEPTION_CONTINUE_SEARCH;
    try {
        if (faultHandler(record->ExceptionInformation[1], record->ExceptionInformation[0] == 1)) {
            if (SlowOperationEnabled_nid_no_patch()) recordFault(exception->ContextRecord->Rip, record->ExceptionInformation[1], record->ExceptionInformation[0] == 1);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    } catch (...) {
        std::terminate();
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void protect(std::uint64_t address, std::size_t bytes, DWORD protection) {
    if (!ProtectNativeRange(address, bytes, protection)) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "guest memory tracking VirtualProtect failed");
}

}

std::size_t PageSize() {
    static const auto size = [] {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        if (info.dwPageSize == 0) throw std::runtime_error("invalid native page size");
        return static_cast<std::size_t>(info.dwPageSize);
    }();
    return size;
}

void Install(FaultHandler handler) {
    if (handler == nullptr || faultHandler != nullptr) throw std::runtime_error("invalid guest memory fault handler installation");
    faultHandler = handler;
    if (AddVectoredExceptionHandler(1, handleException) == nullptr) {
        const auto error = GetLastError();
        faultHandler = nullptr;
        throw std::system_error(static_cast<int>(error), std::system_category(), "guest memory fault handler installation failed");
    }
}

std::vector<Region> Query(std::uint64_t address, std::size_t bytes) {
    std::vector<Region> regions;
    const auto end = address + bytes;
    while (address < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info)) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "tracked guest memory query failed");
        if (info.State != MEM_COMMIT || info.Protect != PAGE_READWRITE) throw std::runtime_error("tracked render memory must be committed, writable and non-executable");
        const auto next = std::min(end, reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize);
        if (next <= address) throw std::runtime_error("invalid tracked guest memory mapping");
        regions.push_back({address, static_cast<std::size_t>(next - address), info.Protect});
        address = next;
    }
    return regions;
}

void Protect(std::uint64_t address, std::size_t bytes, Protection protection) {
    if (protection != Protection::None && protection != Protection::Read) throw std::invalid_argument("invalid tracked page protection");
    protect(address, bytes, protection == Protection::None ? PAGE_NOACCESS : PAGE_READONLY);
}

void Restore(const std::vector<Region>& regions) {
    for (const auto& region : regions) protect(region.address, region.bytes, static_cast<DWORD>(region.protection));
}

}

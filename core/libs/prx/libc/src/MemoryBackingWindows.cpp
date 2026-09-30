#include "prx/libc/include/MemoryBackingPlatform.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/specifics/windows/NativeProtection.hpp"
#include "prx/libc/include/specifics/windows/FaultReport.hpp"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace GuestMemoryBacking::Platform {
namespace {

using VirtualAlloc2Function = PVOID (WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
using MapViewOfFile3Function = PVOID (WINAPI*)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
using UnmapViewOfFile2Function = BOOL (WINAPI*)(HANDLE, PVOID, ULONG);

struct PlaceholderApi {
    VirtualAlloc2Function virtualAlloc2;
    MapViewOfFile3Function mapViewOfFile3;
    UnmapViewOfFile2Function unmapViewOfFile2;
};

struct PhysicalMemory {
    HANDLE section;
    std::byte* alias;
};

struct View {
    std::uint64_t end;
    std::uint64_t offset;
};

struct SavedProtection {
    std::uint64_t address;
    std::size_t bytes;
    DWORD protection;
};

std::recursive_mutex viewMutex;
std::map<std::uint64_t, View> physicalViews;
thread_local std::uintptr_t retriedAddress = 0;
thread_local unsigned retries = 0;

DWORD nativeProtection(int protection) {
    if ((protection & 4) != 0) return (protection & 2) != 0 ? PAGE_EXECUTE_READWRITE : (protection & 1) != 0 ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    if ((protection & 2) != 0) return PAGE_READWRITE;
    return (protection & 1) != 0 ? PAGE_READONLY : PAGE_NOACCESS;
}

void check(bool success, const char* operation) {
    if (!success) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
}

std::size_t granularity() {
    static const auto value = [] {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        return static_cast<std::size_t>(info.dwAllocationGranularity);
    }();
    return value;
}

const PlaceholderApi& placeholders() {
    static const auto api = [] {
        const HMODULE kernelBase = GetModuleHandleW(L"kernelbase.dll");
        check(kernelBase != nullptr, "GetModuleHandle kernelbase");
        PlaceholderApi result{
            reinterpret_cast<VirtualAlloc2Function>(reinterpret_cast<void*>(GetProcAddress(kernelBase, "VirtualAlloc2"))),
            reinterpret_cast<MapViewOfFile3Function>(reinterpret_cast<void*>(GetProcAddress(kernelBase, "MapViewOfFile3"))),
            reinterpret_cast<UnmapViewOfFile2Function>(reinterpret_cast<void*>(GetProcAddress(kernelBase, "UnmapViewOfFile2"))),
        };
        if (result.virtualAlloc2 == nullptr || result.mapViewOfFile3 == nullptr || result.unmapViewOfFile2 == nullptr) throw std::runtime_error("guest memory needs VirtualAlloc2, MapViewOfFile3 and UnmapViewOfFile2 (Windows 10 1803 or newer)");
        return result;
    }();
    return api;
}

const PhysicalMemory& physicalMemory() {
    static const auto memory = [] {
        HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, static_cast<DWORD>(GuestPhysicalMemoryBytes >> 32u), static_cast<DWORD>(GuestPhysicalMemoryBytes), nullptr);
        check(section != nullptr, "CreateFileMapping guest physical memory");
        void* alias = MapViewOfFile(section, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, GuestPhysicalMemoryBytes);
        check(alias != nullptr, "MapViewOfFile guest physical memory alias");
        return PhysicalMemory{section, static_cast<std::byte*>(alias)};
    }();
    return memory;
}

std::string describe(const char* what, std::uint64_t address, std::size_t bytes) {
    char text[160];
    std::snprintf(text, sizeof(text), "%s 0x%llx+0x%zx", what, static_cast<unsigned long long>(address), bytes);
    return text;
}

void* reservePlaceholder(void* address, std::size_t bytes, std::size_t alignment) {
    const auto& api = placeholders();
    if (address != nullptr) {
        void* placeholder = api.virtualAlloc2(GetCurrentProcess(), address, bytes, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
        if (placeholder == nullptr) throw std::runtime_error(describe("fixed guest view is unavailable:", reinterpret_cast<std::uintptr_t>(address), bytes) + ", error " + std::to_string(GetLastError()));
        if (placeholder != address) throw std::runtime_error("guest backing placeholder address mismatch");
        return placeholder;
    }
    MEM_ADDRESS_REQUIREMENTS requirements{};
    requirements.Alignment = std::max(alignment, granularity());
    MEM_EXTENDED_PARAMETER parameter{};
    parameter.Type = MemExtendedParameterAddressRequirements;
    parameter.Pointer = &requirements;
    void* placeholder = api.virtualAlloc2(GetCurrentProcess(), nullptr, bytes, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, &parameter, 1);
    check(placeholder != nullptr, "VirtualAlloc2 guest backing placeholder");
    return placeholder;
}

void* mapIntoPlaceholder(HANDLE section, void* placeholder, std::uint64_t offset, std::size_t bytes) {
    return placeholders().mapViewOfFile3(section, GetCurrentProcess(), placeholder, offset, bytes, MEM_REPLACE_PLACEHOLDER, PAGE_EXECUTE_READWRITE, nullptr, 0);
}

void protectNative(std::uint64_t address, std::size_t bytes, DWORD protection, const char* operation) {
    if (!ProtectNativeRange(address, bytes, protection)) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), describe(operation, address, bytes));
}

bool accessibleNow(std::uintptr_t address, ULONG_PTR access) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT) return false;
    const auto protection = info.Protect & 0xffu;
    if ((info.Protect & PAGE_GUARD) != 0 || protection == PAGE_NOACCESS) return false;
    if (access == 8) return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    if (access == 1) return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    return true;
}

bool retryRemappedAccess(const EXCEPTION_RECORD* record) {
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2) return false;
    const auto address = static_cast<std::uintptr_t>(record->ExceptionInformation[1]);
    { std::lock_guard lock(viewMutex); }
    if (address == 0 || !accessibleNow(address, record->ExceptionInformation[0])) return false;
    if (retriedAddress != address) {
        retriedAddress = address;
        retries = 0;
    }
    return ++retries <= 16;
}

LONG CALLBACK lastChanceHandler(EXCEPTION_POINTERS* exception) {
    if (retryRemappedAccess(exception->ExceptionRecord)) return EXCEPTION_CONTINUE_EXECUTION;
    ReportFatalException(exception);
    return EXCEPTION_CONTINUE_SEARCH;
}

const bool lastChanceHandlerInstalled = AddVectoredExceptionHandler(0, lastChanceHandler) != nullptr;

std::vector<SavedProtection> saveProtection(std::uint64_t address, std::uint64_t end) {
    std::vector<SavedProtection> saved;
    while (address < end) {
        MEMORY_BASIC_INFORMATION info{};
        check(VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) == sizeof(info), "VirtualQuery guest physical view");
        const auto next = std::min<std::uint64_t>(end, reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize);
        if (next <= address) throw std::runtime_error("invalid guest physical view region");
        saved.push_back({address, static_cast<std::size_t>(next - address), info.Protect});
        address = next;
    }
    return saved;
}

void remapPiece(std::uint64_t address, std::uint64_t end, std::uint64_t offset, const std::vector<SavedProtection>& protections) {
    if (address == end) return;
    void* view = mapIntoPlaceholder(physicalMemory().section, reinterpret_cast<void*>(address), offset, static_cast<std::size_t>(end - address));
    if (view != reinterpret_cast<void*>(address)) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), describe("MapViewOfFile3 remaining guest physical view", address, static_cast<std::size_t>(end - address)));
    for (const auto& saved : protections) protectNative(saved.address, saved.bytes, saved.protection, "VirtualProtect remaining guest physical view");
    physicalViews[address] = {end, offset};
}

void splitView(std::uint64_t base, const View& view, std::uint64_t first, std::uint64_t last) {
    const auto& api = placeholders();
    const auto left = saveProtection(base, first);
    const auto right = saveProtection(last, view.end);
    physicalViews.erase(base);
    check(api.unmapViewOfFile2(GetCurrentProcess(), reinterpret_cast<void*>(base), MEM_PRESERVE_PLACEHOLDER) != FALSE, "UnmapViewOfFile2 guest physical view");
    if (base < first) check(VirtualFree(reinterpret_cast<void*>(base), static_cast<std::size_t>(first - base), MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) != FALSE, "VirtualFree split guest physical placeholder");
    if (last < view.end) check(VirtualFree(reinterpret_cast<void*>(first), static_cast<std::size_t>(last - first), MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) != FALSE, "VirtualFree split guest physical placeholder");
    check(VirtualFree(reinterpret_cast<void*>(first), 0, MEM_RELEASE) != FALSE, "VirtualFree released guest physical placeholder");
    remapPiece(base, first, view.offset, left);
    remapPiece(last, view.end, view.offset + (last - base), right);
}

}

Mapping Map(void* address, std::size_t bytes, std::size_t alignment, int protection) {
    if (address != nullptr && reinterpret_cast<std::uintptr_t>(address) % granularity() != 0) throw std::invalid_argument(describe("fixed guest view is not aligned to the 64 KiB Windows allocation granularity:", reinterpret_cast<std::uintptr_t>(address), bytes));
    const auto size = static_cast<std::uint64_t>(bytes);
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, static_cast<DWORD>(size >> 32u), static_cast<DWORD>(size), nullptr);
    check(section != nullptr, "CreateFileMapping guest backing");
    void* alias = nullptr;
    void* placeholder = nullptr;
    void* guest = nullptr;
    try {
        alias = MapViewOfFile(section, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, bytes);
        check(alias != nullptr, "MapViewOfFile guest backing alias");
        placeholder = reservePlaceholder(address, bytes, alignment);
        guest = mapIntoPlaceholder(section, placeholder, 0, bytes);
        check(guest == placeholder, "MapViewOfFile3 guest memory");
        placeholder = nullptr;
        protectNative(reinterpret_cast<std::uintptr_t>(guest), bytes, nativeProtection(protection), "VirtualProtect guest backing view");
        return {reinterpret_cast<std::uintptr_t>(guest), bytes, alias, reinterpret_cast<std::uintptr_t>(section)};
    } catch (...) {
        if (guest != nullptr) check(UnmapViewOfFile(guest) != FALSE, "UnmapViewOfFile failed guest view");
        if (placeholder != nullptr) check(VirtualFree(placeholder, 0, MEM_RELEASE) != FALSE, "VirtualFree failed guest placeholder");
        if (alias != nullptr) check(UnmapViewOfFile(alias) != FALSE, "UnmapViewOfFile failed guest alias");
        check(CloseHandle(section) != FALSE, "CloseHandle failed guest backing");
        throw;
    }
}

Mapping MapPhysical(void* address, std::size_t bytes, std::size_t alignment, int protection, std::uint64_t offset) {
    if (offset > GuestPhysicalMemoryBytes || bytes > GuestPhysicalMemoryBytes - offset) throw std::out_of_range("guest physical mapping exceeds physical memory");
    const auto unit = granularity();
    if (offset % unit != 0 || (address != nullptr && reinterpret_cast<std::uintptr_t>(address) % unit != 0)) {
        char text[192];
        std::snprintf(text, sizeof(text), "guest physical mapping %p+0x%zx of offset 0x%llx is not aligned to the 64 KiB Windows allocation granularity", address, bytes, static_cast<unsigned long long>(offset));
        throw std::invalid_argument(text);
    }
    if (!lastChanceHandlerInstalled) throw std::runtime_error("guest memory needs its last-chance exception handler");
    const auto& memory = physicalMemory();
    std::lock_guard lock(viewMutex);
    void* placeholder = reservePlaceholder(address, bytes, alignment);
    void* guest = mapIntoPlaceholder(memory.section, placeholder, offset, bytes);
    if (guest != placeholder) {
        const auto error = GetLastError();
        VirtualFree(placeholder, 0, MEM_RELEASE);
        throw std::system_error(static_cast<int>(error), std::system_category(), describe("MapViewOfFile3 guest physical view", reinterpret_cast<std::uintptr_t>(placeholder), bytes));
    }
    try {
        check(VirtualAlloc(guest, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr, "VirtualAlloc commit guest physical memory");
        protectNative(reinterpret_cast<std::uintptr_t>(guest), bytes, nativeProtection(protection), "VirtualProtect guest physical view");
    } catch (...) {
        UnmapViewOfFile(guest);
        throw;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(guest);
    physicalViews[start] = {start + bytes, offset};
    return {start, bytes, memory.alias + offset, 0, true};
}

void UnmapViewRange(std::uint64_t address, std::size_t bytes) {
    const auto end = address + bytes;
    std::lock_guard lock(viewMutex);
    auto it = physicalViews.upper_bound(address);
    if (it == physicalViews.begin() || std::prev(it)->second.end < end) throw std::runtime_error(describe("guest physical unmap does not lie within one view:", address, bytes));
    --it;
    const auto base = it->first;
    const auto view = it->second;
    if (base == address && view.end == end) {
        physicalViews.erase(it);
        check(UnmapViewOfFile(reinterpret_cast<void*>(base)) != FALSE, "UnmapViewOfFile guest physical view");
        return;
    }
    const auto unit = granularity();
    if (address % unit != 0 || end % unit != 0) throw std::invalid_argument(describe("partial guest physical unmap is not aligned to the 64 KiB Windows allocation granularity:", address, bytes));
    splitView(base, view, address, end);
}

void UnmapView(const Mapping& mapping) {
    if (mapping.physical) {
        UnmapViewRange(mapping.address, mapping.bytes);
        return;
    }
    check(UnmapViewOfFile(reinterpret_cast<void*>(mapping.address)) != FALSE, "UnmapViewOfFile guest view");
}

void UnmapAlias(const Mapping& mapping) {
    if (mapping.physical) return;
    check(UnmapViewOfFile(mapping.alias) != FALSE, "UnmapViewOfFile guest alias");
    check(CloseHandle(reinterpret_cast<HANDLE>(mapping.handle)) != FALSE, "CloseHandle guest backing");
}

void Unmap(const Mapping& mapping) {
    UnmapView(mapping);
    UnmapAlias(mapping);
}

void Protect(std::uint64_t address, std::size_t bytes, int protection) {
    protectNative(address, bytes, nativeProtection(protection), "VirtualProtect guest backing protect");
}

void Deactivate(std::uint64_t address, std::size_t bytes) {
    protectNative(address, bytes, PAGE_NOACCESS, "VirtualProtect guest backing unmap");
}

}

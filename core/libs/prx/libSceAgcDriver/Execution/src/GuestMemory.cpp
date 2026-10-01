#include <cstdio>
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/CallerSymbol.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <optional>
#include <mutex>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#else
#include <fstream>
#include <sstream>
#endif

namespace AgcDriver::GuestMemory {
namespace {
void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("AGC driver: ") + reason);
}

std::uintptr_t checkedAddress(const void* pointer, std::size_t bytes, std::size_t alignment) {
    require(alignment != 0, "zero guest memory alignment");
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    if (address == 0 || address % alignment != 0) {
        char message[128];
        std::snprintf(message, sizeof(message), "null or misaligned address 0x%llx (%zu bytes, alignment %zu)", static_cast<unsigned long long>(address), bytes, alignment);
        throw std::runtime_error(std::string("AGC driver: ") + message);
    }
    require(bytes <= std::numeric_limits<std::uintptr_t>::max() - address, "address range overflow");
    return address;
}

#if defined(_WIN32) || defined(__APPLE__)
struct VerifiedRegion { std::uint64_t epoch = 0; std::uintptr_t first = 0; std::uintptr_t end = 0; bool writable = false; };
thread_local std::array<VerifiedRegion, 8> verified{};
thread_local std::size_t nextVerified = 0;
#endif

void checkPermissions(std::uintptr_t address, std::size_t bytes, bool writable, PerformanceTimer& timing) {
    auto cursor = address;
    const auto end = address + bytes;
#if defined(_WIN32) || defined(__APPLE__)
    const auto epoch = GuestAllocations::GuestAllocationsMapEpoch_nid_postfix();
    for (const auto& region : verified) {
        if (region.epoch == epoch && address >= region.first && end <= region.end && (region.writable || !writable)) return;
    }
    timing.Mark("verified_miss");
#endif
#ifdef _WIN32
    if (GuestAllocations::GuestAllocationsCovers_nid_postfix(address, bytes, writable)) {
        verified[nextVerified++ % verified.size()] = {epoch, address, end, writable};
        timing.Mark("registry");
        return;
    }
    bool rangeWritable = true;
    bool coveredByOneRegion = false;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION memory{};
        require(VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == sizeof(memory), "cannot query guest memory");
        require(memory.State == MEM_COMMIT && (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0, "guest memory is not readable");
        const auto protection = memory.Protect & 0xffu;
        require(protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY, "guest memory has no read permission");
        const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        require(memory.RegionSize <= std::numeric_limits<std::uintptr_t>::max() - base && base + memory.RegionSize > cursor, "invalid guest memory mapping");
        const bool regionWritable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
        require(!writable || regionWritable, "guest memory has no write permission");
        rangeWritable = rangeWritable && regionWritable;
        if (base <= address && base + memory.RegionSize >= end) {
            verified[nextVerified++ % verified.size()] = {epoch, base, base + memory.RegionSize, regionWritable};
            coveredByOneRegion = true;
        }
        cursor = std::min(end, base + memory.RegionSize);
    }
    if (!coveredByOneRegion) verified[nextVerified++ % verified.size()] = {epoch, address, end, rangeWritable};
    timing.Mark("query");
#elif defined(__APPLE__)
    while (cursor < end) {
        mach_vm_address_t first = cursor;
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        const auto result = mach_vm_region(mach_task_self(), &first, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object);
        if (object != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object);
        if (!(result == KERN_SUCCESS && first <= cursor)) {
            char message[160];
            std::snprintf(message, sizeof(message), "guest address range is not mapped: 0x%llx (%zu bytes), cursor 0x%llx", static_cast<unsigned long long>(address), bytes, static_cast<unsigned long long>(cursor));
            throw std::runtime_error(std::string("AGC driver: ") + message);
        }
        if ((info.protection & VM_PROT_READ) == 0 || (writable && (info.protection & VM_PROT_WRITE) == 0)) {
            char message[192];
            std::snprintf(message, sizeof(message), "guest memory is not %s: range 0x%llx (%zu bytes) at 0x%llx lies in a region 0x%llx+0x%llx with protection %u", writable ? "writable" : "readable", static_cast<unsigned long long>(address), bytes, static_cast<unsigned long long>(cursor), static_cast<unsigned long long>(first), static_cast<unsigned long long>(size), static_cast<unsigned>(info.protection));
            throw std::runtime_error(std::string("AGC driver: ") + message);
        }
        require(size <= std::numeric_limits<std::uintptr_t>::max() - first && first + size > cursor, "invalid guest memory mapping");
        if (first <= address && first + size >= end) {
            verified[nextVerified++ % verified.size()] = {epoch, static_cast<std::uintptr_t>(first), static_cast<std::uintptr_t>(first + size), (info.protection & VM_PROT_WRITE) != 0};
        }
        cursor = std::min(end, static_cast<std::uintptr_t>(first + size));
    }
#else
    (void)timing;
    std::ifstream maps("/proc/self/maps");
    require(maps.is_open(), "cannot query guest memory maps");
    std::string line;
    while (cursor < end && std::getline(maps, line)) {
        std::istringstream fields(line);
        std::uintptr_t first = 0;
        std::uintptr_t last = 0;
        char separator = 0;
        std::string permissions;
        require(static_cast<bool>(fields >> std::hex >> first >> separator >> last >> permissions) && separator == '-' && first < last && !permissions.empty(), "invalid guest memory map entry");
        if (last <= cursor) continue;
        require(first <= cursor && permissions[0] == 'r', "guest memory is not readable");
        require(!writable || (permissions.size() > 1 && permissions[1] == 'w'), "guest memory has no write permission");
        cursor = std::min(end, last);
    }
    if (!(cursor == end)) {
            char message[160];
            std::snprintf(message, sizeof(message), "guest address range is not mapped: 0x%llx (%zu bytes), cursor 0x%llx", static_cast<unsigned long long>(address), bytes, static_cast<unsigned long long>(cursor));
            throw std::runtime_error(std::string("AGC driver: ") + message);
        }
#endif
}

}

MemoryAccessScope::State& MemoryAccessScope::current() {
    thread_local State state;
    return state;
}

void CheckRange(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    const auto address = checkedAddress(pointer, bytes, alignment);
    PerformanceTimer timing("GuestMemory.CheckRange");
    MemoryAccessScope::Resolve(address, bytes, writable);
    timing.Mark("scope_resolve");
    GuestMemoryTracking::GuestMemoryTrackingResolve_nid_postfix(address, bytes, writable);
    timing.Mark("tracking_resolve");
    checkPermissions(address, bytes, writable, timing);
}

void CheckAccess(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    const auto address = checkedAddress(pointer, bytes, alignment);
    PerformanceTimer timing("GuestMemory.CheckAccess");
    if (GuestAllocations::GuestAllocationsCovers_nid_postfix(address, bytes, writable)) return;
    require(!writable || !GuestAllocations::GuestAllocationsCovers_nid_postfix(address, bytes, false), "guest memory has no write permission");
    GuestMemoryTracking::GuestMemoryTrackingValidate_nid_postfix(address, bytes, [&](std::uint64_t first, std::size_t count) {
        checkPermissions(static_cast<std::uintptr_t>(first), count, writable, timing);
    });
}

void Read(std::uint64_t address, std::span<std::byte> destination, std::size_t alignment) {
    if (destination.empty()) return;
    std::optional<PerformanceTimer> timing;
    if (destination.size() >= 4096) timing.emplace("GuestMemory.Read");
    static const bool traceCallers = std::getenv("ANYPS5_TRACE_READ_CALLERS") != nullptr;
    if (traceCallers) {
        static std::mutex callerMutex;
        static std::map<void*, std::pair<std::uint64_t, std::uint64_t>> callers;
        static auto last = std::chrono::steady_clock::now();
        std::lock_guard lock(callerMutex);
        auto& entry = callers[__builtin_return_address(0)];
        entry.first += destination.size();
        ++entry.second;
        if (std::chrono::steady_clock::now() - last > std::chrono::seconds(3)) {
            last = std::chrono::steady_clock::now();
            std::vector<std::pair<std::uint64_t, void*>> sorted;
            for (const auto& [caller, value] : callers) sorted.push_back({value.first, caller});
            std::sort(sorted.rbegin(), sorted.rend());
            for (std::size_t i = 0; i < std::min<std::size_t>(sorted.size(), 6); ++i) {
                std::fprintf(stderr, "[read-callers] %.1f MiB in %llu reads from %p (%s)\n", sorted[i].first / 1048576.0, static_cast<unsigned long long>(callers[sorted[i].second].second), sorted[i].second, DescribeCaller(sorted[i].second).c_str());
            }
            std::fflush(stderr);
            callers.clear();
        }
    }
    const auto* source = reinterpret_cast<const void*>(address);
    CheckRange(source, destination.size(), alignment);
    if (timing) timing->Mark("range_check");
    std::memcpy(destination.data(), source, destination.size());
    if (timing) timing->Mark("copy", destination.size());
}

void Write(std::uint64_t address, std::span<const std::byte> source, std::size_t alignment) {
    PerformanceTimer timing("GuestMemory.Write");
    if (source.empty()) return;
    auto* destination = reinterpret_cast<void*>(address);
    CheckRange(destination, source.size(), alignment, true);
    timing.Mark("range_check");
    std::memcpy(destination, source.data(), source.size());
    timing.Mark("copy", source.size());
}

}

extern "C" void AgcDriverCheckGuestMemory_nid_postfix(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    AgcDriver::GuestMemory::CheckRange(pointer, bytes, alignment, writable);
}

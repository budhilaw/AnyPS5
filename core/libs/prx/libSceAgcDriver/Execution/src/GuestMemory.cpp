#include <cstdio>
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <vector>
#include <cstdlib>
#include <dlfcn.h>
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
}

void CheckRange(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    require(alignment != 0, "zero guest memory alignment");
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    if (address == 0 || address % alignment != 0) {
        char message[128];
        std::snprintf(message, sizeof(message), "null or misaligned address 0x%llx (%zu bytes, alignment %zu)", static_cast<unsigned long long>(address), bytes, alignment);
        throw std::runtime_error(std::string("AGC driver: ") + message);
    }
    require(bytes <= std::numeric_limits<std::uintptr_t>::max() - address, "address range overflow");
    MemoryAccessScope::Resolve(address, bytes, writable);
    GuestMemoryTracking::GuestMemoryTrackingResolve_nid_postfix(address, bytes, writable);
    auto cursor = address;
    const auto end = address + bytes;
#ifdef _WIN32
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION memory{};
        require(VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == sizeof(memory), "cannot query guest memory");
        require(memory.State == MEM_COMMIT && (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0, "guest memory is not readable");
        const auto protection = memory.Protect & 0xffu;
        require(protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY, "guest memory has no read permission");
        const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        require(memory.RegionSize <= std::numeric_limits<std::uintptr_t>::max() - base && base + memory.RegionSize > cursor, "invalid guest memory mapping");
        require(!writable || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY, "guest memory has no write permission");
        cursor = std::min(end, base + memory.RegionSize);
    }
#elif defined(__APPLE__)
    // The last readable and writable region this thread verified stays valid until a guest
    // mapping changes (the epoch): shader memory is read a dword at a time, thousands per frame.
    struct VerifiedRegion { std::uint64_t epoch = 0; std::uintptr_t first = 0; std::uintptr_t end = 0; };
    thread_local VerifiedRegion verified;
    const auto epoch = GuestAllocations::GuestAllocationsMapEpoch_nid_postfix();
    if (verified.epoch == epoch && address >= verified.first && end <= verified.end) return;
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
        if ((info.protection & (VM_PROT_READ | VM_PROT_WRITE)) == (VM_PROT_READ | VM_PROT_WRITE) && first <= address && first + size >= end) {
            verified = {epoch, static_cast<std::uintptr_t>(first), static_cast<std::uintptr_t>(first + size)};
        }
        cursor = std::min(end, static_cast<std::uintptr_t>(first + size));
    }
#else
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

void Read(std::uint64_t address, std::span<std::byte> destination, std::size_t alignment) {
    PerformanceTimer timing("GuestMemory.Read");
    if (destination.empty()) return;
    // ANYPS5_TRACE_READ_CALLERS: bytes read per call site, the largest printed every 3 seconds.
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
                Dl_info info{};
                dladdr(sorted[i].second, &info);
                std::fprintf(stderr, "[read-callers] %.1f MiB in %llu reads from %p (%s+0x%lx)\n", sorted[i].first / 1048576.0, static_cast<unsigned long long>(callers[sorted[i].second].second), sorted[i].second, info.dli_sname ? info.dli_sname : "?", info.dli_saddr ? static_cast<unsigned long>(static_cast<const char*>(sorted[i].second) - static_cast<const char*>(info.dli_saddr)) : 0ul);
            }
            callers.clear();
        }
    }
    const auto* source = reinterpret_cast<const void*>(address);
    CheckRange(source, destination.size(), alignment);
    timing.Mark("range_check");
    std::memcpy(destination.data(), source, destination.size());
    timing.Mark("copy", destination.size());
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

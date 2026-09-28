#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <limits>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
#include "prx/libc/include/General.hpp"
#include <stdexcept>

namespace AgcDriver::GuestMemory {

void DescribeRegions(std::uint64_t address, std::size_t bytes) {
#if defined(__APPLE__)
    mach_vm_address_t cursor = address;
    const auto end = address + bytes;
    for (int count = 0; cursor < end && count < 64; ++count) {
        mach_vm_address_t start = cursor;
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t infoCount = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        if (mach_vm_region(mach_task_self(), &start, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &infoCount, &object) != KERN_SUCCESS) break;
        if (start > cursor) APS5_LOG_OUT("  unmapped 0x%llx+0x%llx", static_cast<unsigned long long>(cursor), static_cast<unsigned long long>(start - cursor));
        APS5_LOG_OUT("  region 0x%llx+0x%llx protection %u max %u", static_cast<unsigned long long>(start), static_cast<unsigned long long>(size), static_cast<unsigned>(info.protection), static_cast<unsigned>(info.max_protection));
        cursor = start + size;
    }
#else
    (void)address; (void)bytes;
#endif
}

std::size_t MappedGpuBytes(const void* pointer, std::size_t bytes, bool writable) {
    // Binary search over 16 KiB pages for the longest checkable prefix.
    constexpr std::size_t page = 16384;
    const auto ok = [&](std::size_t count) {
        try { CheckGpuRange(pointer, count, 1, writable); return true; } catch (const std::exception&) { return false; }
    };
    if (ok(bytes)) return bytes;
    std::size_t low = 0, high = bytes / page; // pages known good / first page count known bad
    while (high - low > 1) {
        const auto middle = low + (high - low) / 2;
        if (ok(middle * page)) low = middle; else high = middle;
    }
    return low * page;
}

void CheckGpuRange(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    if (alignment == 0 || address == 0 || address % alignment != 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - address) throw std::invalid_argument("invalid guest GPU memory range");
    const MemoryAccessScope suspended(nullptr, nullptr);
    GuestMemoryTracking::GuestMemoryTrackingValidate_nid_postfix(address, bytes, [writable](std::uint64_t first, std::size_t count) {
        CheckRange(reinterpret_cast<const void*>(first), count, 1, writable);
    });
}

}

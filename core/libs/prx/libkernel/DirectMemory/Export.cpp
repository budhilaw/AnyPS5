#include <cstdint>
#include <string>
#include <mutex>
#include <map>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
extern "C" Pthread APS5_VABI scePthreadSelf();
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "DirectMemory.hpp"

namespace {
std::mutex rangeNamesMutex;
std::map<std::uintptr_t, std::pair<std::uint64_t, std::string>> rangeNames;
std::string rangeName(std::uintptr_t start, std::uintptr_t end) {
    std::lock_guard lock(rangeNamesMutex);
    auto it = rangeNames.upper_bound(start);
    if (it == rangeNames.begin()) return {};
    --it;
    return it->first <= start && end <= it->first + it->second.first ? it->second.second : std::string{};
}
}

namespace {

constexpr int BatchMapDirect = 0;
constexpr int BatchUnmap = 1;
constexpr int BatchProtect = 2;
constexpr int BatchMapFlexible = 3;
constexpr int BatchTypeProtect = 4;
constexpr int BatchMapFixed = 0x10;

}

extern "C" {

int APS5_VABI sceKernelAllocateDirectMemory(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out) {
 if (search_start < 0 || search_end <= search_start || len == 0
  || (len & (PS5_PAGE_SIZE - 1)) || !phys_addr_out
  || (alignment != 0 && (alignment & (PS5_PAGE_SIZE - 1))))
  { const int result_ = SCE_KERNEL_ERROR_EINVAL; APS5_LOG_OUT("allocate direct len 0x%zx align 0x%zx -> phys 0x%llx result 0x%x", len, alignment, phys_addr_out ? static_cast<unsigned long long>(*phys_addr_out) : 0ull, static_cast<unsigned>(result_)); return result_; }
 { const int result_ = DirectMemoryAlloc(search_start, search_end, len, alignment, memory_type, phys_addr_out); APS5_LOG_OUT("allocate direct len 0x%zx align 0x%zx -> phys 0x%llx result 0x%x", len, alignment, phys_addr_out ? static_cast<unsigned long long>(*phys_addr_out) : 0ull, static_cast<unsigned>(result_)); return result_; }
}

int APS5_VABI sceKernelAllocateMainDirectMemory(size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out) {
 return sceKernelAllocateDirectMemory(0, static_cast<int64_t>(DIRECT_MEMORY_SIZE), len, alignment, memory_type, phys_addr_out);
}

int APS5_VABI sceKernelAvailableDirectMemorySize(int64_t search_start, int64_t search_end, size_t alignment, int64_t* phys_addr_out, size_t* size_out) {
 if (!phys_addr_out || !size_out) return SCE_KERNEL_ERROR_EINVAL;
 int64_t tmpPhys = 0;
 int ret = DirectMemoryAlloc(search_start, search_end, PS5_PAGE_SIZE, alignment, 0, &tmpPhys);
 if (ret != 0) { *phys_addr_out = 0; *size_out = 0; return ret; }
 DirectMemoryFree(tmpPhys, PS5_PAGE_SIZE);
 *phys_addr_out = tmpPhys;
 *size_out = static_cast<size_t>(search_end) - static_cast<size_t>(tmpPhys);
 return 0;
}

int APS5_VABI sceKernelDirectMemoryQuery(int64_t offset, int flags, void* info, size_t info_size) {
 if (!info || offset < 0) return SCE_KERNEL_ERROR_EINVAL;
 struct DirectMemoryQueryInfo { int64_t start; int64_t end; int memory_type; };
 if (info_size < sizeof(DirectMemoryQueryInfo)) return SCE_KERNEL_ERROR_EINVAL;
 auto* q = static_cast<DirectMemoryQueryInfo*>(info);
 if (!DirectMemoryQueryRange(offset, (flags & 1) != 0, &q->start, &q->end, &q->memory_type)) return SCE_KERNEL_ERROR_EACCES;
 return 0;
}

size_t APS5_VABI sceKernelGetDirectMemorySize(void) {
 return DIRECT_MEMORY_SIZE;
}

int APS5_VABI sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags, int64_t direct_memory_start, size_t alignment) {
    APS5_LOG_OUT("map direct %p+0x%zx prot 0x%x flags 0x%x phys 0x%llx", addr ? *addr : nullptr, len, prot, flags, static_cast<unsigned long long>(direct_memory_start));
 const int result = DoMapDirect(addr, len, prot, flags, direct_memory_start, alignment);
 APS5_LOG_OUT("map direct -> %p result 0x%x", addr ? *addr : nullptr, static_cast<unsigned>(result));
 return result;
}

int APS5_VABI sceKernelMapDirectMemory2(void** addr, size_t len, int type, int prot, int flags, int64_t direct_memory_start, size_t alignment) {
    APS5_LOG_OUT("map direct2 %p+0x%zx prot 0x%x flags 0x%x phys 0x%llx", addr ? *addr : nullptr, len, prot, flags, static_cast<unsigned long long>(direct_memory_start));
 (void)type;
 return DoMapDirect(addr, len, prot, flags, direct_memory_start, alignment);
}

int APS5_VABI sceKernelMapFlexibleMemory(void** addr_in_out, size_t len, int prot, int flags) {
    APS5_LOG_OUT("map flexible %p+0x%zx prot 0x%x flags 0x%x", addr_in_out ? *addr_in_out : nullptr, len, prot, flags);
 return DoMapAnon(addr_in_out, len, prot, flags);
}

int APS5_VABI sceKernelMapNamedDirectMemory(void** addr, size_t len, int prot, int flags, int64_t direct_memory_start, size_t alignment, const char* name) {
 (void)name;
 return DoMapDirect(addr, len, prot, flags, direct_memory_start, alignment);
}

int32_t APS5_VABI sceKernelMapNamedFlexibleMemory(void** addr_in_out, size_t len, int prot, int flags, const char* name) {
 (void)name;
 return DoMapAnon(addr_in_out, len, prot, flags);
}

int APS5_VABI sceKernelMprotect(const void* addr, size_t len, int prot) {
    APS5_LOG_OUT("mprotect %p+0x%zx prot 0x%x", addr, len, prot);
 return DoMprotect(addr, len, prot);
}

int APS5_VABI sceKernelMunmap(uint64_t vaddr, size_t len) {
    APS5_LOG_OUT("munmap 0x%llx+0x%zx", static_cast<unsigned long long>(vaddr), len);
 return DoMunmap(reinterpret_cast<void*>(vaddr), len);
}

int APS5_VABI sceKernelReleaseDirectMemory(int64_t start, size_t len) {
    APS5_LOG_OUT("release direct phys 0x%llx+0x%zx", static_cast<unsigned long long>(start), len);
 if (start < 0 || len == 0) return SCE_KERNEL_ERROR_EINVAL;
 DirectMemoryFree(start, len);
 return 0;
}

int APS5_VABI sceKernelReserveVirtualRange(void** addr, size_t len, int flags, size_t alignment) {
    APS5_LOG_OUT("reserve %p+0x%zx flags 0x%x", addr ? *addr : nullptr, len, flags);
 (void)flags;
 return DoReserveVirtual(addr, len, alignment);
}

int APS5_VABI sceKernelVirtualQuery(const void* addr, int flags, VirtualQueryInfo* info, uint64_t info_size) {
    if (!info || info_size < sizeof(VirtualQueryInfo)) return SCE_KERNEL_ERROR_EINVAL;
    if ((flags & ~1) != 0) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Range range{};
    {
        GuestAllocations::Mutation mutation;
        if (!mutation.Query(reinterpret_cast<std::uintptr_t>(addr), (flags & 1) != 0, range)) return SCE_KERNEL_ERROR_EACCES;
    }
    memset(info, 0, sizeof(VirtualQueryInfo));
    std::uintptr_t start = range.address;
    std::uintptr_t end = range.address + range.bytes;
    const std::uintptr_t probe = (flags & 1) != 0 ? start : std::max(start, reinterpret_cast<std::uintptr_t>(addr));
    std::uintptr_t mappingStart = 0;
    std::uintptr_t mappingEnd = 0;
    int64_t physical = 0;
    if (FindDirectMapping(probe, &mappingStart, &mappingEnd, &physical)) {
        start = std::max(start, mappingStart);
        end = std::min(end, mappingEnd);
        int64_t allocationStart = 0;
        int64_t allocationEnd = 0;
        int memoryType = 0;
        info->offset = static_cast<uint64_t>(physical) + (start - mappingStart);
        if (DirectMemoryQueryRange(static_cast<int64_t>(info->offset), false, &allocationStart, &allocationEnd, &memoryType)) info->memory_type = memoryType;
        info->is_direct = 1;
    } else {
        start = std::max(start, PreviousDirectMappingEnd(probe));
        end = std::min(end, NextDirectMapping(probe));
        info->is_flexible = 1;
    }
    info->start = start;
    info->end = end;
    info->protection = (range.readable ? 1 : 0) | (range.writable ? 2 : 0);
    info->is_committed = range.readable || range.writable;
    const auto name = rangeName(start, end);
    std::memcpy(info->name, name.c_str(), name.size());
    return 0;
}

// ---------------------------------------------------------------------------
// Moved as-is (not yet implemented) from the monolithic libkernel/Export.cpp.
// ---------------------------------------------------------------------------

int APS5_VABI sceKernelCheckedReleaseDirectMemory(int64_t start, size_t len) {
    APS5_LOG_OUT("checked release direct phys 0x%llx+0x%zx", static_cast<unsigned long long>(start), len);
    if (start < 0 || len == 0 || (start & (PS5_PAGE_SIZE - 1)) != 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    DirectMemoryFree(start, len);
    return 0;
}

int APS5_VABI sceKernelMtypeprotect(const void* addr, size_t len, int type, int prot) {
    APS5_LOG_OUT("mtypeprotect %p+0x%zx type %d prot 0x%x", addr, len, type, prot);
    return DoMprotect(addr, len, prot);
}

int APS5_VABI sceKernelQueryMemoryProtection(void* addr, void** start, void** end, int* prot) {
 (void)addr;
 (void)start;
 (void)end;
 (void)prot;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelIsStack(void* addr, void** start, void** end) {
    const auto self = scePthreadSelf();
    if (self == nullptr || self->stackAddress == nullptr || self->stackSize == 0) return 0;
    const auto low = reinterpret_cast<std::uintptr_t>(self->stackAddress);
    const auto high = low + self->stackSize;
    const auto value = reinterpret_cast<std::uintptr_t>(addr);
    if (value < low || value >= high) return 0;
    if (start != nullptr) *start = self->stackAddress;
    if (end != nullptr) *end = reinterpret_cast<void*>(high);
    return 1;
}

constexpr size_t FlexibleMemoryBudget = 448ull << 20;

int APS5_VABI sceKernelAvailableFlexibleMemorySize(size_t* size) {
 if (!size) return SCE_KERNEL_ERROR_EINVAL;
 *size = FlexibleMemoryBudget;
 return 0;
}

int APS5_VABI sceKernelConfiguredFlexibleMemorySize(size_t* size) {
 if (!size) return SCE_KERNEL_ERROR_EINVAL;
 *size = FlexibleMemoryBudget;
 return 0;
}

int APS5_VABI sceKernelSetVirtualRangeName(const void* addr, uint64_t len, const char* name) {
    if (addr == nullptr || name == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    if (len == 0 || (start & (PS5_PAGE_SIZE - 1)) != 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    const auto length = std::strlen(name);
    if (length >= 32) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(rangeNamesMutex);
    rangeNames[start] = {len, std::string(name, length)};
    return 0;
}

int APS5_VABI sceKernelGetPageTableStats(int* cpu_total, int* cpu_available, int* gpu_total, int* gpu_available) {
 (void)cpu_total;
 (void)cpu_available;
 (void)gpu_total;
 (void)gpu_available;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetPrtAperture(int index, void** addr, size_t* len) {
 (void)index;
 (void)addr;
 (void)len;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelSetPrtAperture(int index, void* addr, size_t len) {
 (void)index;
 (void)addr;
 (void)len;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelBatchMap2(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out, int flags) {
    if (entries == nullptr || num_entries < 0) return SCE_KERNEL_ERROR_EINVAL;
    int processed = 0;
    int result = 0;
    for (; processed < num_entries; ++processed) {
        const auto& entry = entries[processed];
        const int protection = static_cast<unsigned char>(entry.protection);
        APS5_LOG_OUT("batch map op %d %p+0x%llx phys 0x%llx prot 0x%x type %d", entry.operation, entry.start, static_cast<unsigned long long>(entry.length), static_cast<unsigned long long>(entry.offset), static_cast<unsigned>(protection), entry.type);
        void* address = entry.start;
        switch (entry.operation) {
        case BatchMapDirect: result = DoMapDirect(&address, entry.length, protection, flags, static_cast<int64_t>(entry.offset), 0); break;
        case BatchUnmap: result = DoMunmap(entry.start, entry.length); break;
        case BatchProtect:
        case BatchTypeProtect: result = DoMprotect(entry.start, entry.length, protection); break;
        case BatchMapFlexible: result = DoMapAnon(&address, entry.length, protection, flags); break;
        default: result = SCE_KERNEL_ERROR_EINVAL; break;
        }
        if (result != 0) break;
    }
    if (num_entries_out != nullptr) *num_entries_out = processed;
    return result;
}

int APS5_VABI sceKernelBatchMap(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out) {
    return sceKernelBatchMap2(entries, num_entries, num_entries_out, BatchMapFixed);
}

}

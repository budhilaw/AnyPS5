#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <utility>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();

// sceLibcMspace*: allocators over caller-provided memory ranges. Titles build their general heap on
// one (Hades: a 4 GiB "User Malloc" space), so every operation has to stay logarithmic. The
// bookkeeping lives in host memory; the guest range only holds the allocations themselves and the
// handle is the range's base address.
namespace {

constexpr std::size_t Granule = 16;

struct Allocation {
    std::uintptr_t start;     // the block the allocation occupies (alignment padding included)
    std::size_t capacity;     // block bytes
    std::size_t requested;    // bytes the caller asked for (rounded to the granule)
};

struct Arena {
    std::uintptr_t start;
    std::uintptr_t end;
    std::map<std::uintptr_t, std::size_t> freeByAddress;                 // block start -> bytes
    std::set<std::pair<std::size_t, std::uintptr_t>> freeBySize;         // (bytes, start)
    std::unordered_map<std::uintptr_t, Allocation> allocations;          // user pointer -> block

    void AddFree(std::uintptr_t address, std::size_t bytes) {
        if (bytes == 0) return;
        // Coalesce with the neighbours on both sides.
        auto next = freeByAddress.lower_bound(address);
        if (next != freeByAddress.end() && address + bytes == next->first) {
            freeBySize.erase({next->second, next->first});
            bytes += next->second;
            next = freeByAddress.erase(next);
        }
        if (next != freeByAddress.begin()) {
            auto previous = std::prev(next);
            if (previous->first + previous->second == address) {
                freeBySize.erase({previous->second, previous->first});
                address = previous->first;
                bytes += previous->second;
                freeByAddress.erase(previous);
            }
        }
        freeByAddress.emplace(address, bytes);
        freeBySize.emplace(bytes, address);
    }

    void RemoveFree(std::uintptr_t address, std::size_t bytes) {
        freeByAddress.erase(address);
        freeBySize.erase({bytes, address});
    }
};

std::mutex arenaMutex;
std::map<std::uintptr_t, std::unique_ptr<Arena>> arenas; // keyed by start

void Error(int value) { *__error_nid_postfix() = value; }

std::size_t RoundUp(std::size_t value) { return (value + Granule - 1) & ~(Granule - 1); }

Arena* Find(void* handle) {
    const auto it = arenas.find(reinterpret_cast<std::uintptr_t>(handle));
    if (it == arenas.end()) { Error(22); return nullptr; }
    return it->second.get();
}

// The innermost space holding the pointer as an allocation (spaces may nest).
Arena* Owner(const void* pointer) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    auto it = arenas.upper_bound(address);
    while (it != arenas.begin()) {
        --it;
        auto* arena = it->second.get();
        if (address < arena->end && arena->allocations.count(address) != 0) return arena;
    }
    return nullptr;
}

void* Allocate(Arena* arena, std::size_t size, std::size_t alignment) {
    if (!arena) return nullptr;
    if (size > std::numeric_limits<std::size_t>::max() - 2 * Granule) { Error(12); return nullptr; }
    size = RoundUp(std::max<std::size_t>(size, 1));
    alignment = std::max(alignment, Granule);
    // Best fit: the smallest free block that holds the size; larger alignments may need a larger one.
    for (auto it = arena->freeBySize.lower_bound({size, 0}); it != arena->freeBySize.end(); ++it) {
        const auto [bytes, start] = *it;
        const auto aligned = (start + alignment - 1) & ~(alignment - 1);
        const auto padding = aligned - start;
        if (padding > bytes || size > bytes - padding) continue;
        arena->RemoveFree(start, bytes);
        // Padding of a granule or more goes back to the free lists; so does the tail.
        std::uintptr_t blockStart = start;
        std::size_t blockBytes = bytes;
        if (padding >= Granule) {
            arena->AddFree(start, padding);
            blockStart = aligned;
            blockBytes -= padding;
        }
        const auto used = (aligned - blockStart) + size;
        if (blockBytes - used >= Granule) {
            arena->AddFree(blockStart + used, blockBytes - used);
            blockBytes = used;
        }
        arena->allocations.emplace(aligned, Allocation{blockStart, blockBytes, size});
        return reinterpret_cast<void*>(aligned);
    }
    {
        std::size_t freeBytes = 0, largest = 0;
        for (const auto& [bytes, start] : arena->freeBySize) { freeBytes += bytes; largest = std::max(largest, bytes); }
        static int reported = 0;
        if (reported++ < 16) APS5_LOG_OUT("mspace 0x%llx: cannot allocate %zu bytes aligned %zu (free %zu bytes in %zu blocks, largest %zu; %zu allocations)", static_cast<unsigned long long>(arena->start), size, alignment, freeBytes, arena->freeBySize.size(), largest, arena->allocations.size());
    }
    Error(12);
    return nullptr;
}

void Release(Arena* arena, std::unordered_map<std::uintptr_t, Allocation>::iterator allocation) {
    const auto block = allocation->second;
    arena->allocations.erase(allocation);
    arena->AddFree(block.start, block.capacity);
}

// Grows or shrinks in place when the block (and a free block right after it) allows it.
bool ResizeInPlace(Arena* arena, Allocation& block, std::uintptr_t pointer, std::size_t size) {
    size = RoundUp(std::max<std::size_t>(size, 1));
    const auto needed = (pointer - block.start) + size;
    if (needed <= block.capacity) {
        if (block.capacity - needed >= Granule) {
            arena->AddFree(block.start + needed, block.capacity - needed);
            block.capacity = needed;
        }
        block.requested = size;
        return true;
    }
    const auto next = arena->freeByAddress.find(block.start + block.capacity);
    if (next == arena->freeByAddress.end() || block.capacity + next->second < needed) return false;
    const auto nextBytes = next->second;
    arena->RemoveFree(next->first, nextBytes);
    const auto total = block.capacity + nextBytes;
    if (total - needed >= Granule) {
        block.capacity = needed;
        arena->AddFree(block.start + needed, total - needed);
    } else {
        block.capacity = total;
    }
    block.requested = size;
    return true;
}

void* Reallocate(Arena* arena, void* pointer, std::size_t alignment, std::size_t size) {
    if (!arena) return nullptr;
    if (!pointer) return Allocate(arena, size, alignment);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto found = arena->allocations.find(address);
    if (found == arena->allocations.end()) { Error(22); return nullptr; }
    if (size == 0) { Release(arena, found); return nullptr; }
    if ((address & (std::max(alignment, Granule) - 1)) == 0 && ResizeInPlace(arena, found->second, address, size)) return pointer;
    const auto copied = std::min(found->second.requested, size);
    void* result = Allocate(arena, size, alignment);
    if (!result) return nullptr;
    std::memcpy(result, pointer, copied);
    Release(arena, arena->allocations.find(address));
    return result;
}

bool ValidAlignment(std::size_t alignment) { return alignment != 0 && (alignment & (alignment - 1)) == 0; }

}

extern "C" {

void* APS5_VABI sceLibcMspaceCreate_nid_postfix(const char* name, void* base, std::size_t size, unsigned flags) {
    // Flag bits (thread safety, debug checks, statistics) change bookkeeping only; every arena here
    // is locked and unchecked.
    const auto start = reinterpret_cast<std::uintptr_t>(base);
#if defined(__APPLE__)
    {
        // Diagnostic: how much of the range is mapped (titles may reserve more than they commit).
        mach_vm_address_t address = start;
        std::size_t mapped = 0;
        while (address < start + size) {
            mach_vm_size_t regionSize = 0;
            vm_region_basic_info_data_64_t info{};
            mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
            mach_port_t object = MACH_PORT_NULL;
            mach_vm_address_t regionAddress = address;
            if (mach_vm_region(mach_task_self(), &regionAddress, &regionSize, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS) break;
            if (regionAddress >= start + size) break;
            APS5_LOG_OUT("  region 0x%llx+0x%llx prot %d/%d", static_cast<unsigned long long>(regionAddress), static_cast<unsigned long long>(regionSize), info.protection, info.max_protection);
            if (info.protection != 0) mapped += regionSize;
            address = regionAddress + regionSize;
        }
        APS5_LOG_OUT("mspace '%s' base %p size 0x%zx flags 0x%x: 0x%zx bytes mapped", name ? name : "", base, size, flags, mapped);
    }
#endif
    if (!base || (start & (Granule - 1)) || size < 2 * Granule || size > std::numeric_limits<std::uintptr_t>::max() - start || (flags & ~0xfu) != 0) {
        Error(22);
        return nullptr;
    }
    std::lock_guard lock(arenaMutex);
    const auto end = start + size;
    // Titles nest spaces: a space's memory may come from an allocation of another one (Hades'
    // ActivityManagerMspace lives in its "User Malloc" space). Only an identical base is refused.
    if (arenas.count(start) != 0) {
        Error(22);
        return nullptr;
    }
    auto arena = std::make_unique<Arena>();
    arena->start = start;
    arena->end = end;
    arena->AddFree(start, size & ~(Granule - 1));
    arenas.emplace(start, std::move(arena));
    return base;
}

int APS5_VABI sceLibcMspaceDestroy_nid_postfix(void* handle) {
    std::lock_guard lock(arenaMutex);
    if (arenas.erase(reinterpret_cast<std::uintptr_t>(handle)) == 0) {
        Error(22);
        return -1;
    }
    return 0;
}

void* APS5_VABI sceLibcMspaceMalloc_nid_postfix(void* handle, std::size_t size) {
    std::lock_guard lock(arenaMutex);
    return Allocate(Find(handle), size, Granule);
}

void APS5_VABI sceLibcMspaceFree_nid_postfix(void* handle, void* pointer) {
    if (!pointer) return;
    std::lock_guard lock(arenaMutex);
    auto* arena = Find(handle);
    if (!arena) return;
    const auto found = arena->allocations.find(reinterpret_cast<std::uintptr_t>(pointer));
    if (found == arena->allocations.end()) {
        static int reported = 0;
        if (reported++ < 16) APS5_LOG_OUT("mspace 0x%llx: free of unknown pointer %p", static_cast<unsigned long long>(arena->start), pointer);
        Error(22);
        return;
    }
    Release(arena, found);
}

void* APS5_VABI sceLibcMspaceCalloc_nid_postfix(void* handle, std::size_t count, std::size_t size) {
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) { Error(12); return nullptr; }
    std::lock_guard lock(arenaMutex);
    void* result = Allocate(Find(handle), count * size, Granule);
    if (result) std::memset(result, 0, count * size);
    return result;
}

void* APS5_VABI sceLibcMspaceRealloc_nid_postfix(void* handle, void* pointer, std::size_t size) {
    std::lock_guard lock(arenaMutex);
    return Reallocate(Find(handle), pointer, Granule, size);
}

int APS5_VABI sceLibcMspacePosixMemalign_nid_postfix(void* handle, void** result, std::size_t alignment, std::size_t size) {
    if (!result || alignment < sizeof(void*) || !ValidAlignment(alignment)) return 22;
    std::lock_guard lock(arenaMutex);
    auto* arena = Find(handle);
    if (!arena) return 22;
    void* pointer = Allocate(arena, size, alignment);
    if (!pointer) return 12;
    *result = pointer;
    return 0;
}

void* APS5_VABI sceLibcMspaceMemalign_nid_postfix(void* handle, std::size_t alignment, std::size_t size) {
    if (!ValidAlignment(alignment)) { Error(22); return nullptr; }
    std::lock_guard lock(arenaMutex);
    return Allocate(Find(handle), size, alignment);
}

void* APS5_VABI sceLibcMspaceReallocalign_nid_postfix(void* handle, void* pointer, std::size_t alignment, std::size_t size) {
    if (!ValidAlignment(alignment)) { Error(22); return nullptr; }
    std::lock_guard lock(arenaMutex);
    return Reallocate(Find(handle), pointer, alignment, size);
}

std::size_t APS5_VABI sceLibcMspaceMallocUsableSize_nid_postfix(const void* pointer) {
    if (!pointer) return 0;
    std::lock_guard lock(arenaMutex);
    if (auto* arena = Owner(pointer)) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        const auto found = arena->allocations.find(address);
        if (found != arena->allocations.end()) return found->second.start + found->second.capacity - address;
    }
    Error(22);
    return 0;
}

}

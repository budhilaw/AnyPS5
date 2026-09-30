#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/MemoryBackingPlatform.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include "prx/libc/include/SlowOperation.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace GuestMemoryBacking {
namespace {

struct Allocation {
    Platform::Mapping mapping;
    std::map<std::uint64_t, std::uint64_t> ranges;
    std::uint64_t serial = 0;
    std::size_t imports = 0;
    std::chrono::steady_clock::time_point released{};
};

std::uint64_t nextSerial = 1;
std::atomic<std::uint64_t> unmapGeneration{0};

struct RetiredAlias {
    Platform::Mapping mapping;
    std::size_t imports;
    std::chrono::steady_clock::time_point released{};
};

std::vector<RetiredAlias>& retired() {
    static auto* value = new std::vector<RetiredAlias>;
    return *value;
}

void collectRetired() {
    const auto now = std::chrono::steady_clock::now();
    auto& list = retired();
    for (auto it = list.begin(); it != list.end();) {
        if (it->imports != 0 || now - it->released < std::chrono::seconds(2)) {
            ++it;
            continue;
        }
        Platform::UnmapAlias(it->mapping);
        it = list.erase(it);
    }
}

std::map<std::uint64_t, Allocation>& allocations() {
    static auto* value = new std::map<std::uint64_t, Allocation>;
    return *value;
}

Allocation* lookup(std::uint64_t address, std::size_t bytes) {
    if (address == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return nullptr;
    auto found = allocations().upper_bound(address);
    if (found == allocations().begin()) return nullptr;
    auto& allocation = std::prev(found)->second;
    auto range = allocation.ranges.upper_bound(address);
    if (range == allocation.ranges.begin() || address + bytes > std::prev(range)->second) return nullptr;
    return &allocation;
}

Allocation& find(std::uint64_t address, std::size_t bytes) {
    if (address == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) throw std::invalid_argument("invalid guest backing range");
    auto* allocation = lookup(address, bytes);
    if (allocation == nullptr) throw std::runtime_error("guest memory backing range is unmapped");
    return *allocation;
}

}

namespace {

void validateMapping(void* address, std::size_t bytes, std::size_t alignment, int protection) {
    const auto pageSize = GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix();
    if (bytes == 0 || bytes % pageSize != 0 || alignment < pageSize || (alignment & (alignment - 1)) != 0 || (protection & ~7) != 0) throw std::invalid_argument("invalid shared guest memory mapping");
    if (address != nullptr && reinterpret_cast<std::uintptr_t>(address) % alignment != 0) throw std::invalid_argument("misaligned fixed guest memory mapping");
}

void* registerMapping(const Platform::Mapping& mapping, std::size_t bytes) {
    try {
        if (mapping.bytes > std::numeric_limits<std::uint64_t>::max() - mapping.address) throw std::overflow_error("guest backing mapping overflow");
        const auto next = allocations().lower_bound(mapping.address);
        if (next != allocations().end() && next->first < mapping.address + bytes) throw std::runtime_error("overlapping guest backing mappings");
        if (next != allocations().begin()) {
            const auto& previous = std::prev(next)->second.mapping;
            if (previous.address + previous.bytes > mapping.address) throw std::runtime_error("guest mapping overlaps a retained backing reservation");
        }
        Allocation allocation{mapping, {{mapping.address, mapping.address + bytes}}, nextSerial++};
        if (!allocations().emplace(mapping.address, std::move(allocation)).second) throw std::runtime_error("duplicate guest backing mapping");
    } catch (...) {
        Platform::Unmap(mapping);
        throw;
    }
    return reinterpret_cast<void*>(mapping.address);
}

void unmapPhysical(Allocation& allocation, std::uint64_t address, std::size_t bytes) {
    const auto original = allocation;
    const auto base = original.mapping.address;
    const auto end = base + original.mapping.bytes;
    Platform::UnmapViewRange(address, bytes);
    allocations().erase(base);
    if (base < address) {
        auto left = original;
        left.mapping.bytes = static_cast<std::size_t>(address - base);
        left.ranges = {{base, address}};
        allocations().emplace(base, std::move(left));
    }
    if (address + bytes < end) {
        const auto start = address + bytes;
        Platform::Mapping mapping = original.mapping;
        mapping.address = start;
        mapping.bytes = static_cast<std::size_t>(end - start);
        mapping.alias = static_cast<std::byte*>(original.mapping.alias) + (start - base);
        allocations().emplace(start, Allocation{mapping, {{start, end}}, nextSerial++});
    }
    unmapGeneration.fetch_add(1, std::memory_order_release);
}

}

void* GuestMemoryBackingMap_nid_postfix(void* address, std::size_t bytes, std::size_t alignment, int protection) {
    validateMapping(address, bytes, alignment, protection);
    SlowOperationTimer timer("backing map");
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    timer.Split("backing lock wait");
    collectRetired();
    return registerMapping(Platform::Map(address, bytes, alignment, protection), bytes);
}

void* GuestMemoryBackingMapPhysical_nid_postfix(void* address, std::size_t bytes, std::size_t alignment, int protection, std::uint64_t physicalOffset) {
    validateMapping(address, bytes, alignment, protection);
    SlowOperationTimer timer("backing map physical");
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    timer.Split("backing lock wait");
    collectRetired();
    return registerMapping(Platform::MapPhysical(address, bytes, alignment, protection, physicalOffset), bytes);
}

void GuestMemoryBackingUnmap_nid_postfix(void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto pageSize = GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix();
    if (address % pageSize != 0 || bytes % pageSize != 0) throw std::invalid_argument("misaligned guest backing unmap");
    SlowOperationTimer timer("backing unmap");
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    timer.Split("backing lock wait");
    auto& allocation = find(address, bytes);
    GuestMemoryTracking::GuestMemoryTrackingInvalidate_nid_postfix(address, bytes);
    if (allocation.mapping.physical) {
        unmapPhysical(allocation, address, bytes);
        return;
    }
    auto replacement = allocation.ranges;
    const auto found = std::prev(replacement.upper_bound(address));
    const auto first = found->first;
    const auto last = found->second;
    replacement.erase(found);
    if (first < address) replacement.emplace(first, address);
    if (address + bytes < last) replacement.emplace(address + bytes, last);
    if (replacement.empty()) {
        const auto base = allocation.mapping.address;
        if (allocation.imports != 0 || std::chrono::steady_clock::now() - allocation.released < std::chrono::seconds(2)) {
            Platform::UnmapView(allocation.mapping);
            retired().push_back({allocation.mapping, allocation.imports, allocation.released});
        } else {
            Platform::Unmap(allocation.mapping);
        }
        allocations().erase(base);
        unmapGeneration.fetch_add(1, std::memory_order_release);
    } else {
        Platform::Deactivate(address, bytes);
        allocation.ranges.swap(replacement);
    }
}

void GuestMemoryBackingActivate_nid_postfix(std::uint64_t address, std::size_t bytes, int protection) {
    const auto pageSize = GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix();
    if (address % pageSize != 0 || bytes % pageSize != 0 || (protection & ~7) != 0) throw std::invalid_argument("misaligned guest backing activation");
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    static_cast<void>(find(address, bytes));
    Platform::Protect(address, bytes, protection);
}

void GuestMemoryBackingRequire_nid_postfix(std::uint64_t address, std::size_t bytes) {
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    static_cast<void>(find(address, bytes));
}

void* GuestMemoryBackingAlias_nid_postfix(std::uint64_t address, std::size_t bytes) {
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    const auto* allocation = lookup(address, bytes);
    if (allocation == nullptr) return nullptr;
    return static_cast<std::byte*>(allocation->mapping.alias) + (address - allocation->mapping.address);
}

bool GuestMemoryBackingExtent_nid_postfix(std::uint64_t address, std::size_t bytes, GuestMemoryBackingExtentInfo* info) {
    if (info == nullptr) return false;
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    const auto* allocation = lookup(address, bytes);
    if (allocation == nullptr) return false;
    info->address = allocation->mapping.address;
    info->bytes = allocation->mapping.bytes;
    info->alias = allocation->mapping.alias;
    info->serial = allocation->serial;
    return true;
}

void* GuestMemoryBackingRetainAlias_nid_postfix(std::uint64_t address, std::uint64_t serial) {
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    const auto found = allocations().find(address);
    if (found == allocations().end() || found->second.serial != serial) return nullptr;
    ++found->second.imports;
    return found->second.mapping.alias;
}

void GuestMemoryBackingReleaseAlias_nid_postfix(void* alias) {
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    bool found = false;
    for (auto& [base, allocation] : allocations()) {
        if (allocation.mapping.alias != alias || allocation.imports == 0) continue;
        if (--allocation.imports == 0) allocation.released = std::chrono::steady_clock::now();
        found = true;
        break;
    }
    for (auto& entry : retired()) {
        if (found || entry.mapping.alias != alias || entry.imports == 0) continue;
        if (--entry.imports == 0) entry.released = std::chrono::steady_clock::now();
        found = true;
    }
    collectRetired();
}

std::uint64_t GuestMemoryBackingUnmapGeneration_nid_postfix() {
    return unmapGeneration.load(std::memory_order_acquire);
}

void GuestMemoryBackingWrite_nid_postfix(std::uint64_t address, const void* source, std::size_t bytes) {
    if (source == nullptr) throw std::invalid_argument("missing guest backing write source");
    std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    auto& allocation = find(address, bytes);
    auto* destination = static_cast<std::byte*>(allocation.mapping.alias) + (address - allocation.mapping.address);
    std::memcpy(destination, source, bytes);
}

}

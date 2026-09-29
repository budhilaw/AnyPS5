#include <atomic>
#include <cstdio>
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <limits>
#include <iterator>
#include <map>
#include <vector>
#include <stdexcept>
#if defined(__APPLE__)
#include <cstring>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace GuestAllocations {
namespace {

struct Registry {
    std::recursive_mutex& mutex = GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix();
    std::map<std::uint64_t, std::shared_ptr<const Range>> ranges;
    bool mainImageRegistered = false;
    std::uint32_t registeredImageCount = 0;
};

Registry& registry() {
    static Registry value;
    return value;
}

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

}

std::atomic<std::uint64_t>& guestMapEpoch();

void* GuestAllocationsBegin_nid_postfix() {
    return new std::unique_lock<std::recursive_mutex>(registry().mutex);
}

void GuestAllocationsEnd_nid_postfix(void* mutation) noexcept {
    delete static_cast<std::unique_lock<std::recursive_mutex>*>(mutation);
}

#ifdef _WIN32
void GuestAllocationsRegisterMainImage_nid_postfix(void*) {
    auto& state = registry();
    if (state.mainImageRegistered) return;
    const auto image = GetModuleHandleW(nullptr);
    require(image != nullptr, "cannot locate the main guest image");
    auto replacement = state.ranges;
    auto cursor = reinterpret_cast<std::uintptr_t>(image);
    bool registered = false;
    for (;;) {
        MEMORY_BASIC_INFORMATION memory{};
        require(VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == sizeof(memory), "cannot query the main guest image");
        if (memory.AllocationBase != image) break;
        require(memory.Type == MEM_IMAGE && (memory.State == MEM_COMMIT || memory.State == MEM_RESERVE), "unsupported guest image mapping");
        require(reinterpret_cast<std::uintptr_t>(memory.BaseAddress) == cursor && memory.RegionSize != 0 && memory.RegionSize <= std::numeric_limits<std::uintptr_t>::max() - cursor, "invalid guest image range");
        if (memory.State == MEM_COMMIT) {
            require((memory.Protect & PAGE_GUARD) == 0, "guarded guest image pages are not supported");
            const auto protection = memory.Protect & 0xffu;
            const bool writable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
            const bool readable = writable || protection == PAGE_READONLY || protection == PAGE_EXECUTE_READ;
            require(readable || protection == PAGE_NOACCESS || protection == PAGE_EXECUTE, "unsupported guest image protection");
            const auto next = replacement.lower_bound(cursor);
            require(next == replacement.end() || cursor + memory.RegionSize <= next->first, "guest image overlaps a registered allocation");
            if (next != replacement.begin()) {
                const auto& previous = *std::prev(next)->second;
                require(previous.address + previous.bytes <= cursor, "guest image overlaps a registered allocation");
            }
            replacement.emplace(cursor, std::make_shared<const Range>(Range{cursor, memory.RegionSize, readable, writable, cursor, memory.RegionSize, false}));
            registered = true;
        }
        cursor += memory.RegionSize;
    }
    require(registered, "main guest image has no committed pages");
    state.ranges.swap(replacement);
    state.mainImageRegistered = true;
}
#elif defined(__APPLE__)
void GuestAllocationsRegisterMainImage_nid_postfix(void*) {
    auto& state = registry();
    const auto count = _dyld_image_count();
    if (state.registeredImageCount == count) return;
    auto replacement = state.ranges;
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(index));
        if (header == nullptr || header->magic != MH_MAGIC_64) continue;
        const auto slide = _dyld_get_image_vmaddr_slide(index);
        std::vector<const segment_command_64*> segments;
        bool guest = false;
        const auto* command = reinterpret_cast<const load_command*>(header + 1);
        for (std::uint32_t i = 0; i < header->ncmds; ++i) {
            if (command->cmd == LC_SEGMENT_64) {
                const auto* segment = reinterpret_cast<const segment_command_64*>(command);
                if (std::strcmp(segment->segname, "__ANYPS5") == 0) guest = true;
                if (std::strncmp(segment->segname, "__GUEST", 7) == 0 || std::strcmp(segment->segname, "__ANYPS5") == 0) segments.push_back(segment);
            }
            command = reinterpret_cast<const load_command*>(reinterpret_cast<const std::byte*>(command) + command->cmdsize);
        }
        if (!guest) continue;
        for (const auto* segment : segments) {
            const auto address = static_cast<std::uint64_t>(segment->vmaddr) + static_cast<std::uint64_t>(slide);
            const auto bytes = static_cast<std::size_t>(segment->vmsize);
            if (bytes == 0) continue;
            const auto next = replacement.lower_bound(address);
            if (next != replacement.end() && next->first < address + bytes) continue;
            if (next != replacement.begin()) {
                const auto& previous = *std::prev(next)->second;
                if (previous.address + previous.bytes > address) continue;
            }
            const bool writable = (segment->initprot & VM_PROT_WRITE) != 0;
            const bool readable = writable || (segment->initprot & VM_PROT_READ) != 0;
            replacement.emplace(address, std::make_shared<const Range>(Range{address, bytes, readable, writable, address, bytes, false}));
        }
    }
    state.ranges.swap(replacement);
    state.registeredImageCount = count;
}
#endif

void GuestAllocationsAdd_nid_postfix(void*, void* pointer, std::size_t bytes, bool readable, bool writable) {
    guestMapEpoch().fetch_add(1, std::memory_order_acq_rel);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest allocation range");
    require(!writable || readable, "writable guest allocation must be readable");
    auto& ranges = registry().ranges;
    const auto next = ranges.lower_bound(address);
    require(next == ranges.end() || (next->first != address && address + bytes <= next->first), "overlapping guest allocation");
    if (next != ranges.begin()) {
        const auto& previous = *std::prev(next)->second;
        require(previous.address + previous.bytes <= address, "overlapping guest allocation");
    }
    ranges.emplace(address, std::make_shared<const Range>(Range{address, bytes, readable, writable, address, bytes}));
}

void GuestAllocationsRequireUnpinned_nid_postfix(void*, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(bytes <= std::numeric_limits<std::uint64_t>::max() - address, "guest allocation range overflow");
    GuestMemoryTracking::GuestMemoryTrackingInvalidate_nid_postfix(address, bytes);
    const auto end = address + bytes;
    for (const auto& [base, range] : registry().ranges) {
        if (base >= end && base != address) break;
        if ((address < base + range->bytes && base < end) || base == address) require(range.use_count() == 1, "guest allocation is owned by an active GPU command");
    }
}

void GuestAllocationsRequireAvailable_nid_postfix(void*, const void* pointer, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    require(address != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid fixed guest mapping");
    for (const auto& [base, range] : registry().ranges) {
        if (base >= address + bytes) break;
        require(base + range->bytes <= address, "fixed mapping overlaps a registered guest allocation");
    }
}

bool GuestAllocationsIsReserved_nid_postfix(void*, const void* pointer, std::size_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    if (address == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    const auto end = address + bytes;
    auto cursor = address;
    for (const auto& [base, range] : registry().ranges) {
        if (base + range->bytes <= cursor) continue;
        if (base > cursor) return false;
        if (range->readable || range->writable || !range->releasable) return false;
        cursor = std::min(end, base + range->bytes);
        if (cursor == end) return true;
    }
    return false;
}

bool GuestAllocationsQuery_nid_postfix(void*, std::uint64_t address, bool findNext, Range* result) {
    if (result == nullptr) throw std::invalid_argument("guest allocation query without a result");
    for (const auto& [base, range] : registry().ranges) {
        if (base + range->bytes <= address) continue;
        if (base > address && !findNext) return false;
        *result = *range;
        return true;
    }
    return false;
}

Range GuestAllocationsFind_nid_postfix(void*, const void* pointer) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    for (const auto& [base, range] : registry().ranges) {
        if (range->allocationAddress == address) {
            require(range->releasable, "guest image memory is not a releasable allocation");
            return {address, range->allocationBytes, range->readable, range->writable, address, range->allocationBytes, range->releasable};
        }
    }
    throw std::runtime_error("guest allocation is not registered");
}

void GuestAllocationsRemove_nid_postfix(void* mutation, const void* pointer) {
    guestMapEpoch().fetch_add(1, std::memory_order_acq_rel);
    const auto range = GuestAllocationsFind_nid_postfix(mutation, pointer);
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, range.bytes);
    std::erase_if(registry().ranges, [&](const auto& entry) { return entry.second->allocationAddress == range.address; });
}

namespace {

std::map<std::uint64_t, std::shared_ptr<const Range>> replaceRange(const void* pointer, std::size_t bytes, bool remove, bool readable, bool writable) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest protection or unmap range");
    require(!writable || readable, "writable guest allocation must be readable");
    const auto end = address + bytes;
    auto replacement = registry().ranges;
    auto cursor = address;
    for (const auto& [base, entry] : registry().ranges) {
        const auto& range = *entry;
        const auto finish = base + range.bytes;
        if (finish <= address) continue;
        if (base >= end) break;
        if (base > cursor) {
            char message[160];
            std::snprintf(message, sizeof(message), "guest protection or unmap range has a hole: 0x%llx+0x%llx, unregistered 0x%llx..0x%llx (next range 0x%llx+0x%llx)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), static_cast<unsigned long long>(cursor), static_cast<unsigned long long>(base), static_cast<unsigned long long>(base), static_cast<unsigned long long>(range.bytes));
            throw std::runtime_error(message);
        }
        replacement.erase(base);
        const auto insert = [&](std::uint64_t first, std::uint64_t last, bool canRead, bool canWrite) {
            if (first < last) replacement.emplace(first, std::make_shared<const Range>(Range{first, static_cast<std::size_t>(last - first), canRead, canWrite, range.allocationAddress, range.allocationBytes, range.releasable}));
        };
        insert(base, std::max(base, address), range.readable, range.writable);
        if (!remove) insert(std::max(base, address), std::min(finish, end), readable, writable);
        insert(std::min(finish, end), finish, range.readable, range.writable);
        cursor = std::min(finish, end);
    }
    if (cursor != end) {
        char message[128];
        std::snprintf(message, sizeof(message), "guest protection or unmap range is not registered: 0x%llx+0x%llx (registered up to 0x%llx)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), static_cast<unsigned long long>(cursor));
        throw std::runtime_error(message);
    }
    return replacement;
}

}

namespace {
std::atomic<std::uint64_t> protectionGeneration{1};
}

std::atomic<std::uint64_t>& guestMapEpoch() {
    static std::atomic<std::uint64_t> epoch{1};
    return epoch;
}

std::uint64_t GuestAllocationsMapEpoch_nid_postfix() {
    return guestMapEpoch().load(std::memory_order_acquire);
}

std::uint64_t GuestAllocationsProtectionGeneration_nid_postfix() {
    return protectionGeneration.load(std::memory_order_acquire);
}

void GuestAllocationsProtect_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, bool readable, bool writable, const std::function<void()>& apply) {
    guestMapEpoch().fetch_add(1, std::memory_order_acq_rel);
    protectionGeneration.fetch_add(1, std::memory_order_acq_rel);
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, bytes);
    auto replacement = replaceRange(pointer, bytes, false, readable, writable);
    apply();
    registry().ranges.swap(replacement);
}

void GuestAllocationsUnmap_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, const std::function<void(const void*, bool)>& apply) {
    guestMapEpoch().fetch_add(1, std::memory_order_acq_rel);
    GuestAllocationsRequireUnpinned_nid_postfix(mutation, pointer, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto found = registry().ranges.upper_bound(address);
    require(found != registry().ranges.begin(), "unmap address is not registered");
    const auto& range = *std::prev(found)->second;
    require(range.releasable, "guest image memory cannot be unmapped");
    require(address >= range.address && address - range.allocationAddress <= range.allocationBytes && bytes <= range.allocationBytes - (address - range.allocationAddress), "unmap crosses allocation boundaries");
    auto replacement = replaceRange(pointer, bytes, true, false, false);
    bool last = true;
    for (const auto& [base, entry] : replacement) {
        if (entry->allocationAddress == range.allocationAddress) last = false;
    }
    apply(reinterpret_cast<const void*>(range.allocationAddress), last);
    registry().ranges.swap(replacement);
}

Lease GuestAllocationsAcquire_nid_postfix() {
    std::lock_guard lock(registry().mutex);
    Lease result;
    for (const auto& [address, range] : registry().ranges) {
        if (range->readable && range->bytes != 0) result.push_back(range);
    }
    return result;
}

}

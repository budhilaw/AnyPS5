#include <cstdio>
#if !defined(_WIN32)
#include <execinfo.h>
#endif
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include "prx/libc/include/MemoryTrackingPlatform.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/SlowOperation.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <limits>
#include <map>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <vector>

namespace GuestMemoryTracking {
namespace {

struct Entry {
    std::uint64_t address;
    std::size_t bytes;
    void* context;
    Resolver resolver;
    Protection protection = Protection::ReadWrite;
    std::vector<Platform::Region> original;
    bool resolving = false;
    bool active = false;
};

constexpr unsigned PageBlockShift = 30;
constexpr std::uint64_t PageStateLimit = std::uint64_t{1} << 48;

struct Registry {
    std::recursive_mutex mutex;
    std::map<std::uint64_t, std::shared_ptr<Entry>> entries;
    bool installed = false;
    std::array<std::atomic<std::atomic<std::uint8_t>*>, (PageStateLimit >> PageBlockShift)> pageStates{};
};

Registry& registry() {
    static auto* value = new Registry;
    return *value;
}

std::uint8_t restriction(Protection protection) {
    return protection == Protection::None ? 2 : protection == Protection::Read ? 1 : 0;
}

void markPages(std::uint64_t address, std::size_t bytes, Protection protection) {
    const auto value = restriction(protection);
    const auto pageSize = Platform::PageSize();
    const auto end = address + bytes;
    auto cursor = address;
    while (cursor < end) {
        if (cursor >= PageStateLimit) throw std::runtime_error("tracked guest memory lies beyond the page state table");
        const auto block = cursor >> PageBlockShift;
        const auto blockEnd = std::min(end, (block + 1) << PageBlockShift);
        auto* states = registry().pageStates[block].load(std::memory_order_acquire);
        if (states == nullptr) {
            if (value == 0) {
                cursor = blockEnd;
                continue;
            }
            states = new std::atomic<std::uint8_t>[(std::uint64_t{1} << PageBlockShift) / pageSize]();
            registry().pageStates[block].store(states, std::memory_order_release);
        }
        for (auto page = (cursor - (block << PageBlockShift)) / pageSize; cursor < blockEnd; cursor += pageSize, ++page) states[page].store(value, std::memory_order_release);
    }
}

bool pagesRestricted(std::uint64_t address, std::uint64_t end, bool writable) {
    const auto pageSize = Platform::PageSize();
    const std::uint8_t threshold = writable ? 1 : 2;
    auto cursor = address - address % pageSize;
    while (cursor < end) {
        if (cursor >= PageStateLimit) return true;
        const auto block = cursor >> PageBlockShift;
        const auto blockEnd = std::min(end, (block + 1) << PageBlockShift);
        const auto* states = registry().pageStates[block].load(std::memory_order_acquire);
        if (states == nullptr) {
            cursor = blockEnd;
            continue;
        }
        for (auto page = (cursor - (block << PageBlockShift)) / pageSize; cursor < blockEnd; cursor += pageSize, ++page) {
            if (states[page].load(std::memory_order_acquire) >= threshold) return true;
        }
    }
    return false;
}

std::uint64_t checkedEnd(std::uint64_t address, std::size_t bytes) {
    if (address == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) {
        char message[128];
        std::snprintf(message, sizeof(message), "invalid tracked guest memory range 0x%llx (%zu bytes)", static_cast<unsigned long long>(address), bytes);
#if !defined(_WIN32)
        void* frames[24];
        const auto count = ::backtrace(frames, 24);
        std::fprintf(stderr, "%s\n", message);
        ::backtrace_symbols_fd(frames, count, 2);
#endif
        throw std::invalid_argument(message);
    }
    return address + bytes;
}

void resolve(const std::shared_ptr<Entry>& entry, Access access) {
    if (entry->resolving) throw std::runtime_error("recursive guest memory ownership resolution");
    entry->resolving = true;
    try {
        entry->resolver(entry->context, access);
        if (entry->protection == Protection::None || (access != Access::Read && entry->protection != Protection::ReadWrite)) throw std::runtime_error("guest memory resolver did not release CPU access");
        entry->resolving = false;
    } catch (...) {
        entry->resolving = false;
        throw;
    }
}

std::vector<std::shared_ptr<Entry>> overlapping(std::uint64_t address, std::size_t bytes) {
    const auto end = checkedEnd(address, bytes);
    auto& entries = registry().entries;
    auto first = entries.upper_bound(address);
    if (first != entries.begin()) --first;
    std::vector<std::shared_ptr<Entry>> result;
    for (auto it = first; it != entries.end() && it->first < end; ++it) {
        if (it->first + it->second->bytes > address) result.push_back(it->second);
    }
    return result;
}

bool fault(std::uint64_t address, bool writable) {
    if (address == 0) return false;
    SlowOperationTimer timer(writable ? "tracking fault write" : "tracking fault read");
    std::lock_guard lock(registry().mutex);
    timer.Split("tracking fault lock wait");
    const auto entries = overlapping(address, 1);
    if (entries.empty()) return false;
    bool handled = false;
    for (const auto& entry : entries) {
        if (!entry->active) continue;
        if (entry->protection == Protection::None || (writable && entry->protection == Protection::Read)) resolve(entry, writable ? Access::Write : Access::Read);
        handled = true;
    }
    return handled;
}

}

std::recursive_mutex& GuestMemoryTrackingMutex_nid_postfix() {
    return registry().mutex;
}

std::size_t GuestMemoryTrackingPageSize_nid_postfix() {
    return Platform::PageSize();
}

void* GuestMemoryTrackingCreate_nid_postfix(std::uint64_t address, std::size_t bytes, void* context, Resolver resolver) {
    const auto end = checkedEnd(address, bytes);
    const auto pageSize = Platform::PageSize();
    if (context == nullptr || resolver == nullptr) throw std::invalid_argument("missing guest memory ownership resolver");
    if (end > std::numeric_limits<std::uint64_t>::max() - (pageSize - 1)) throw std::overflow_error("tracked guest memory page range overflow");
    const auto first = address - address % pageSize;
    const auto last = (end + pageSize - 1) / pageSize * pageSize;
    std::lock_guard lock(registry().mutex);
    GuestMemoryBacking::GuestMemoryBackingRequire_nid_postfix(first, static_cast<std::size_t>(last - first));
    if (!overlapping(first, static_cast<std::size_t>(last - first)).empty()) throw std::runtime_error("overlapping guest memory ownership pages");
    static_cast<void>(Platform::Query(first, static_cast<std::size_t>(last - first)));
    if (!registry().installed) {
        Platform::Install(fault);
        registry().installed = true;
    }
    auto entry = std::make_shared<Entry>();
    entry->address = first;
    entry->bytes = static_cast<std::size_t>(last - first);
    entry->context = context;
    entry->resolver = resolver;
    auto handle = std::make_unique<std::shared_ptr<Entry>>(entry);
    registry().entries.emplace(first, std::move(entry));
    return handle.release();
}

void GuestMemoryTrackingDestroy_nid_postfix(void* handle) noexcept {
    if (handle == nullptr) std::terminate();
    std::lock_guard lock(registry().mutex);
    std::unique_ptr<std::shared_ptr<Entry>> owner(static_cast<std::shared_ptr<Entry>*>(handle));
    const auto& entry = **owner;
    if (entry.protection != Protection::ReadWrite) {
        Platform::Restore(entry.original);
        markPages(entry.address, entry.bytes, Protection::ReadWrite);
    }
    registry().entries.erase(entry.address);
}

void GuestMemoryTrackingProtect_nid_postfix(void* handle, Protection protection) {
    if (handle == nullptr) throw std::invalid_argument("missing guest memory watch");
    std::lock_guard lock(registry().mutex);
    auto& entry = **static_cast<std::shared_ptr<Entry>*>(handle);
    if (entry.protection == protection) return;
    const bool tightening = restriction(protection) > restriction(entry.protection);
    if (tightening) markPages(entry.address, entry.bytes, protection);
    if (protection == Protection::ReadWrite) {
        Platform::Restore(entry.original);
    } else {
        if (entry.original.empty()) entry.original = Platform::Query(entry.address, entry.bytes);
        Platform::Protect(entry.address, entry.bytes, protection);
        entry.active = true;
    }
    entry.protection = protection;
    if (!tightening) markPages(entry.address, entry.bytes, protection);
}

void GuestMemoryTrackingResolve_nid_postfix(std::uint64_t address, std::size_t bytes, bool writable) {
    if (bytes == 0) return;
    if (!pagesRestricted(address, checkedEnd(address, bytes), writable)) return;
    SlowOperationTimer timer(writable ? "tracking resolve write" : "tracking resolve read");
    std::lock_guard lock(registry().mutex);
    timer.Split("tracking resolve lock wait");
    for (const auto& entry : overlapping(address, bytes)) {
        if (entry->protection == Protection::None || (writable && entry->protection == Protection::Read)) resolve(entry, writable ? Access::Write : Access::Read);
    }
}

void GuestMemoryTrackingInvalidate_nid_postfix(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return;
    SlowOperationTimer timer("tracking invalidate");
    std::lock_guard lock(registry().mutex);
    timer.Split("tracking invalidate lock wait");
    for (const auto& entry : overlapping(address, bytes)) {
        resolve(entry, Access::Invalidate);
        entry->original.clear();
        entry->active = false;
    }
}

void GuestMemoryTrackingReapply_nid_postfix(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return;
    std::lock_guard lock(registry().mutex);
    for (const auto& entry : overlapping(address, bytes)) {
        if (!entry->active || entry->protection == Protection::ReadWrite) continue;
        entry->original = Platform::Query(entry->address, entry->bytes);
        Platform::Protect(entry->address, entry->bytes, entry->protection);
    }
}

void GuestMemoryTrackingDescribe_nid_postfix(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return;
    auto& entries = registry().entries;
    std::fprintf(stderr, "  tracking registry: %zu watches, installed %d\n", entries.size(), registry().installed ? 1 : 0);
    const auto end = address + bytes;
    auto first = entries.upper_bound(address);
    if (first != entries.begin()) --first;
    for (auto it = first; it != entries.end() && it->first < end; ++it) {
        const auto& entry = *it->second;
        if (entry.address + entry.bytes <= address) continue;
        std::fprintf(stderr, "  watch 0x%llx+0x%zx protection %d active %d resolving %d\n", static_cast<unsigned long long>(entry.address), entry.bytes, static_cast<int>(entry.protection), entry.active ? 1 : 0, entry.resolving ? 1 : 0);
    }
    std::fflush(stderr);
}

void GuestMemoryTrackingValidate_nid_postfix(std::uint64_t address, std::size_t bytes, const std::function<void(std::uint64_t, std::size_t)>& validate) {
    if (bytes == 0) return;
    if (!validate) throw std::invalid_argument("missing native memory range validator");
    const auto end = checkedEnd(address, bytes);
    if (!pagesRestricted(address, end, true)) {
        try {
            validate(address, bytes);
            return;
        } catch (const std::exception&) {
        }
    }
    SlowOperationTimer timer("tracking validate");
    std::lock_guard lock(registry().mutex);
    timer.Split("tracking validate lock wait");
    for (const auto& entry : overlapping(address, bytes)) {
        if (address < entry->address) validate(address, static_cast<std::size_t>(entry->address - address));
        const auto first = std::max(address, entry->address);
        const auto last = std::min(end, entry->address + entry->bytes);
        if (entry->protection == Protection::ReadWrite) validate(first, static_cast<std::size_t>(last - first));
        else if (entry->original.empty()) throw std::runtime_error("protected guest memory has no native permission record");
        address = last;
    }
    if (address < end) validate(address, static_cast<std::size_t>(end - address));
}

}

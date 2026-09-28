#include "prx/libSceAgcDriver/Graphics/include/GuestBufferCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include <algorithm>
#include <set>
#include <chrono>
#include <cstdlib>
#include <limits>

namespace AgcDriver::Graphics {

namespace {

std::uint64_t ConfiguredBudget() {
    if (const char* value = std::getenv("ANYPS5_BUFFER_MIRROR_MB")) {
        const auto megabytes = std::strtoull(value, nullptr, 10);
        if (megabytes != 0) return megabytes << 20;
    }
    return 3ull << 30;
}

}

GuestBufferCache::GuestBufferCache(const Context& context) : context(context), budget(ConfiguredBudget()) {}

GuestBufferCache::~GuestBufferCache() {
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    mirrors.clear();
    chunks.clear();
}

GuestBufferCache::Chunk& GuestBufferCache::chunk(std::uint64_t address) {
    const auto base = address - address % chunkBytes;
    auto it = chunks.find(base);
    if (it == chunks.end()) {
        auto created = std::make_unique<Chunk>();
        created->address = base;
        created->owner = std::make_unique<Owner>(Owner{this, base});
        created->generation = generation;
        it = chunks.emplace(base, std::move(created)).first;
    }
    return *it->second;
}

// Called by the tracking registry (under its mutex) when the CPU, or the driver on the CPU's
// behalf, touches a watched chunk.
void GuestBufferCache::resolve(void* owner, GuestMemoryTracking::Access access) {
    auto* self = static_cast<Owner*>(owner);
    auto& cache = *self->cache;
    std::lock_guard lock(cache.mutex);
    const auto it = cache.chunks.find(self->address);
    if (it == cache.chunks.end()) return;
    auto& chunk = *it->second;
    if (access == GuestMemoryTracking::Access::Read) return; // reads see what the mirror holds
    chunk.generation = ++cache.generation;
    chunk.protectedRead = false;
    if (chunk.watch) chunk.watch->Protect(GuestMemoryTracking::Protection::ReadWrite);
    if (access == GuestMemoryTracking::Access::Invalidate) chunk.lost = true;
}

bool GuestBufferCache::current(const Mirror& mirror) {
    for (auto address = mirror.begin - mirror.begin % chunkBytes; address < mirror.end; address += chunkBytes) {
        const auto it = chunks.find(address);
        if (it == chunks.end()) return false;
        const auto& chunk = *it->second;
        if (chunk.immutableSince != 0 && chunk.immutableSince == GuestAllocations::GuestAllocationsProtectionGeneration_nid_postfix()) continue;
        if (chunk.untrackable || chunk.lost || !chunk.protectedRead || chunk.generation > mirror.synced) return false;
    }
    return true;
}

// Watches every chunk of the mirror against CPU writes (creating the watches on first use).
void GuestBufferCache::protect(const Mirror& mirror) {
    for (auto address = mirror.begin - mirror.begin % chunkBytes; address < mirror.end; address += chunkBytes) {
        auto& chunk = this->chunk(address);
        if (chunk.immutableSince != 0) {
            if (chunk.immutableSince == GuestAllocations::GuestAllocationsProtectionGeneration_nid_postfix()) continue;
            chunk.immutableSince = 0; // protections moved: watch it like any other chunk
            chunk.untrackable = false;
            chunk.generation = ++generation;
        }
        if (chunk.untrackable) continue;
        if (chunk.lost) {
            chunk.watch.reset();
            chunk.lost = false;
        }
        if (!chunk.watch) {
            try {
                chunk.watch = std::make_unique<GuestMemoryTracking::Watch>(chunk.address, chunkBytes, chunk.owner.get(), &GuestBufferCache::resolve);
            } catch (const std::exception& error) {
                // Mapped read-only for the CPU: nothing writes it, so it counts as current until
                // its protection changes.
                bool readable = true, writable = true;
                try {
                    GuestMemory::CheckRange(reinterpret_cast<const void*>(chunk.address), chunkBytes, 1, false);
                } catch (...) {
                    readable = false;
                }
                try {
                    GuestMemory::CheckRange(reinterpret_cast<const void*>(chunk.address), chunkBytes, 1, true);
                } catch (...) {
                    writable = false;
                }
                if (readable && !writable) {
                    chunk.immutableSince = GuestAllocations::GuestAllocationsProtectionGeneration_nid_postfix();
                    continue;
                }
                // Pages shared with a render target, or not (fully) mapped: never trusted.
                static int reported = 0;
                if (reported++ < 8) APS5_LOG_OUT("guest buffer chunk 0x%llx is not watchable (%s); buffers there copy on every use", static_cast<unsigned long long>(chunk.address), error.what());
                chunk.untrackable = true;
                chunk.untrackableReason = 'w';
                continue;
            }
        }
        if (!chunk.protectedRead) {
            chunk.watch->Protect(GuestMemoryTracking::Protection::Read);
            chunk.protectedRead = true;
        }
    }
}

std::shared_ptr<GuestBufferCache::Mirror> GuestBufferCache::Acquire(std::uint64_t begin, std::uint64_t end, VkBufferUsageFlags usage) {
    PerformanceTimer timing("Graphics.GuestBufferCache");
    Require(begin < end && end - begin <= std::numeric_limits<std::size_t>::max(), "invalid guest buffer mirror range");
    // Lock order: the tracking registry first (its fault handler holds it when it calls resolve).
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    const auto bytes = static_cast<std::size_t>(end - begin);
    auto& slot = mirrors[{begin, end}];
    if (slot && (slot->buffer->Usage() & usage) != usage) slot.reset();
    if (slot && current(*slot)) {
        slot->lastUse = ++uses;
        reusedBytes += bytes;
        timing.Mark("hit");
        return slot;
    }
    // Heap-sized ranges: the GPU reads and writes guest memory in place, as on the console.
    if (context.hostPointerImport && bytes >= ImportBytes) {
        if (slot && slot->imported && (slot->buffer->Usage() & usage) == usage) {
            slot->lastUse = ++uses;
            timing.Mark("imported");
            return slot;
        }
        if (auto imported = import(begin, end, usage)) {
            if (slot && !slot->imported) retainedBytes -= end - begin;
            slot = imported;
            slot->lastUse = ++uses;
            timing.Mark("import");
            return slot;
        }
    }
    // Large mirrors (a descriptor over a title's whole heap) follow the console's unified memory:
    // the stale chunks are refreshed in place, even while a submission holds the mirror. The
    // title only rewrites memory the GPU is done with (it waits on the GPU's labels), so the parts
    // a pending draw reads do not change; recopying gigabytes per draw is what this avoids.
    static const bool incremental = std::getenv("ANYPS5_NO_INCREMENTAL_MIRRORS") == nullptr; // diagnostics
    if (incremental && slot && bytes >= IncrementalBytes) {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> stale;
        for (auto address = begin - begin % chunkBytes; address < end; address += chunkBytes) {
            const auto it = chunks.find(address);
            bool fresh = it != chunks.end();
            if (fresh) {
                const auto& chunk = *it->second;
                const bool immutable = chunk.immutableSince != 0 && chunk.immutableSince == GuestAllocations::GuestAllocationsProtectionGeneration_nid_postfix();
                fresh = immutable || (!chunk.untrackable && !chunk.lost && chunk.protectedRead && chunk.generation <= slot->synced);
            }
            if (fresh) continue;
            const auto first = std::max(address, begin), last = std::min(address + chunkBytes, end);
            if (!stale.empty() && stale.back().second == first) stale.back().second = last;
            else stale.emplace_back(first, last);
        }
        protect(*slot);
        slot->synced = generation;
        std::uint64_t refreshed = 0;
        for (const auto& [first, last] : stale) {
            GuestMemory::Read(first, slot->buffer->Bytes().subspan(static_cast<std::size_t>(first - begin), static_cast<std::size_t>(last - first)));
            refreshed += last - first;
        }
        copiedBytes += refreshed;
        reusedBytes += bytes - refreshed;
        slot->lastUse = ++uses;
        timing.Mark("refresh");
        auto mirror = slot;
        trim();
        return mirror;
    }
    if (!slot || slot.use_count() > 1) {
        // Absent, or still bound by a submission in flight: a fresh mirror leaves that one alone.
        if (slot) retainedBytes -= end - begin;
        slot = std::make_shared<Mirror>(Mirror{begin, end, std::make_shared<Buffer>(context, bytes, usage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), 0, 0});
        retainedBytes += bytes;
        timing.Mark("allocate");
    }
    // Protect first, then copy: a write that lands during the copy faults and ages the chunk, so
    // the mirror is not trusted next time.
    protect(*slot);
    slot->synced = generation;
    GuestMemory::Read(begin, slot->buffer->Bytes());
    copiedBytes += bytes;
    {
        // Statistics every two seconds: how much is copied versus reused, and the largest copies.
        static auto lastReport = std::chrono::steady_clock::now();
        static std::uint64_t reportedCopied = 0, reportedReused = 0, copies = 0, largest = 0, largestBegin = 0;
        ++copies;
        if (bytes > largest) { largest = bytes; largestBegin = begin; }
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport >= std::chrono::seconds(2)) {
            std::size_t untrackable = 0, lost = 0;
            for (const auto& [address, chunk] : chunks) { untrackable += chunk->untrackable; lost += chunk->lost; }
            APS5_LOG_OUT("buffer mirrors: copied %.1f MiB in %llu copies (largest 0x%llx+%.1f MiB), reused %.1f MiB, retained %.1f MiB, %zu mirrors, chunks %zu (untrackable %zu, lost %zu)",
                (copiedBytes - reportedCopied) / 1048576.0, static_cast<unsigned long long>(copies), static_cast<unsigned long long>(largestBegin), largest / 1048576.0,
                (reusedBytes - reportedReused) / 1048576.0, retainedBytes / 1048576.0, mirrors.size(), chunks.size(), untrackable, lost);
            reportedCopied = copiedBytes; reportedReused = reusedBytes; copies = 0; largest = 0; lastReport = now;
        }
    }
    slot->lastUse = ++uses;
    timing.Mark("copy");
    // trim() may erase map nodes, `slot` included when the budget is exceeded: hold the mirror
    // first (a held mirror is never a candidate) and never touch the reference afterwards.
    auto mirror = slot;
    trim();
    return mirror;
}

std::shared_ptr<GuestBufferCache::Mirror> GuestBufferCache::import(std::uint64_t begin, std::uint64_t end, VkBufferUsageFlags usage) {
    const auto alignment = static_cast<std::uint64_t>(context.hostPointerAlignment);
    const auto first = begin - begin % alignment;
    const auto last = (end + alignment - 1) / alignment * alignment;
    // A range that failed to import once is mirrored from then on.
    static std::set<std::pair<std::uint64_t, std::uint64_t>> refused;
    if (refused.count({begin, end}) != 0) return nullptr;
    void* alias = GuestMemoryBacking::GuestMemoryBackingAlias_nid_postfix(first, static_cast<std::size_t>(last - first));
    static int reported = 0;
    if (alias == nullptr) {
        // Memory outside the shared guest mappings (the executable's own data and bss) is plain
        // host memory at the guest address: import it where it is, when it is readable and writable.
        try {
            GuestMemory::CheckRange(reinterpret_cast<const void*>(first), static_cast<std::size_t>(last - first), 1, true);
            alias = reinterpret_cast<void*>(first);
        } catch (const std::exception&) {
            if (reported++ < 4) APS5_LOG_OUT("guest range 0x%llx+0x%llx has no shared backing; mirroring it", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin));
            refused.insert({begin, end});
            return nullptr;
        }
    }
    try {
        auto buffer = std::make_shared<Buffer>(context, Buffer::HostImport{}, alias, static_cast<std::size_t>(last - first), static_cast<std::size_t>(begin - first), static_cast<std::size_t>(end - begin), usage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        if (reported++ < 4) APS5_LOG_OUT("guest range 0x%llx+%.1f MiB bound in place (host import)", static_cast<unsigned long long>(begin), (end - begin) / 1048576.0);
        auto mirror = std::make_shared<Mirror>(Mirror{begin, end, std::move(buffer), 0, 0});
        mirror->imported = true;
        return mirror;
    } catch (const std::exception& error) {
        if (reported++ < 4) APS5_LOG_OUT("host import of 0x%llx+0x%llx failed (%s); mirroring it", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin), error.what());
        refused.insert({begin, end});
        return nullptr;
    }
}

GuestBufferCache::HostView GuestBufferCache::HostRange(std::uint64_t address, std::uint64_t bytes) {
    static const bool disabled = std::getenv("ANYPS5_NO_HOST_VERTEX") != nullptr; // diagnostics
    if (disabled || !context.hostPointerImport || bytes == 0) return {};
    GuestMemoryBacking::GuestMemoryBackingExtentInfo extent{};
    const bool found = GuestMemoryBacking::GuestMemoryBackingExtent_nid_postfix(address, static_cast<std::size_t>(bytes), &extent);
    const auto alignment = static_cast<std::uint64_t>(context.hostPointerAlignment);
    // The import covers whole host pages of the mapping; a range in a partial tail page is copied.
    const auto importBytes = extent.bytes / alignment * alignment;
    if (!found || reinterpret_cast<std::uintptr_t>(extent.alias) % alignment != 0 || address + bytes > extent.address + importBytes) {
        static const bool trace = std::getenv("ANYPS5_TRACE_WAITS") != nullptr; // diagnostics
        static int reported = 0;
        if (trace && bytes >= (1u << 20) && reported++ < 20) APS5_LOG_OUT("host range 0x%llx+0x%llx unavailable: extent %d 0x%llx+0x%llx alias %p", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), found ? 1 : 0, static_cast<unsigned long long>(extent.address), static_cast<unsigned long long>(extent.bytes), extent.alias);
        return {};
    }
    std::lock_guard lock(mutex);
    auto& mapping = hostMappings[extent.address];
    if (!mapping.buffer || mapping.serial != extent.serial || mapping.bytes != importBytes) {
        try {
            const VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | (context.bufferDeviceAddress ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
            mapping.buffer = std::make_shared<Buffer>(context, Buffer::HostImport{}, extent.alias, static_cast<std::size_t>(importBytes), 0, static_cast<std::size_t>(importBytes), usage);
            mapping.serial = extent.serial;
            mapping.bytes = importBytes;
            static int reported = 0;
            if (reported++ < 8) APS5_LOG_OUT("guest mapping 0x%llx+%.1f MiB imported for vertex and index data", static_cast<unsigned long long>(extent.address), importBytes / 1048576.0);
        } catch (const std::exception& error) {
            static int reported = 0;
            if (reported++ < 4) APS5_LOG_OUT("host import of guest mapping 0x%llx failed (%s); copying its data", static_cast<unsigned long long>(extent.address), error.what());
            hostMappings.erase(extent.address);
            return {};
        }
    }
    return {mapping.buffer, address - extent.address};
}

void GuestBufferCache::MarkSynced(const std::shared_ptr<Mirror>& mirror) {
    if (!mirror || mirror->imported) return;
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    // The write-back went through the tracking registry, which aged (and unprotected) the
    // chunks; every other mirror there is stale now. This one matches guest memory again.
    protect(*mirror);
    mirror->synced = generation;
}

void GuestBufferCache::ReleaseTracking(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return;
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    const auto first = address - address % chunkBytes;
    for (auto base = first; base < address + bytes; base += chunkBytes) {
        const auto it = chunks.find(base);
        if (it == chunks.end()) continue;
        auto& chunk = *it->second;
        chunk.watch.reset();
        chunk.generation = ++generation;
        chunk.protectedRead = false;
        chunk.untrackable = true;
        chunk.untrackableReason = 'r';
    }
}

std::uint64_t GuestBufferCache::Track(std::uint64_t begin, std::uint64_t end) {
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    Mirror range{begin, end, nullptr, 0, 0};
    protect(range);
    return generation;
}

bool GuestBufferCache::Current(std::uint64_t begin, std::uint64_t end, std::uint64_t stamp) {
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    Mirror range{begin, end, nullptr, stamp, 0};
    return current(range);
}

std::string GuestBufferCache::Describe(std::uint64_t begin, std::uint64_t end, std::uint64_t stamp) {
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    std::string result;
    for (auto address = begin - begin % chunkBytes; address < end; address += chunkBytes) {
        const auto it = chunks.find(address);
        char text[96];
        if (it == chunks.end()) { std::snprintf(text, sizeof(text), " 0x%llx:none", static_cast<unsigned long long>(address)); result += text; continue; }
        const auto& chunk = *it->second;
        std::snprintf(text, sizeof(text), " 0x%llx:%s%c%s%s%s", static_cast<unsigned long long>(address), chunk.untrackable ? "untrackable" : "", chunk.untrackable ? chunk.untrackableReason : ' ', chunk.lost ? "lost" : "", chunk.protectedRead ? "" : "unprotected", chunk.generation > stamp ? "aged" : "");
        result += text;
    }
    return result;
}

void GuestBufferCache::Flush() {
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    for (auto it = mirrors.begin(); it != mirrors.end();) {
        if (it->second.use_count() == 1) {
            retainedBytes -= it->second->end - it->second->begin;
            it = mirrors.erase(it);
        } else {
            ++it;
        }
    }
}

void GuestBufferCache::trim() {
    if (retainedBytes <= budget) return;
    // Least recently used first, never a mirror a submission still holds.
    std::vector<decltype(mirrors)::iterator> candidates;
    // Mirrors used by the last few hundred acquisitions stay: evicting a mirror that is about to
    // be reused only turns into a full copy on the next draw.
    for (auto it = mirrors.begin(); it != mirrors.end(); ++it) if (it->second && !it->second->imported && it->second.use_count() == 1 && it->second->lastUse + 256 < uses) candidates.push_back(it);
    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) { return left->second->lastUse < right->second->lastUse; });
    for (const auto& it : candidates) {
        if (retainedBytes <= budget) break;
        retainedBytes -= it->second->end - it->second->begin;
        mirrors.erase(it);
    }
}

}

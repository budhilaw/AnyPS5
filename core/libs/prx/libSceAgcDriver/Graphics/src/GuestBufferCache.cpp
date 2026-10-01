#include "prx/libSceAgcDriver/Graphics/include/GuestBufferCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReleaseQueue.hpp"
#include "prx/libc/include/SlowOperation.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include <algorithm>
#include <set>
#include <string>
#include <chrono>
#include <cstdlib>
#include <cstring>
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

std::uint64_t ConfiguredResidentBudget() {
    if (const char* value = std::getenv("ANYPS5_RESIDENT_MB")) {
        const auto megabytes = std::strtoull(value, nullptr, 10);
        if (megabytes != 0) return megabytes << 20;
    }
    return 2560ull << 20;
}

std::chrono::steady_clock::rep Ticks(std::chrono::steady_clock::duration duration) {
    return duration.count();
}

}

bool TextureMemcmp() {
    static const bool enabled = [] {
        const char* value = std::getenv("ANYPS5_TEXTURE_MEMCMP");
        return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

namespace {

std::atomic<int> residentOverride{-1};

}

bool GuestBufferCache::ResidentEnabled() {
    if (const auto forced = residentOverride.load(std::memory_order_relaxed); forced >= 0) return forced != 0;
    static const bool enabled = std::getenv("ANYPS5_NO_RESIDENT_BUFFERS") == nullptr && std::getenv("ANYPS5_CAPTURE_DISPATCH") == nullptr && !TextureMemcmp();
    return enabled;
}

void GuestBufferCache::SetResidentEnabled(bool enabled) {
    residentOverride.store(enabled ? 1 : 0, std::memory_order_relaxed);
}

GuestBufferCache::GuestBufferCache(const Context& context) : context(context), stateBlocks(new std::atomic<ChunkState*>[StateLimit >> StateBlockShift]()), budget(ConfiguredBudget()), residentBudget(ConfiguredResidentBudget()) {}

GuestBufferCache::~GuestBufferCache() {
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    mirrors.clear();
    chunks.clear();
}

GuestBufferCache::ChunkState* GuestBufferCache::stateOf(std::uint64_t address) const {
    if (address >= StateLimit) return nullptr;
    auto* block = stateBlocks[address >> StateBlockShift].load(std::memory_order_acquire);
    if (block == nullptr) return nullptr;
    return &block[(address & ((std::uint64_t{1} << StateBlockShift) - 1)) / chunkBytes];
}

GuestBufferCache::ChunkState* GuestBufferCache::createState(std::uint64_t address) {
    if (address >= StateLimit) return nullptr;
    auto& slot = stateBlocks[address >> StateBlockShift];
    auto* block = slot.load(std::memory_order_acquire);
    if (block == nullptr) {
        stateStorage.push_back(std::make_unique<ChunkState[]>(StateBlockChunks));
        block = stateStorage.back().get();
        slot.store(block, std::memory_order_release);
    }
    return &block[(address & ((std::uint64_t{1} << StateBlockShift) - 1)) / chunkBytes];
}

void GuestBufferCache::publish(const Chunk& chunk) {
    if (chunk.published == nullptr) return;
    const auto flags = (chunk.protectedRead ? ProtectedState : 0) | (chunk.lost ? LostState : 0) | (chunk.untrackable ? UntrackableState : 0) | (chunk.written ? WrittenState : 0);
    chunk.published->immutableSince.store(chunk.immutableSince, std::memory_order_relaxed);
    chunk.published->state.store((chunk.generation << StateFlagBits) | flags, std::memory_order_release);
}

void GuestBufferCache::touch(ChunkState& state, std::chrono::steady_clock::rep now) {
    if (now - state.touched.load(std::memory_order_relaxed) >= Ticks(TouchGranularity)) state.touched.store(now, std::memory_order_relaxed);
}

GuestBufferCache::Chunk& GuestBufferCache::chunk(std::uint64_t address) {
    const auto base = address - address % chunkBytes;
    auto it = chunks.find(base);
    if (it == chunks.end()) {
        auto created = std::make_unique<Chunk>();
        created->address = base;
        created->owner = std::make_unique<Owner>(Owner{this, base});
        created->generation = generation;
        created->published = createState(base);
        publish(*created);
        it = chunks.emplace(base, std::move(created)).first;
    }
    return *it->second;
}

void GuestBufferCache::resolve(void* owner, GuestMemoryTracking::Access access) {
    SlowOperationTimer slowTimer("buffer cache cpu access");
    auto* self = static_cast<Owner*>(owner);
    auto& cache = *self->cache;
    std::lock_guard lock(cache.mutex);
    const auto it = cache.chunks.find(self->address);
    if (it == cache.chunks.end()) return;
    auto& chunk = *it->second;
    if (access == GuestMemoryTracking::Access::Read) return;
    chunk.generation = ++cache.generation;
    chunk.protectedRead = false;
    chunk.written = true;
    if (chunk.watch) chunk.watch->Protect(GuestMemoryTracking::Protection::ReadWrite);
    if (access == GuestMemoryTracking::Access::Invalidate) chunk.lost = true;
    publish(chunk);
}

void GuestBufferCache::ageChunks() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    if (now - swept.load(std::memory_order_relaxed) < Ticks(ChunkSweep)) return;
    swept.store(now, std::memory_order_relaxed);
    for (auto& [address, chunk] : chunks) {
        if (!chunk->watch || !chunk->protectedRead || chunk->published == nullptr || now - chunk->published->touched.load(std::memory_order_relaxed) < Ticks(ChunkAge)) continue;
        chunk->watch->Protect(GuestMemoryTracking::Protection::ReadWrite);
        chunk->protectedRead = false;
        chunk->generation = ++generation;
        publish(*chunk);
    }
}

bool GuestBufferCache::current(const Mirror& mirror) {
    const auto protection = GuestAllocations::GuestAllocationsProtectionGeneration_nid_postfix();
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto first = mirror.begin - mirror.begin % chunkBytes;
    auto it = chunks.find(first);
    for (auto address = first; address < mirror.end; address += chunkBytes, ++it) {
        if (it == chunks.end() || it->first != address) return false;
        auto& chunk = *it->second;
        if (chunk.published != nullptr) touch(*chunk.published, now);
        if (chunk.immutableSince != 0 && chunk.immutableSince == protection) continue;
        if (chunk.untrackable || chunk.lost || !chunk.protectedRead || chunk.generation > mirror.synced) return false;
    }
    return true;
}

void GuestBufferCache::protect(const Mirror& mirror) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    for (auto address = mirror.begin - mirror.begin % chunkBytes; address < mirror.end; address += chunkBytes) {
        auto& chunk = this->chunk(address);
        protectChunk(chunk, now);
        publish(chunk);
    }
}

void GuestBufferCache::protectChunk(Chunk& chunk, std::chrono::steady_clock::rep now) {
    if (chunk.published != nullptr) touch(*chunk.published, now);
    if (chunk.immutableSince != 0) {
        if (chunk.immutableSince == GuestAllocations::GuestAllocationsProtectionGeneration_nid_postfix()) return;
        chunk.immutableSince = 0;
        chunk.untrackable = false;
        chunk.generation = ++generation;
    }
    if (chunk.untrackable) return;
    if (chunk.lost) {
        chunk.watch.reset();
        chunk.lost = false;
    }
    if (!chunk.watch) {
        try {
            chunk.watch = std::make_unique<GuestMemoryTracking::Watch>(chunk.address, chunkBytes, chunk.owner.get(), &GuestBufferCache::resolve);
        } catch (const std::exception& error) {
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
                return;
            }
            static int reported = 0;
            if (reported++ < 8) APS5_LOG_OUT("guest buffer chunk 0x%llx is not watchable (%s); buffers there copy on every use", static_cast<unsigned long long>(chunk.address), error.what());
            chunk.untrackable = true;
            chunk.untrackableReason = 'w';
            return;
        }
    }
    if (!chunk.protectedRead) {
        chunk.watch->Protect(GuestMemoryTracking::Protection::Read);
        chunk.protectedRead = true;
        chunk.written = false;
    }
}

std::shared_ptr<GuestBufferCache::Mirror> GuestBufferCache::Acquire(std::uint64_t begin, std::uint64_t end, VkBufferUsageFlags usage) {
    PerformanceTimer timing("Graphics.GuestBufferCache");
    Require(begin < end && end - begin <= std::numeric_limits<std::size_t>::max(), "invalid guest buffer mirror range");
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    ageChunks();
    const auto bytes = static_cast<std::size_t>(end - begin);
    auto& slot = mirrors[{begin, end}];
    if (slot && (slot->buffer->Usage() & usage) != usage) slot.reset();
    if (slot && current(*slot)) {
        slot->lastUse = ++uses;
        reusedBytes += bytes;
        timing.Mark("hit");
        return slot;
    }
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
    static const bool incremental = std::getenv("ANYPS5_NO_INCREMENTAL_MIRRORS") == nullptr;
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
        if (slot) retainedBytes -= end - begin;
        slot = std::make_shared<Mirror>(Mirror{begin, end, std::make_shared<Buffer>(context, bytes, usage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), 0, 0});
        retainedBytes += bytes;
        timing.Mark("allocate");
    }
    protect(*slot);
    slot->synced = generation;
    GuestMemory::Read(begin, slot->buffer->Bytes());
    copiedBytes += bytes;
    {
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
    auto mirror = slot;
    trim();
    return mirror;
}

std::shared_ptr<GuestBufferCache::Mirror> GuestBufferCache::import(std::uint64_t begin, std::uint64_t end, VkBufferUsageFlags usage) {
    const auto alignment = static_cast<std::uint64_t>(context.hostPointerAlignment);
    const auto first = begin - begin % alignment;
    const auto last = (end + alignment - 1) / alignment * alignment;
    static std::set<std::pair<std::uint64_t, std::uint64_t>> refused;
    if (refused.count({begin, end}) != 0) return nullptr;
    void* alias = nullptr;
    void* retained = nullptr;
    GuestMemoryBacking::GuestMemoryBackingExtentInfo extent{};
    if (GuestMemoryBacking::GuestMemoryBackingExtent_nid_postfix(first, static_cast<std::size_t>(last - first), &extent)) {
        retained = GuestMemoryBacking::GuestMemoryBackingRetainAlias_nid_postfix(extent.address, extent.serial);
        if (retained != nullptr) alias = static_cast<std::byte*>(retained) + (first - extent.address);
    }
    static int reported = 0;
    if (alias == nullptr) {
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
        std::unique_ptr<Buffer> imported;
        try {
            imported = std::make_unique<Buffer>(context, Buffer::HostImport{}, alias, static_cast<std::size_t>(last - first), static_cast<std::size_t>(begin - first), static_cast<std::size_t>(end - begin), usage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        } catch (...) {
            if (retained != nullptr) GuestMemoryBacking::GuestMemoryBackingReleaseAlias_nid_postfix(retained);
            throw;
        }
        auto buffer = std::shared_ptr<Buffer>(imported.release(), [retained, releases = context.releaseQueue](Buffer* released) {
            delete released;
            if (retained != nullptr) Release(releases, [retained] { GuestMemoryBacking::GuestMemoryBackingReleaseAlias_nid_postfix(retained); });
        });
        if (reported++ < 4 || end - begin > HostWindowBytes) APS5_LOG_OUT("guest range 0x%llx+%.1f MiB bound in place (host import)", static_cast<unsigned long long>(begin), (end - begin) / 1048576.0);
        auto mirror = std::make_shared<Mirror>(Mirror{begin, end, std::move(buffer), 0, 0});
        mirror->imported = true;
        return mirror;
    } catch (const std::exception& error) {
        if (reported++ < 4) APS5_LOG_OUT("host import of 0x%llx+0x%llx failed (%s); mirroring it", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin), error.what());
        refused.insert({begin, end});
        return nullptr;
    }
}

bool GuestBufferCache::hostExtent(std::uint64_t address, std::uint64_t bytes, GuestMemoryBacking::GuestMemoryBackingExtentInfo& extent, std::uint64_t& importBytes) const {
    static const bool disabled = std::getenv("ANYPS5_NO_HOST_VERTEX") != nullptr;
    if (disabled || !context.hostPointerImport || bytes == 0) return false;
    const bool found = GuestMemoryBacking::GuestMemoryBackingExtent_nid_postfix(address, static_cast<std::size_t>(bytes), &extent);
    const auto alignment = static_cast<std::uint64_t>(context.hostPointerAlignment);
    importBytes = extent.bytes / alignment * alignment;
    if (!found || reinterpret_cast<std::uintptr_t>(extent.alias) % alignment != 0 || address + bytes > extent.address + importBytes) {
        static const bool trace = std::getenv("ANYPS5_TRACE_WAITS") != nullptr;
        static int reported = 0;
        if (trace && reported++ < 60) APS5_LOG_OUT("host range 0x%llx+0x%llx unavailable: extent %d 0x%llx+0x%llx alias %p", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), found ? 1 : 0, static_cast<unsigned long long>(extent.address), static_cast<unsigned long long>(extent.bytes), extent.alias);
        return false;
    }
    return true;
}

bool GuestBufferCache::HostImportable(std::uint64_t address, std::uint64_t bytes) const {
    GuestMemoryBacking::GuestMemoryBackingExtentInfo extent{};
    std::uint64_t importBytes = 0;
    return hostExtent(address, bytes, extent, importBytes);
}

GuestBufferCache::HostView GuestBufferCache::HostRange(std::uint64_t address, std::uint64_t bytes, bool allowResident) {
    GuestMemoryBacking::GuestMemoryBackingExtentInfo extent{};
    std::uint64_t importBytes = 0;
    if (!hostExtent(address, bytes, extent, importBytes)) return {};
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    if (allowResident && ResidentEnabled() && context.drawQueue != nullptr) {
        if (auto view = residentRange(address, bytes, extent, importBytes); view.buffer) return view;
    }
    if (const auto generation = GuestMemoryBacking::GuestMemoryBackingUnmapGeneration_nid_postfix(); generation != unmapGeneration) {
        unmapGeneration = generation;
        for (auto it = hostMappings.begin(); it != hostMappings.end();) {
            GuestMemoryBacking::GuestMemoryBackingExtentInfo current{};
            const bool live = GuestMemoryBacking::GuestMemoryBackingExtent_nid_postfix(it->first, 1, &current) && current.serial == it->second.serial;
            if (live) {
                ++it;
                continue;
            }
            static int reported = 0;
            if (reported++ < 16) APS5_LOG_OUT("guest mapping 0x%llx was unmapped: its import goes once its users finish", static_cast<unsigned long long>(it->first));
            hostBytes -= it->second.bytes;
            it = hostMappings.erase(it);
        }
    }
    if (auto window = hostMappings.upper_bound(address); window != hostMappings.begin()) {
        auto& [first, mapping] = *std::prev(window);
        if (mapping.serial == extent.serial && address + bytes <= first + mapping.bytes) {
            mapping.lastUse = ++hostUses;
            return {mapping.buffer, address - first};
        }
    }
    const auto first = extent.address + (address - extent.address) / HostWindowBytes * HostWindowBytes;
    const auto last = std::min(extent.address + importBytes, extent.address + (address + bytes - extent.address + HostWindowBytes - 1) / HostWindowBytes * HostWindowBytes);
    void* alias = GuestMemoryBacking::GuestMemoryBackingRetainAlias_nid_postfix(extent.address, extent.serial);
    if (alias == nullptr) return {};
    std::shared_ptr<Buffer> buffer;
    try {
        const VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | (context.bufferDeviceAddress ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
        std::unique_ptr<Buffer> imported;
        try {
            imported = std::make_unique<Buffer>(context, Buffer::HostImport{}, static_cast<std::byte*>(alias) + (first - extent.address), static_cast<std::size_t>(last - first), 0, static_cast<std::size_t>(last - first), usage);
        } catch (...) {
            GuestMemoryBacking::GuestMemoryBackingReleaseAlias_nid_postfix(alias);
            throw;
        }
        buffer = std::shared_ptr<Buffer>(imported.release(), [alias, releases = context.releaseQueue](Buffer* released) {
            delete released;
            Release(releases, [alias] { GuestMemoryBacking::GuestMemoryBackingReleaseAlias_nid_postfix(alias); });
        });
    } catch (const std::exception& error) {
        static int reported = 0;
        if (reported++ < 4) APS5_LOG_OUT("host import of guest range 0x%llx+0x%llx failed (%s); copying its data", static_cast<unsigned long long>(first), static_cast<unsigned long long>(last - first), error.what());
        return {};
    }
    auto& mapping = hostMappings[first];
    if (mapping.buffer) hostBytes -= mapping.bytes;
    mapping = {extent.serial, last - first, buffer, ++hostUses};
    hostBytes += last - first;
    static int reported = 0;
    if (reported++ < 8 || last - first > HostWindowBytes) APS5_LOG_OUT("guest range 0x%llx+%.1f MiB imported for vertex, index and buffer data (request 0x%llx+0x%llx, %.1f MiB imported in all)", static_cast<unsigned long long>(first), (last - first) / 1048576.0, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), hostBytes / 1048576.0);
    trimHost();
    return {std::move(buffer), address - first};
}

GuestBufferCache::HostView GuestBufferCache::residentRange(std::uint64_t address, std::uint64_t bytes, const GuestMemoryBacking::GuestMemoryBackingExtentInfo& extent, std::uint64_t importBytes) {
    if (const auto generation = GuestMemoryBacking::GuestMemoryBackingUnmapGeneration_nid_postfix(); generation != residentUnmapGeneration) {
        residentUnmapGeneration = generation;
        for (auto it = residentWindows.begin(); it != residentWindows.end();) {
            GuestMemoryBacking::GuestMemoryBackingExtentInfo current{};
            const bool live = GuestMemoryBacking::GuestMemoryBackingExtent_nid_postfix(it->first, 1, &current) && current.serial == it->second.serial;
            if (live) ++it;
            else dropResidentWindow(it++);
        }
    }
    const auto slotFirst = extent.address + (address - extent.address) / HostWindowBytes * HostWindowBytes;
    const auto slotLast = std::min(extent.address + importBytes, extent.address + (address + bytes - extent.address + HostWindowBytes - 1) / HostWindowBytes * HostWindowBytes);
    if (address + bytes > slotLast) return {};
    auto it = residentWindows.upper_bound(address);
    if (it != residentWindows.begin()) --it;
    const bool contained = it != residentWindows.end() && it->second.serial == extent.serial && it->first <= address && address + bytes <= it->first + it->second.bytes;
    if (!contained) {
        static const std::uint64_t minimumBytes = [] {
            const char* value = std::getenv("ANYPS5_RESIDENT_MIN_KB");
            return value != nullptr ? std::strtoull(value, nullptr, 10) << 10 : std::uint64_t{4} << 20;
        }();
        if (bytes < minimumBytes) return {};
        auto newFirst = slotFirst;
        auto newLast = slotLast;
        for (bool grown = true; grown;) {
            grown = false;
            auto cursor = residentWindows.lower_bound(newFirst);
            if (cursor != residentWindows.begin()) --cursor;
            for (; cursor != residentWindows.end() && cursor->first < newLast; ++cursor) {
                if (cursor->first + cursor->second.bytes <= newFirst || cursor->second.serial != extent.serial) continue;
                if (cursor->second.buffer.use_count() > 1) return {};
                if (cursor->first < newFirst) {
                    newFirst = cursor->first;
                    grown = true;
                }
                if (cursor->first + cursor->second.bytes > newLast) {
                    newLast = cursor->first + cursor->second.bytes;
                    grown = true;
                }
            }
        }
        auto cursor = residentWindows.lower_bound(newFirst);
        if (cursor != residentWindows.begin()) --cursor;
        while (cursor != residentWindows.end() && cursor->first < newLast) {
            if (cursor->first + cursor->second.bytes <= newFirst) {
                ++cursor;
                continue;
            }
            dropResidentWindow(cursor++);
        }
        trimResident();
        std::shared_ptr<Buffer> buffer;
        try {
            const VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | (context.bufferDeviceAddress ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
            buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(newLast - newFirst), usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        } catch (const std::exception& error) {
            static int reported = 0;
            if (reported++ < 4) APS5_LOG_OUT("guest range 0x%llx+%.1f MiB cannot be made resident in GPU memory (%s); importing it instead", static_cast<unsigned long long>(newFirst), (newLast - newFirst) / 1048576.0, error.what());
            return {};
        }
        it = residentWindows.emplace(newFirst, ResidentWindow{extent.serial, newLast - newFirst, std::move(buffer), 0}).first;
        residentBytes += newLast - newFirst;
        static int reported = 0;
        if (reported++ < 12) APS5_LOG_OUT("guest range 0x%llx+%.1f MiB resident in GPU memory (request 0x%llx+0x%llx, %.1f MiB resident in all)", static_cast<unsigned long long>(newFirst), (newLast - newFirst) / 1048576.0, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), residentBytes / 1048576.0);
    }
    const auto first = it->first;
    auto& window = it->second;
    window.lastUse = ++hostUses;
    if (!syncResident(first, window, address, bytes)) return {};
    return {window.buffer, address - first, true};
}

bool GuestBufferCache::syncResident(std::uint64_t first, const ResidentWindow& window, std::uint64_t address, std::uint64_t bytes) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto protection = GuestAllocations::GuestAllocationsProtectionGeneration_nid_postfix();
    const auto end = address + bytes;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> stale;
    for (auto base = address - address % chunkBytes; base < end; base += chunkBytes) {
        auto& chunk = this->chunk(base);
        if (chunk.residentWindow == first && chunk.pendingWrites != 0) continue;
        protectChunk(chunk, now);
        const bool immutable = chunk.immutableSince != 0 && chunk.immutableSince == protection;
        const bool tracked = immutable || (!chunk.untrackable && !chunk.lost && chunk.protectedRead && chunk.generation <= chunk.residentSynced);
        if (chunk.residentWindow == first && chunk.residentSynced != 0 && tracked) continue;
        const auto from = std::max(base, first);
        const auto to = std::min(base + chunkBytes, first + window.bytes);
        if (from >= to) continue;
        if (context.drawQueue->WritesPending(from, static_cast<std::size_t>(to - from))) return false;
        if (!stale.empty() && stale.back().second == from) stale.back().second = to;
        else stale.emplace_back(from, to);
        chunk.residentWindow = first;
        chunk.residentSynced = chunk.untrackable ? 0 : generation;
        publish(chunk);
    }
    if (stale.empty()) return true;
    PerformanceTimer timing("Graphics.GuestBufferCache.Upload");
    const auto copyBuffer = context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
    for (const auto& [from, to] : stale) {
        const auto count = static_cast<std::size_t>(to - from);
        auto staging = std::make_shared<Buffer>(context, count, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        GuestMemory::Read(from, staging->Bytes());
        const auto commands = context.drawQueue->UploadCommands(context);
        const VkBufferCopy copy{0, from - first, count};
        copyBuffer(commands, staging->Handle(), window.buffer->Handle(), 1, &copy);
        context.drawQueue->EnqueueUpload([staging] {}, count);
        copiedBytes += count;
    }
    return true;
}

void GuestBufferCache::NoteResidentWritePending(std::uint64_t begin, std::uint64_t end) {
    if (begin >= end) return;
    std::lock_guard lock(mutex);
    for (auto it = chunks.lower_bound(begin - begin % chunkBytes); it != chunks.end() && it->first < end; ++it) ++it->second->pendingWrites;
}

void GuestBufferCache::NoteResidentWriteThrough(std::uint64_t begin, std::uint64_t end) {
    if (begin >= end) return;
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    for (auto it = chunks.lower_bound(begin - begin % chunkBytes); it != chunks.end() && it->first < end; ++it) {
        auto& chunk = *it->second;
        if (chunk.pendingWrites != 0) --chunk.pendingWrites;
        chunk.generation = ++generation;
        chunk.residentSynced = chunk.protectedRead && !chunk.untrackable && !chunk.lost ? generation : 0;
        publish(chunk);
    }
}

void GuestBufferCache::dropResidentWindow(std::map<std::uint64_t, ResidentWindow>::iterator window) {
    const auto first = window->first;
    const auto end = first + window->second.bytes;
    for (auto it = chunks.lower_bound(first - first % chunkBytes); it != chunks.end() && it->first < end; ++it) {
        auto& chunk = *it->second;
        if (chunk.residentWindow != first) continue;
        chunk.residentWindow = 0;
        chunk.residentSynced = 0;
        chunk.pendingWrites = 0;
        publish(chunk);
    }
    residentBytes -= window->second.bytes;
    residentWindows.erase(window);
}

void GuestBufferCache::trimResident() {
    while (residentBytes > residentBudget) {
        auto oldest = residentWindows.end();
        for (auto it = residentWindows.begin(); it != residentWindows.end(); ++it) {
            if (it->second.buffer.use_count() == 1 && (oldest == residentWindows.end() || it->second.lastUse < oldest->second.lastUse)) oldest = it;
        }
        if (oldest == residentWindows.end()) return;
        dropResidentWindow(oldest);
    }
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> GuestBufferCache::AddressWindows(std::uint64_t begin, std::uint64_t end, std::span<const std::pair<std::uint64_t, std::uint64_t>> required) {
    std::lock_guard lock(mutex);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> windows;
    for (auto it = addressWindows.lower_bound(begin - begin % HostWindowBytes); it != addressWindows.end() && *it < end; ++it) windows.emplace_back(*it, *it + HostWindowBytes);
    for (const auto& [first, last] : required) {
        if (last <= begin || end <= first) continue;
        windows.emplace_back(first - first % HostWindowBytes, (last + HostWindowBytes - 1) / HostWindowBytes * HostWindowBytes);
    }
    std::sort(windows.begin(), windows.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    for (const auto& window : windows) {
        const auto first = std::max(window.first, begin);
        const auto last = std::min(window.second, end);
        if (first >= last) continue;
        if (!merged.empty() && first <= merged.back().second) merged.back().second = std::max(merged.back().second, last);
        else merged.emplace_back(first, last);
    }
    return merged;
}

bool GuestBufferCache::LearnAddress(std::uint64_t address) {
    std::lock_guard lock(mutex);
    return addressWindows.insert(address - address % HostWindowBytes).second;
}

void GuestBufferCache::trimHost() {
    while (hostBytes > HostBudgetBytes) {
        auto oldest = hostMappings.end();
        for (auto it = hostMappings.begin(); it != hostMappings.end(); ++it) {
            if (oldest == hostMappings.end() || it->second.lastUse < oldest->second.lastUse) oldest = it;
        }
        if (oldest == hostMappings.end() || oldest->second.lastUse == hostUses) return;
        hostBytes -= oldest->second.bytes;
        hostMappings.erase(oldest);
    }
}

std::shared_ptr<Buffer> GuestBufferCache::ImageCopy(const std::shared_ptr<const GuestAllocations::Range>& range, std::uint64_t padding, VkBufferUsageFlags usage) {
    {
        std::lock_guard lock(mutex);
        const auto found = imageCopies.find(range.get());
        if (found != imageCopies.end() && (found->second.second->Usage() & usage) == usage) return found->second.second;
    }
    auto buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(range->bytes + padding), usage);
    GuestMemory::Read(range->address - padding, buffer->Bytes());
    std::lock_guard lock(mutex);
    if (imageCopies.size() >= 64) imageCopies.clear();
    imageCopies[range.get()] = {range, buffer};
    return buffer;
}

void GuestBufferCache::MarkSynced(const std::shared_ptr<Mirror>& mirror) {
    if (!mirror || mirror->imported) return;
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
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
        publish(chunk);
    }
}

std::uint64_t GuestBufferCache::Track(std::uint64_t begin, std::uint64_t end) {
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    ageChunks();
    Mirror range{begin, end, nullptr, 0, 0};
    protect(range);
    return generation;
}

void GuestBufferCache::NoteGpuWrite(std::uint64_t begin, std::uint64_t end) {
    if (begin >= end) return;
    std::lock_guard lock(mutex);
    for (auto it = chunks.lower_bound(begin - begin % chunkBytes); it != chunks.end() && it->first < end; ++it) {
        auto& chunk = *it->second;
        chunk.generation = ++generation;
        publish(chunk);
    }
}

GuestBufferCache::RangeState GuestBufferCache::Check(std::uint64_t begin, std::uint64_t end, std::uint64_t stamp) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    if (now - swept.load(std::memory_order_relaxed) >= Ticks(ChunkSweep)) {
        std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
        std::lock_guard lock(mutex);
        ageChunks();
    }
    const auto protection = GuestAllocations::GuestAllocationsProtectionGeneration_nid_postfix();
    auto result = RangeState::Current;
    for (auto address = begin - begin % chunkBytes; address < end; address += chunkBytes) {
        auto* entry = stateOf(address);
        if (entry == nullptr) return RangeState::Untracked;
        const auto state = entry->state.load(std::memory_order_acquire);
        if (state == 0) return RangeState::Untracked;
        touch(*entry, now);
        const auto immutableSince = entry->immutableSince.load(std::memory_order_relaxed);
        if (immutableSince != 0 && immutableSince == protection) continue;
        if ((state & (UntrackableState | LostState)) != 0) return RangeState::Untracked;
        if ((state & ProtectedState) == 0 && (state & WrittenState) != 0) result = RangeState::Written;
        else if (result == RangeState::Current && ((state & ProtectedState) == 0 || (state >> StateFlagBits) > stamp)) result = RangeState::Changed;
    }
    return result;
}

std::string GuestBufferCache::Describe(std::uint64_t begin, std::uint64_t end, std::uint64_t stamp) {
    std::lock_guard registryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::lock_guard lock(mutex);
    std::string result;
    for (auto address = begin - begin % chunkBytes; address < end; address += chunkBytes) {
        const auto it = chunks.find(address);
        char text[112];
        if (it == chunks.end()) { std::snprintf(text, sizeof(text), " 0x%llx:none", static_cast<unsigned long long>(address)); result += text; continue; }
        const auto& chunk = *it->second;
        std::snprintf(text, sizeof(text), " 0x%llx:%s%c%s%s%s%s", static_cast<unsigned long long>(address), chunk.untrackable ? "untrackable" : "", chunk.untrackable ? chunk.untrackableReason : ' ', chunk.lost ? "lost" : "", chunk.protectedRead ? "" : "unprotected", chunk.written ? "written" : "", chunk.generation > stamp ? "aged" : "");
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
    std::vector<decltype(mirrors)::iterator> candidates;
    for (auto it = mirrors.begin(); it != mirrors.end(); ++it) if (it->second && !it->second->imported && it->second.use_count() == 1 && it->second->lastUse + 256 < uses) candidates.push_back(it);
    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) { return left->second->lastUse < right->second->lastUse; });
    for (const auto& it : candidates) {
        if (retainedBytes <= budget) break;
        retainedBytes -= it->second->end - it->second->begin;
        mirrors.erase(it);
    }
}

}

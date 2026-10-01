#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERCACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERCACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <span>
#include <vector>
#include <string>

namespace AgcDriver::Graphics {

bool TextureMemcmp();

class GuestBufferCache {
public:
    struct Mirror {
        std::uint64_t begin;
        std::uint64_t end;
        std::shared_ptr<Buffer> buffer;
        std::uint64_t synced = 0;
        std::uint64_t lastUse = 0;
        bool imported = false;
    };
    enum class RangeState { Current, Changed, Written, Untracked };

    explicit GuestBufferCache(const Context& context);
    ~GuestBufferCache();
    GuestBufferCache(const GuestBufferCache&) = delete;
    GuestBufferCache& operator=(const GuestBufferCache&) = delete;

    std::shared_ptr<Mirror> Acquire(std::uint64_t begin, std::uint64_t end, VkBufferUsageFlags usage);
    void MarkSynced(const std::shared_ptr<Mirror>& mirror);
    struct HostView {
        std::shared_ptr<Buffer> buffer;
        VkDeviceSize offset = 0;
        bool resident = false;
    };
    HostView HostRange(std::uint64_t address, std::uint64_t bytes, bool writable = false);
    bool HostImportable(std::uint64_t address, std::uint64_t bytes) const;
    static bool ResidentEnabled();
    static void SetResidentEnabled(bool enabled);
    bool ResidentDirty(std::uint64_t address, std::uint64_t bytes) const;
    void WriteBackResident(std::uint64_t address, std::uint64_t bytes);
    void NoteResidentWrite(std::uint64_t begin, std::uint64_t end);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> AddressWindows(std::uint64_t begin, std::uint64_t end, std::span<const std::pair<std::uint64_t, std::uint64_t>> required);
    bool LearnAddress(std::uint64_t address);
    std::shared_ptr<Buffer> ImageCopy(const std::shared_ptr<const GuestAllocations::Range>& range, std::uint64_t padding, VkBufferUsageFlags usage);
    void ReleaseTracking(std::uint64_t address, std::size_t bytes);
    void Flush();
    std::uint64_t Track(std::uint64_t begin, std::uint64_t end);
    void NoteGpuWrite(std::uint64_t begin, std::uint64_t end);
    RangeState Check(std::uint64_t begin, std::uint64_t end, std::uint64_t stamp);
    bool Current(std::uint64_t begin, std::uint64_t end, std::uint64_t stamp) { return Check(begin, end, stamp) == RangeState::Current; }
    std::string Describe(std::uint64_t begin, std::uint64_t end, std::uint64_t stamp);

private:
    struct Owner {
        GuestBufferCache* cache;
        std::uint64_t address;
    };
    struct ChunkState {
        std::atomic<std::uint64_t> state{0};
        std::atomic<std::uint64_t> immutableSince{0};
        std::atomic<std::chrono::steady_clock::rep> touched{0};
    };
    struct Chunk {
        std::uint64_t address;
        std::unique_ptr<Owner> owner;
        std::unique_ptr<GuestMemoryTracking::Watch> watch;
        ChunkState* published = nullptr;
        std::uint64_t generation = 0;
        bool protectedRead = false;
        bool untrackable = false;
        char untrackableReason = 0;
        std::uint64_t immutableSince = 0;
        bool lost = false;
        bool written = false;
        std::uint64_t residentWindow = 0;
        std::uint64_t residentSynced = 0;
        bool gpuDirty = false;
    };
    struct ResidentWindow {
        std::uint64_t serial;
        std::uint64_t bytes;
        std::shared_ptr<Buffer> buffer;
        std::uint64_t lastUse = 0;
    };
    static constexpr std::uint64_t chunkBytes = 1u << 16;
    static constexpr unsigned StateBlockShift = 30;
    static constexpr std::uint64_t StateLimit = std::uint64_t{1} << 48;
    static constexpr std::uint64_t StateBlockChunks = (std::uint64_t{1} << StateBlockShift) / chunkBytes;
    static constexpr unsigned StateFlagBits = 5;
    static constexpr std::uint64_t ProtectedState = 1;
    static constexpr std::uint64_t LostState = 2;
    static constexpr std::uint64_t UntrackableState = 4;
    static constexpr std::uint64_t WrittenState = 8;
    static constexpr std::uint64_t GpuDirtyState = 16;
    static constexpr std::chrono::milliseconds ChunkAge{250};
    static constexpr std::chrono::milliseconds ChunkSweep{100};
    static constexpr std::chrono::milliseconds TouchGranularity{1};
    void ageChunks();
    std::atomic<std::chrono::steady_clock::rep> swept{0};
    static constexpr std::uint64_t IncrementalBytes = 16ull << 20;
    static constexpr std::uint64_t ImportBytes = 1ull << 20;
    std::shared_ptr<Mirror> import(std::uint64_t begin, std::uint64_t end, VkBufferUsageFlags usage);
    Chunk& chunk(std::uint64_t address);
    bool current(const Mirror& mirror);
    void protect(const Mirror& mirror);
    void protectChunk(Chunk& chunk, std::chrono::steady_clock::rep now);
    void trim();
    static void resolve(void* owner, GuestMemoryTracking::Access access);
    ChunkState* stateOf(std::uint64_t address) const;
    ChunkState* createState(std::uint64_t address);
    static void publish(const Chunk& chunk);
    static void touch(ChunkState& state, std::chrono::steady_clock::rep now);

    Context context;
    mutable std::recursive_mutex mutex;
    std::unique_ptr<std::atomic<ChunkState*>[]> stateBlocks;
    std::vector<std::unique_ptr<ChunkState[]>> stateStorage;
    std::map<std::uint64_t, std::unique_ptr<Chunk>> chunks;
    std::map<std::pair<std::uint64_t, std::uint64_t>, std::shared_ptr<Mirror>> mirrors;
    std::uint64_t generation = 1;
    std::uint64_t uses = 0;
    std::uint64_t retainedBytes = 0;
    std::uint64_t budget;
    struct HostMapping {
        std::uint64_t serial;
        std::uint64_t bytes;
        std::shared_ptr<Buffer> buffer;
        std::uint64_t lastUse = 0;
    };
    static constexpr std::uint64_t HostWindowBytes = 32ull << 20;
    static constexpr std::uint64_t HostBudgetBytes = 2ull << 30;
    void trimHost();
    bool hostExtent(std::uint64_t address, std::uint64_t bytes, GuestMemoryBacking::GuestMemoryBackingExtentInfo& extent, std::uint64_t& importBytes) const;
    HostView residentRange(std::uint64_t address, std::uint64_t bytes, bool writable, const GuestMemoryBacking::GuestMemoryBackingExtentInfo& extent, std::uint64_t importBytes);
    bool residentWritable(std::uint64_t address, std::uint64_t bytes);
    void syncResident(std::uint64_t first, const ResidentWindow& window, std::uint64_t address, std::uint64_t bytes);
    void writeBackChunk(Chunk& chunk);
    void dropResidentWindow(std::map<std::uint64_t, ResidentWindow>::iterator window);
    void trimResident();
    void addDirtyRange(std::uint64_t begin, std::uint64_t end);
    void removeDirtyRange(std::uint64_t begin, std::uint64_t end);
    bool dirtyOverlaps(std::uint64_t begin, std::uint64_t end) const;
    std::map<std::uint64_t, HostMapping> hostMappings;
    std::uint64_t hostBytes = 0;
    std::uint64_t hostUses = 0;
    std::map<std::uint64_t, ResidentWindow> residentWindows;
    std::uint64_t residentBytes = 0;
    std::uint64_t residentBudget;
    std::uint64_t residentUnmapGeneration = 0;
    std::atomic<std::uint64_t> dirtyChunks{0};
    std::map<std::uint64_t, std::uint64_t> residentDirty;
    std::set<std::uint64_t> addressWindows;
    std::uint64_t unmapGeneration = 0;
    std::map<const GuestAllocations::Range*, std::pair<std::shared_ptr<const GuestAllocations::Range>, std::shared_ptr<Buffer>>> imageCopies;
    std::uint64_t copiedBytes = 0;
    std::uint64_t reusedBytes = 0;
};

}

#endif

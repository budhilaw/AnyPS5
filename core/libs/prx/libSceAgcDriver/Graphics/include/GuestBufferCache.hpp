#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERCACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERCACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <string>

namespace AgcDriver::Graphics {

// GPU mirrors of guest buffer ranges that outlive one submission. A title binds the same
// (large) buffers every frame; copying them to the GPU each time dominated frame time. Guest
// memory is watched in fixed chunks: a CPU write to a chunk (a page fault, or the driver's own
// write-back) bumps the chunk's generation, and a mirror is current while every chunk it spans is
// older than its last synchronization. Chunks that cannot be watched (they share pages with a
// render target, or are unmapped) count as always written, so mirrors there copy every time.
class GuestBufferCache {
public:
    struct Mirror {
        std::uint64_t begin;
        std::uint64_t end;
        std::shared_ptr<Buffer> buffer;
        std::uint64_t synced = 0;  // generation at the last synchronization with guest memory
        std::uint64_t lastUse = 0;
        // Backed by guest memory itself (host import): always current, never watched or copied.
        bool imported = false;
    };

    explicit GuestBufferCache(const Context& context);
    ~GuestBufferCache();
    GuestBufferCache(const GuestBufferCache&) = delete;
    GuestBufferCache& operator=(const GuestBufferCache&) = delete;

    // The mirror of [begin, end) with current contents (copied from guest memory when needed).
    // A mirror another submission may still be executing with is replaced rather than rewritten.
    std::shared_ptr<Mirror> Acquire(std::uint64_t begin, std::uint64_t end, VkBufferUsageFlags usage);
    // The mirror's contents were just written back to guest memory: it is current again, and the
    // other mirrors over the same memory are not.
    void MarkSynced(const std::shared_ptr<Mirror>& mirror);
    // A buffer over the whole host-imported guest mapping holding [address, address + bytes) and
    // the offset of `address` in it: the GPU reads guest memory in place, nothing is copied. Null
    // when the device cannot import host memory or the range has no shared backing.
    struct HostView {
        std::shared_ptr<Buffer> buffer;
        VkDeviceSize offset = 0;
    };
    HostView HostRange(std::uint64_t address, std::uint64_t bytes);
    // A GPU copy of read-only image data (code, constants), made once while `range` stays registered.
    std::shared_ptr<Buffer> ImageCopy(const std::shared_ptr<const GuestAllocations::Range>& range, std::uint64_t padding, VkBufferUsageFlags usage);
    // Stops watching pages in [address, address + bytes): another owner (a render target) takes
    // them over. Mirrors there copy on every use afterwards.
    void ReleaseTracking(std::uint64_t address, std::size_t bytes);
    void Flush();
    // Change tracking for other guest-memory consumers (textures): Track protects the chunks of
    // [begin, end) and returns a stamp; Current tells whether no CPU write reached them since.
    std::uint64_t Track(std::uint64_t begin, std::uint64_t end);
    bool Current(std::uint64_t begin, std::uint64_t end, std::uint64_t stamp);
    // Why [begin, end) is not current for `stamp` (diagnostics).
    std::string Describe(std::uint64_t begin, std::uint64_t end, std::uint64_t stamp);

private:
    // The watch entry the tracking registry calls back with: it names the cache and the chunk.
    struct Owner {
        GuestBufferCache* cache;
        std::uint64_t address;
    };
    struct Chunk {
        std::uint64_t address;
        std::unique_ptr<Owner> owner;
        std::unique_ptr<GuestMemoryTracking::Watch> watch;
        std::uint64_t generation = 0;
        bool protectedRead = false;
        bool untrackable = false;
        char untrackableReason = 0;  // 'r' released to a render target, 'w' watch creation failed
        // Mapped read-only for the CPU: nothing can change it while the mapping's protection
        // stays (the protection generation it was seen with).
        std::uint64_t immutableSince = 0;
        bool lost = false;  // the watch was invalidated (memory released): recreate before use
    };
    // Small chunks keep an unmapped or render-target page from making a whole texture untrackable.
    static constexpr std::uint64_t chunkBytes = 1u << 16;
    // Mirrors from this size on are refreshed chunk by chunk, in place (see Acquire).
    static constexpr std::uint64_t IncrementalBytes = 16ull << 20;
    // Ranges from this size on are bound in place when the device imports host memory.
    static constexpr std::uint64_t ImportBytes = 1ull << 20;
    std::shared_ptr<Mirror> import(std::uint64_t begin, std::uint64_t end, VkBufferUsageFlags usage);
    Chunk& chunk(std::uint64_t address);
    bool current(const Mirror& mirror);
    void protect(const Mirror& mirror);
    void trim();
    static void resolve(void* owner, GuestMemoryTracking::Access access);

    Context context;
    std::recursive_mutex mutex;
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
    };
    std::map<std::uint64_t, HostMapping> hostMappings;  // keyed by guest mapping address
    std::map<const GuestAllocations::Range*, std::pair<std::shared_ptr<const GuestAllocations::Range>, std::shared_ptr<Buffer>>> imageCopies;
    std::uint64_t copiedBytes = 0;
    std::uint64_t reusedBytes = 0;
};

}

#endif

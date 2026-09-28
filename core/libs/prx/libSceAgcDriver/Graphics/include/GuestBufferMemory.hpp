#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERMEMORY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "BdaAbi.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferCache.hpp"
#include <memory>
#include <limits>
#include <vector>

namespace AgcDriver::Graphics {

struct GuestMemorySnapshot {
    std::uint64_t address;
    std::span<const std::byte> bytes;
};

class GuestBufferMemory {
public:
    // Guest buffer views start at addresses aligned to this, which covers every Vulkan device's
    // storage buffer offset alignment (at most 256); the shader adds the residue itself.
    static constexpr std::uint64_t ViewAlignment = 256;
    explicit GuestBufferMemory(const Context& context);
    void AcquireRegistered();
    void AddWritable(std::uint64_t address, std::size_t bytes);
    // A buffer the shader only reads: uploaded from guest memory, never written back.
    void AddReadOnly(std::uint64_t address, std::size_t bytes);
    void AddSnapshot(const GuestMemorySnapshot& snapshot);
    void Upload(bool addressable);
    VkDescriptorBufferInfo Descriptor(std::uint64_t address, std::size_t bytes) const;
    std::vector<ShaderRecompiler::BdaAbi::Range> AddressRanges() const;
    // `sequence` orders the work in the draw queue: resident render targets the GPU refreshed
    // from guest memory after it already hold what it wrote in place (their watch is skipped).
    void WriteBack(std::uint64_t sequence = std::numeric_limits<std::uint64_t>::max());
    // Whether writes the host copies back after completion (not in place) overlap the range.
    bool WritesOverlapCopied(std::uint64_t address, std::size_t bytes) const;
    bool WritesOverlap(std::uint64_t address, std::size_t bytes) const;
    bool HasWrites() const { return !writes.empty(); }
    const std::vector<std::pair<std::uint64_t, std::uint64_t>>& WriteRanges() const { return writes; }

private:
    struct Region {
        std::uint64_t begin;
        std::uint64_t end;
        bool writable;
        bool fromGuest = false;  // read-only contents come from guest memory (no snapshot)
        std::vector<std::byte> snapshot;
        std::shared_ptr<Buffer> buffer;
        std::shared_ptr<GuestBufferCache::Mirror> mirror;  // set when the buffer is a cached mirror
        // The buffer starts `padding` bytes before `begin` so that views taken from 256-byte
        // aligned guest addresses satisfy any storage buffer offset alignment.
        std::uint64_t padding = 0;
        // Where `begin - padding` lies in `buffer` (a whole imported guest mapping is shared).
        std::uint64_t bufferOffset = 0;
        bool inPlace = false;  // the buffer is guest memory itself (no copy in either direction)
        std::shared_ptr<const GuestAllocations::Range> image;  // read-only image data, copied once per registration
    };

    void validate(std::uint64_t address, std::size_t bytes) const;
    Context context;
    GuestAllocations::Lease lease;
    std::vector<Region> regions;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
    struct Access {
        std::uint64_t address;
        std::size_t bytes;
        bool writable;
    };
    std::vector<Access> accesses;  // guest ranges added, resolved for the device at upload
    bool uploaded = false;
    bool committed = false;
};

}

#endif

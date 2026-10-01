#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERMEMORY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DispatchBindings.hpp"
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
    static constexpr std::uint64_t ViewAlignment = 256;
    explicit GuestBufferMemory(const Context& context);
    void AcquireRegistered(std::span<const GuestMemorySnapshot> snapshots);
    bool LearnAddress(std::uint64_t address) const;
    void AddWritable(std::uint64_t address, std::size_t bytes, bool checked = false);
    void AddReadOnly(std::uint64_t address, std::size_t bytes, bool checked = false);
    bool OverlapsImmutable(std::uint64_t address, std::uint64_t bytes) const;
    void AddSnapshot(const GuestMemorySnapshot& snapshot);
    void Upload(bool addressable);
    VkDescriptorBufferInfo Descriptor(std::uint64_t address, std::size_t bytes) const;
    std::vector<ShaderRecompiler::BdaAbi::Range> AddressRanges() const;
    std::vector<DispatchBindings::BoundRegion> CaptureRegions() const;
    void WriteBack(std::uint64_t sequence = std::numeric_limits<std::uint64_t>::max());
    bool WritesOverlapCopied(std::uint64_t address, std::size_t bytes) const;
    bool WriteCopied(std::uint64_t address) const;
    bool WritesOverlap(std::uint64_t address, std::size_t bytes) const;
    bool HasWrites() const { return !writes.empty(); }
    const std::vector<std::pair<std::uint64_t, std::uint64_t>>& WriteRanges() const { return writes; }

private:
    struct Region {
        std::uint64_t begin;
        std::uint64_t end;
        bool writable;
        bool fromGuest = false;
        std::vector<std::byte> snapshot;
        std::shared_ptr<Buffer> buffer;
        std::shared_ptr<GuestBufferCache::Mirror> mirror;
        std::uint64_t padding = 0;
        std::uint64_t bufferOffset = 0;
        bool inPlace = false;
        std::shared_ptr<const GuestAllocations::Range> image;
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
    std::vector<Access> accesses;
    bool uploaded = false;
    bool committed = false;
};

}

#endif

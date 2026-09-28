#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERMEMORY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "BdaAbi.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferCache.hpp"
#include <memory>
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
    void WriteBack();
    bool WritesOverlap(std::uint64_t address, std::size_t bytes) const;

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
    };

    void validate(std::uint64_t address, std::size_t bytes) const;
    Context context;
    GuestAllocations::Lease lease;
    std::vector<Region> regions;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
    bool uploaded = false;
    bool committed = false;
};

}

#endif

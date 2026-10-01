#include "BdaTests.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/WriteRanges.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include <cstring>
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

template<typename TAction>
void reject(TAction action) {
    try { action(); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("expected guest allocation ownership rejection");
}

template<typename TAction>
bool throws(TAction action) {
    try { action(); }
    catch (const std::runtime_error&) { return true; }
    return false;
}

struct WatchProbe {
    GuestMemoryTracking::Watch* watch = nullptr;
    int resolves = 0;
};

void releaseWatch(void* context, GuestMemoryTracking::Access) {
    auto& probe = *static_cast<WatchProbe*>(context);
    ++probe.resolves;
    probe.watch->Protect(GuestMemoryTracking::Protection::ReadWrite);
}

struct CoverageProbe {
    GuestMemoryTracking::Watch* watch = nullptr;
    std::uint64_t address = 0;
    std::size_t bytes = 0;
    int queries = 0;
};

void queryCoverage(void* context, GuestMemoryTracking::Access) {
    auto& probe = *static_cast<CoverageProbe*>(context);
    ++probe.queries;
    static_cast<void>(GuestAllocations::GuestAllocationsCovers_nid_postfix(probe.address, probe.bytes, false));
    probe.watch->Protect(GuestMemoryTracking::Protection::ReadWrite);
}

template<typename TAction>
void requireWithoutTrackingLock(TAction action, const char* failure) {
    std::promise<void> locked;
    std::promise<void> finished;
    auto lockedFuture = locked.get_future();
    auto finishedFuture = finished.get_future();
    std::atomic<bool> released{false};
    std::thread holder([&] {
        std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
        locked.set_value();
        finishedFuture.wait_for(std::chrono::seconds(10));
        released = true;
    });
    lockedFuture.wait();
    try {
        action();
    } catch (...) {
        finished.set_value();
        holder.join();
        throw;
    }
    const bool waited = released;
    finished.set_value();
    holder.join();
    Require(!waited, failure);
}

std::byte* mapBacking(std::size_t bytes, int protection) {
    return static_cast<std::byte*>(GuestMemoryBacking::GuestMemoryBackingMap_nid_postfix(nullptr, bytes, 1u << 16, protection));
}

std::uint64_t addressOf(const std::byte* pointer) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

void unmapRegistered(std::byte* pointer, std::size_t bytes) {
    GuestAllocations::Mutation mutation;
    mutation.Unmap(pointer, bytes, [&](const void*, bool) { GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(pointer, bytes); });
}

void gpuRangeTests() {
    using AgcDriver::GuestMemory::CheckGpuRange;
    using AgcDriver::GuestMemory::MappedGpuBytes;
    using GuestMemoryTracking::Protection;
    using GuestMemoryTracking::Watch;
    constexpr std::size_t region = 1u << 18;
    auto* writable = mapBacking(region, 3);
    auto* readOnly = mapBacking(region, 3);
    auto* unregistered = mapBacking(region, 3);
    auto* unregisteredReadOnly = mapBacking(region, 1);
    auto* inaccessible = mapBacking(region, 0);
    auto* partial = mapBacking(2 * region, 3);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(writable, region, true, true);
        mutation.Add(readOnly, region, true, false);
        mutation.Add(partial, 2 * region, true, true);
        mutation.Unmap(partial + region, region, [&](const void*, bool) { GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(partial + region, region); });
    }
    Require(GuestAllocations::GuestAllocationsCovers_nid_postfix(addressOf(writable), region, true) && !GuestAllocations::GuestAllocationsCovers_nid_postfix(addressOf(unregistered), region, false), "GPU range test memory is not registered as expected");
    {
        WatchProbe writableProbe;
        WatchProbe readOnlyProbe;
        WatchProbe unregisteredProbe;
        Watch writableWatch(addressOf(writable), region, &writableProbe, &releaseWatch);
        Watch readOnlyWatch(addressOf(readOnly), region, &readOnlyProbe, &releaseWatch);
        Watch unregisteredWatch(addressOf(unregistered), region, &unregisteredProbe, &releaseWatch);
        writableProbe.watch = &writableWatch;
        readOnlyProbe.watch = &readOnlyWatch;
        unregisteredProbe.watch = &unregisteredWatch;
        writableWatch.Protect(Protection::None);
        readOnlyWatch.Protect(Protection::None);
        unregisteredWatch.Protect(Protection::None);
        requireWithoutTrackingLock([&] { CheckGpuRange(writable, region, 256, true); }, "a GPU range check of registered writable memory under a protected watch waited for the tracking mutex");
        requireWithoutTrackingLock([&] { CheckGpuRange(readOnly, region, 256, false); }, "a GPU read range check of registered read-only memory under a protected watch waited for the tracking mutex");
        requireWithoutTrackingLock([&] {
            Require(throws([&] { CheckGpuRange(readOnly, region, 256, true); }), "a writable GPU range check accepted registered read-only memory under a protected watch");
        }, "a writable GPU range check of registered read-only memory waited for the tracking mutex");
        CheckGpuRange(unregistered, region, 256, true);
        Require(writableProbe.resolves == 0 && readOnlyProbe.resolves == 0 && unregisteredProbe.resolves == 0, "a GPU range check resolved a guest memory watch");
    }
    CheckGpuRange(unregisteredReadOnly, region, 256, false);
    Require(throws([&] { CheckGpuRange(unregisteredReadOnly, region, 256, true); }), "a writable GPU range check accepted unregistered read-only memory");
    Require(throws([&] { CheckGpuRange(inaccessible, region, 256, false); }), "a GPU range check accepted unregistered inaccessible memory");
    Require(throws([&] { CheckGpuRange(partial, 2 * region, 256, true); }), "a GPU range check accepted a partially unmapped range");
    Require(MappedGpuBytes(partial, 2 * region, true) == region, "the mapped prefix of a partially unmapped range is wrong");
    requireWithoutTrackingLock([&] {
        Require(MappedGpuBytes(partial, 2 * region, true) == region, "a memoized mapped prefix changed");
    }, "a memoized mapped prefix waited for the tracking mutex");
    {
        GuestAllocations::Mutation mutation;
        mutation.Protect(partial, region, true, false, [&] { GuestMemoryBacking::GuestMemoryBackingActivate_nid_postfix(addressOf(partial), region, 1); });
        mutation.Protect(writable, region, true, false, [&] { GuestMemoryBacking::GuestMemoryBackingActivate_nid_postfix(addressOf(writable), region, 1); });
    }
    Require(MappedGpuBytes(partial, 2 * region, true) == 0, "a memoized mapped prefix survived a protection change");
    Require(MappedGpuBytes(partial, 2 * region, false) == region, "the readable prefix of a partially unmapped range is wrong");
    Require(throws([&] { CheckGpuRange(writable, region, 256, true); }), "a cached GPU range verdict survived a protection change");
    CheckGpuRange(writable, region, 256, false);
    unmapRegistered(writable, region);
    unmapRegistered(readOnly, region);
    unmapRegistered(partial, region);
    Require(!GuestAllocations::GuestAllocationsCovers_nid_postfix(addressOf(writable), 1, false) && !GuestAllocations::GuestAllocationsCovers_nid_postfix(addressOf(readOnly), 1, false) && !GuestAllocations::GuestAllocationsCovers_nid_postfix(addressOf(partial), 1, false), "GPU range test memory remains registered");
    for (auto* pointer : {unregistered, unregisteredReadOnly, inaccessible}) GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(pointer, region);
}

void coverageDuringMutationTests() {
    using AgcDriver::GuestMemory::CheckGpuRange;
    constexpr std::size_t region = 1u << 18;
    auto* memory = mapBacking(region, 3);
    CoverageProbe probe{nullptr, addressOf(memory), region};
    const auto covers = [&](bool writable) { return GuestAllocations::GuestAllocationsCovers_nid_postfix(probe.address, region, writable); };
    const auto change = [&](auto mutate) {
        const auto queries = probe.queries;
        mutate();
        Require(probe.queries > queries, "a guest mapping change did not query the registry while it invalidated watched memory");
    };
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(memory, region, true, false);
    }
    GuestMemoryTracking::Watch watch(probe.address, region, &probe, &queryCoverage);
    probe.watch = &watch;
    Require(throws([&] { CheckGpuRange(memory, region, 256, true); }), "a writable GPU range check accepted registered read-only memory");
    change([&] {
        GuestAllocations::Mutation mutation;
        mutation.Protect(memory, region, true, true, [&] { GuestMemoryBacking::GuestMemoryBackingActivate_nid_postfix(probe.address, region, 3); });
    });
    Require(covers(true) && !throws([&] { CheckGpuRange(memory, region, 256, true); }), "a registry snapshot built while memory was made writable outlived the protection change");
    change([&] {
        GuestAllocations::Mutation mutation;
        mutation.Remove(memory);
    });
    Require(!covers(false), "a registry snapshot built while an allocation was removed outlived the removal");
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(memory, region, true, true);
    }
    change([&] { unmapRegistered(memory, region); });
    Require(!covers(false) && throws([&] { CheckGpuRange(memory, region, 256, false); }), "a registry snapshot built while memory was unmapped outlived the unmap");
}

void extentTests() {
    constexpr std::size_t region = 1u << 18;
    auto* memory = mapBacking(4 * region, 3);
    const auto base = addressOf(memory);
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    const auto extent = [&](std::uint64_t address) {
        begin = 0;
        end = 0;
        return GuestAllocations::GuestAllocationsExtent_nid_postfix(address, &begin, &end);
    };
    Require(!extent(base) && !extent(base + 3 * region), "unregistered memory has a guest allocation extent");
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(memory, region, true, true);
        mutation.Add(memory + region, region, true, true);
        mutation.Add(memory + 3 * region, region, true, false);
    }
    Require(extent(base) && begin == base && end == base + 2 * region, "adjacent guest allocations do not form one extent");
    Require(extent(base + 2 * region - 1) && begin == base && end == base + 2 * region, "the last byte of adjacent guest allocations is outside their extent");
    Require(!extent(base + 2 * region) && !extent(base + 3 * region - 1), "unregistered memory between guest allocations has an extent");
    Require(extent(base + 3 * region + 16) && begin == base + 3 * region && end == base + 4 * region, "a read-only guest allocation after a gap has a wrong extent");
    Require(!extent(base + 4 * region), "memory after the last guest allocation has an extent");
    {
        GuestAllocations::Mutation mutation;
        mutation.Protect(memory + region / 2, region, true, false, [&] { GuestMemoryBacking::GuestMemoryBackingActivate_nid_postfix(base + region / 2, region, 1); });
    }
    Require(extent(base + region) && begin == base && end == base + 2 * region, "a protection change split the extent of the allocations it covers");
    unmapRegistered(memory + region, region);
    Require(extent(base) && begin == base && end == base + region, "an unmapped guest allocation remained in the extent of its neighbour");
    Require(!extent(base + region), "an unmapped guest allocation still has an extent");
    unmapRegistered(memory, region);
    unmapRegistered(memory + 3 * region, region);
    Require(!extent(base) && !extent(base + 3 * region), "unmapped guest allocations still have an extent");
    GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(memory + 2 * region, region);
    bool rejected = false;
    try {
        GuestAllocations::GuestAllocationsExtent_nid_postfix(base, nullptr, &end);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "a guest allocation extent query without a result was accepted");
}

void writeRangeTests() {
    using AgcDriver::WriteOrigin;
    using AgcDriver::WriteRange;
    using ShaderRecompiler::DescriptorBinding;
    constexpr std::size_t region = 1u << 18;
    auto* memory = mapBacking(4 * region, 3);
    const auto base = addressOf(memory);
    const auto runEnd = base + 2 * region;
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(memory, region, true, true);
        mutation.Add(memory + region, region, true, true);
    }
    const auto buffer = [](std::uint64_t address, std::uint32_t stride, std::uint32_t records, bool optional) {
        DescriptorBinding binding{};
        binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
        binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
        binding.count = 1;
        binding.guestDescriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (stride << 16u), records, 0u};
        binding.elementWritten = {true};
        binding.elementOptional = {optional};
        return binding;
    };
    const auto image = [](std::uint64_t address, std::uint32_t tileMode, bool readOnly) {
        constexpr std::uint32_t side = 64;
        const auto base40 = address >> 8u;
        DescriptorBinding binding{};
        binding.kind = ShaderRecompiler::DescriptorKind::StorageImage;
        binding.role = ShaderRecompiler::DescriptorRole::GuestImages;
        binding.count = 1;
        binding.readOnly = readOnly;
        binding.guestDescriptor = {
            static_cast<std::uint32_t>(base40),
            static_cast<std::uint32_t>((base40 >> 32u) & 0xffu) | (56u << 20u) | (((side - 1u) & 3u) << 30u),
            ((side - 1u) >> 2u) | ((side - 1u) << 14u),
            0xfacu | (tileMode << 20u) | (9u << 28u),
            0u, 0u, 0u, 0u
        };
        return binding;
    };
    bool unknown = false;
    const auto collect = [&](const std::vector<DescriptorBinding>& bindings) {
        std::vector<WriteRange> writes;
        unknown = false;
        AgcDriver::CollectWrites(bindings, writes, unknown);
        return writes;
    };
    const auto declares = [&](const std::vector<WriteRange>& writes, std::uint64_t first, std::uint64_t last, WriteOrigin origin) {
        return !unknown && writes.size() == 1 && writes[0].first == first && writes[0].last == last && writes[0].origin == origin;
    };
    Require(declares(collect({buffer(base + 0x100, 0, 0xffffffffu, false)}), base + 0x100, runEnd, WriteOrigin::BufferElement), "a raw written buffer was not clamped to the end of the run of allocations that contains its base");
    Require(declares(collect({buffer(base + region + 0x100, 16, 64, false)}), base + region + 0x100, base + region + 0x500, WriteOrigin::BufferElement), "a strided written buffer inside a run of allocations lost its size");
    Require(declares(collect({buffer(runEnd + 0x100, 0, 1u << 30u, false)}), runEnd + 0x100, runEnd + 0x100 + (1ull << 30u), WriteOrigin::BufferElement), "a written buffer based outside the allocation registry was clamped");
    Require(collect({buffer(base + 0x100, 0, 0xffffffffu, true)}).empty() && !unknown, "an optional written buffer larger than 64 MiB before its clamp declared a write");
    std::uint64_t surfaceBase = 0;
    std::uint64_t surfaceBytes = 0;
    Require(AgcDriver::Graphics::DecodeTextureExtent(image(base + 0x1000, 0, false).guestDescriptor, surfaceBase, surfaceBytes) && surfaceBase == base + 0x1000 && surfaceBytes > 0x100, "the storage image test descriptor has no surface extent");
    Require(declares(collect({image(base + 0x1000, 0, false)}), base + 0x1000, base + 0x1000 + surfaceBytes, WriteOrigin::StorageImage), "a storage image inside a run of allocations did not declare its surface");
    Require(declares(collect({image(runEnd - 0x100, 0, false)}), runEnd - 0x100, runEnd, WriteOrigin::StorageImage), "a storage image surface was not clamped to the end of the run of allocations that contains its base");
    Require(declares(collect({image(base + region + 0x1000, 0x02, false)}), base + region + 0x1000, runEnd, WriteOrigin::AllocationFallback), "a storage image whose surface cannot be sized did not declare the rest of the run of allocations that contains its base");
    collect({image(runEnd + 0x1000, 0x02, false)});
    Require(unknown, "a storage image whose surface cannot be sized and whose base is not registered did not make its writes unknown");
    Require(collect({image(runEnd + 0x1000, 0x02, true), image(0, 0, false)}).empty() && !unknown, "a read-only or null storage image declared a write");
    const auto writes = collect({buffer(base + 0x100, 4, 16, false), image(base + region + 0x1000, 0x02, false)});
    Require(!unknown && writes.size() == 2 && writes[0].origin == WriteOrigin::BufferElement && writes[0].last == base + 0x140 && writes[1].origin == WriteOrigin::AllocationFallback && writes[1].last == runEnd, "the writes of several bindings were not all declared");
    unmapRegistered(memory, region);
    unmapRegistered(memory + region, region);
    Require(declares(collect({buffer(base + 0x100, 0, 0xffffffffu, false)}), base + 0x100, base + 0x100 + 0xffffffffull, WriteOrigin::BufferElement), "a written buffer stayed clamped to unmapped allocations");
    GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(memory + 2 * region, 2 * region);
}

}

void RunGuestAllocationTests() {
    void* pointer = GuestHeap::GuestHeapAllocate_nid_postfix(32);
    Require(reinterpret_cast<std::uintptr_t>(pointer) % alignof(std::max_align_t) == 0, "guest malloc is not suitably aligned");
    std::memset(pointer, 0x55, 32);
    {
        auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        Require(lease.size() == 1 && lease.front()->address == reinterpret_cast<std::uintptr_t>(pointer), "guest heap registration is missing");
        reject([&] { GuestHeap::GuestHeapFree_nid_postfix(pointer); });
        reject([&] { GuestHeap::GuestHeapReallocate_nid_postfix(pointer, 64); });
        GuestAllocations::Mutation mutation;
        bool applied = false;
        reject([&] { mutation.Protect(pointer, 32, true, false, [&] { applied = true; }); });
        Require(!applied, "pinned guest protection changed");
    }
    pointer = GuestHeap::GuestHeapReallocate_nid_postfix(pointer, 64);
    for (std::size_t i = 0; i < 32; ++i) Require(static_cast<unsigned char*>(pointer)[i] == 0x55, "guest realloc lost data");
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
    pointer = GuestHeap::GuestHeapAlign_nid_postfix(4, 32);
    Require(reinterpret_cast<std::uintptr_t>(pointer) % 4 == 0, "small guest alignment was not respected");
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
    pointer = GuestHeap::GuestHeapAlign_nid_postfix(256, 32);
    Require(reinterpret_cast<std::uintptr_t>(pointer) % 256 == 0, "guest aligned allocation lost alignment");
    std::memset(pointer, 0x66, 32);
    pointer = GuestHeap::GuestHeapReallocate_nid_postfix(pointer, 64);
    Require(static_cast<unsigned char*>(pointer)[31] == 0x66, "aligned guest realloc lost data");
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
    Require(GuestAllocations::GuestAllocationsAcquire_nid_postfix().empty(), "freed guest allocations remain registered");
    std::array<std::byte, 128> mapping{};
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(mapping.data(), mapping.size(), true, true);
        reject([&] { mutation.RequireAvailable(mapping.data() + 32, 16); });
        mutation.Protect(mapping.data() + 32, 32, true, false, [] {});
    }
    {
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        Require(lease.size() == 3 && lease[0]->bytes == 32 && !lease[1]->writable && lease[2]->bytes == 64, "partial protection did not split the mapping");
        GuestAllocations::Mutation mutation;
        reject([&] { mutation.Unmap(mapping.data() + 32, 32, [](const void*, bool) {}); });
    }
    {
        GuestAllocations::Mutation mutation;
        bool applied = false;
        mutation.Unmap(mapping.data() + 32, 32, [&](const void* allocation, bool last) {
            Require(allocation == mapping.data() && !last, "partial unmap released the allocation");
            applied = true;
        });
        Require(applied, "partial unmap callback was not called");
        reject([&] { mutation.Protect(mapping.data(), mapping.size(), true, true, [] {}); });
        mutation.Unmap(mapping.data(), 32, [&](const void* allocation, bool last) {
            Require(allocation == mapping.data() && !last, "first fragment released remaining mapping");
        });
        mutation.Unmap(mapping.data() + 64, 64, [&](const void* allocation, bool last) {
            Require(allocation == mapping.data() && last, "last fragment did not release the original allocation");
        });
    }
    Require(GuestAllocations::GuestAllocationsAcquire_nid_postfix().empty(), "unmapped fragments remain registered");
#ifdef _WIN32
    static std::byte imageProbe{};
    {
        GuestAllocations::Mutation mutation;
        mutation.RegisterMainImage();
        mutation.RegisterMainImage();
    }
    const auto imageAddress = reinterpret_cast<std::uintptr_t>(&imageProbe);
    std::uint64_t allocationAddress = 0;
    std::size_t imageRangeCount = 0;
    {
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        imageRangeCount = lease.size();
        const auto found = std::find_if(lease.begin(), lease.end(), [&](const auto& range) { return imageAddress >= range->address && imageAddress - range->address < range->bytes; });
        Require(found != lease.end() && (*found)->writable && !(*found)->releasable, "main image registration is missing or releasable");
        allocationAddress = (*found)->allocationAddress;
        GuestAllocations::Mutation mutation;
        bool applied = false;
        reject([&] { mutation.Protect(&imageProbe, 1, true, false, [&] { applied = true; }); });
        Require(!applied, "pinned image protection changed");
    }
    {
        GuestAllocations::Mutation mutation;
        reject([&] { mutation.Find(reinterpret_cast<void*>(allocationAddress)); });
        reject([&] { mutation.Remove(reinterpret_cast<void*>(allocationAddress)); });
        bool applied = false;
        reject([&] { mutation.Unmap(&imageProbe, 1, [&](const void*, bool) { applied = true; }); });
        Require(!applied, "image memory was unmapped");
        reject([&] { mutation.Protect(&imageProbe, 1, true, false, [] { throw std::runtime_error("host protection failure"); }); });
    }
    Require(GuestAllocations::GuestAllocationsAcquire_nid_postfix().size() == imageRangeCount, "failed image protection changed registry ranges");
    {
        GuestAllocations::Mutation mutation;
        mutation.Protect(&imageProbe, 1, true, true, [] {});
        bool applied = false;
        reject([&] { mutation.Unmap(&imageProbe, 1, [&](const void*, bool) { applied = true; }); });
        Require(!applied, "split image memory became releasable");
    }
#endif
    gpuRangeTests();
    coverageDuringMutationTests();
    extentTests();
    writeRangeTests();
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

extern "C" {
void* APS5_VABI mmap_nid_postfix(void*, std::size_t, int, int, int, std::int64_t) noexcept;
int APS5_VABI munmap_nid_postfix(void*, std::size_t) noexcept;
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelMunmap(std::uint64_t, std::size_t);
int APS5_VABI sceKernelReleaseDirectMemory(std::int64_t, std::size_t);
}

static void Require(bool condition) {
    if (!condition) {
        std::fputs("Guest memory check failed\n", stderr);
        std::abort();
    }
}

static void RequirePhysicalAliasing() {
    constexpr std::size_t mebibyte = 1u << 20;
    std::int64_t physical = -1;
    Require(sceKernelAllocateDirectMemory(0, 13824ll << 20, 8 * mebibyte, 2 * mebibyte, 0, &physical) == 0);
    void* firstView = nullptr;
    void* secondView = nullptr;
    Require(sceKernelMapDirectMemory(&firstView, 4 * mebibyte, 3, 0, physical, 2 * mebibyte) == 0);
    Require(sceKernelMapDirectMemory(&secondView, 4 * mebibyte, 3, 0, physical, 2 * mebibyte) == 0);
    auto* first = static_cast<unsigned char*>(firstView);
    auto* second = static_cast<unsigned char*>(secondView);
    Require(first != second);
    for (std::size_t piece = 0; piece < 4; ++piece) first[piece * mebibyte + 5] = static_cast<unsigned char>(piece + 1);
    for (std::size_t piece = 0; piece < 4; ++piece) Require(second[piece * mebibyte + 5] == piece + 1);
    Require(sceKernelMunmap(reinterpret_cast<std::uintptr_t>(first + mebibyte), mebibyte) == 0);
    Require(first[5] == 1 && first[2 * mebibyte + 5] == 3 && first[3 * mebibyte + 5] == 4);
    second[3 * mebibyte + 5] = 44;
    Require(first[3 * mebibyte + 5] == 44);
    void* hole = first + mebibyte;
    Require(sceKernelMapDirectMemory(&hole, mebibyte, 3, 0x10, physical + static_cast<std::int64_t>(4 * mebibyte), 0) == 0);
    Require(hole == first + mebibyte);
    first[mebibyte + 9] = 99;
    void* thirdView = nullptr;
    Require(sceKernelMapDirectMemory(&thirdView, mebibyte, 3, 0, physical + static_cast<std::int64_t>(4 * mebibyte), 0) == 0);
    Require(static_cast<unsigned char*>(thirdView)[9] == 99);
    Require(second[mebibyte + 5] == 2);
    Require(sceKernelMunmap(reinterpret_cast<std::uintptr_t>(first), 4 * mebibyte) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uintptr_t>(second), 4 * mebibyte) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uintptr_t>(thirdView), mebibyte) == 0);
    Require(sceKernelReleaseDirectMemory(physical, 8 * mebibyte) == 0);
}

struct TrackingProbe {
    GuestMemoryTracking::Watch* watch = nullptr;
    int reads = 0;
    int writes = 0;
};

static void TrackingResolver(void* context, GuestMemoryTracking::Access access) {
    auto& probe = *static_cast<TrackingProbe*>(context);
    if (access == GuestMemoryTracking::Access::Read) ++probe.reads;
    else ++probe.writes;
    probe.watch->Protect(access == GuestMemoryTracking::Access::Read ? GuestMemoryTracking::Protection::Read : GuestMemoryTracking::Protection::ReadWrite);
}

static void RequireTrackingFastPath() {
    using namespace GuestMemoryTracking;
    const auto pageSize = GuestMemoryTrackingPageSize_nid_postfix();
    constexpr std::size_t mapped = 1u << 20;
    void* memory = GuestMemoryBacking::GuestMemoryBackingMap_nid_postfix(nullptr, mapped, 1u << 16, 3);
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    {
        TrackingProbe probe;
        Watch watch(base + pageSize, 2 * pageSize, &probe, &TrackingResolver);
        probe.watch = &watch;
        GuestMemoryTrackingResolve_nid_postfix(base + pageSize, 4, true);
        Require(probe.reads == 0 && probe.writes == 0);
        watch.Protect(Protection::Read);
        GuestMemoryTrackingResolve_nid_postfix(base + pageSize, 4, false);
        GuestMemoryTrackingResolve_nid_postfix(base, pageSize, true);
        GuestMemoryTrackingResolve_nid_postfix(base + 3 * pageSize, 4, true);
        Require(probe.reads == 0 && probe.writes == 0);
        GuestMemoryTrackingResolve_nid_postfix(base + 2 * pageSize + 8, 4, true);
        Require(probe.reads == 0 && probe.writes == 1);
        GuestMemoryTrackingResolve_nid_postfix(base + pageSize, 4, true);
        Require(probe.writes == 1);
        watch.Protect(Protection::None);
        GuestMemoryTrackingResolve_nid_postfix(base, 2 * pageSize, false);
        Require(probe.reads == 1 && probe.writes == 1);
        GuestMemoryTrackingResolve_nid_postfix(base + pageSize, 4, false);
        Require(probe.reads == 1);
        *reinterpret_cast<volatile std::uint32_t*>(base + pageSize + 16) = 7;
        Require(probe.writes == 2 && *reinterpret_cast<volatile std::uint32_t*>(base + pageSize + 16) == 7);
        watch.Protect(Protection::Read);
    }
    GuestMemoryTrackingResolve_nid_postfix(base + pageSize, 4, true);
    *reinterpret_cast<volatile std::uint32_t*>(base + pageSize + 16) = 9;
    Require(*reinterpret_cast<volatile std::uint32_t*>(base + pageSize + 16) == 9);
    GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(memory, mapped);
}

static void RequirePhysicalViewSplits() {
    using namespace GuestMemoryTracking;
    constexpr std::size_t mebibyte = 1u << 20;
    constexpr std::size_t granule = 1u << 16;
    const auto pageSize = GuestMemoryTrackingPageSize_nid_postfix();
    std::int64_t physical = -1;
    Require(sceKernelAllocateDirectMemory(0, 13824ll << 20, 4 * mebibyte, 2 * mebibyte, 0, &physical) == 0);
    void* view = nullptr;
    Require(sceKernelMapDirectMemory(&view, 4 * mebibyte, 3, 0, physical, 2 * mebibyte) == 0);
    auto* bytes = static_cast<unsigned char*>(view);
    const auto base = reinterpret_cast<std::uint64_t>(view);
    for (std::size_t offset = 0; offset < 4 * mebibyte; offset += granule) bytes[offset] = static_cast<unsigned char>(offset / granule + 1);
    {
        TrackingProbe probe;
        Watch watch(base + mebibyte - pageSize, 2 * pageSize, &probe, &TrackingResolver);
        probe.watch = &watch;
        watch.Protect(Protection::None);
        Require(*reinterpret_cast<volatile unsigned char*>(base + mebibyte) == 17 && probe.reads == 1);
        *reinterpret_cast<volatile unsigned char*>(base + mebibyte - pageSize) = 5;
        Require(probe.writes == 1);
        watch.Protect(Protection::Read);
    }
    bytes[mebibyte - pageSize] = 6;
    Require(sceKernelMunmap(base + mebibyte + granule, mebibyte) == 0);
    for (std::size_t offset = 0; offset < 4 * mebibyte; offset += granule) {
        if (offset < mebibyte + granule || offset >= 2 * mebibyte + granule) Require(bytes[offset] == static_cast<unsigned char>(offset / granule + 1));
    }
    Require(bytes[mebibyte - pageSize] == 6);
    bytes[mebibyte] = 7;
    bytes[2 * mebibyte + granule] = 8;
    Require(sceKernelMunmap(base, mebibyte + granule) == 0);
    Require(bytes[2 * mebibyte + granule] == 8);
    Require(sceKernelMunmap(base + 2 * mebibyte + granule, 2 * mebibyte - granule) == 0);
    Require(sceKernelReleaseDirectMemory(physical, 4 * mebibyte) == 0);
}

int main() {
    constexpr std::size_t page = 0x4000;
    const auto failed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    const auto reject = [&](std::size_t length, int protection, int flags, int fd,
                            std::int64_t offset, int error) {
        *__error_nid_postfix() = 0;
        Require(mmap_nid_postfix(nullptr, length, protection, flags, fd, offset) == failed);
        Require(*__error_nid_postfix() == error);
    };
    reject(0, 3, 0x1002, -1, 0, 22);
    reject(std::numeric_limits<std::size_t>::max(), 3, 0x1002, -1, 0, 22);
    reject(page, 8, 0x1002, -1, 0, 22);
    reject(page, 3, 0x1002, 0, 0, 22);
    reject(page, 3, 0x1002, -1, 1, 22);
    reject(page, 3, 0x1001, -1, 0, 45); // shared
    reject(page, 3, 0x1012, -1, 0, 45); // fixed
    reject(page, 3, 0x2, 0, 0, 45);    // file-backed
    reject(page, 3, 0x22, -1, 0, 45);  // Linux MAP_ANON is not guest MAP_ANON

    auto* memory = static_cast<unsigned char*>(mmap_nid_postfix(nullptr, page * 3 - 1, 3, 0x1002, -1, 0));
    Require(memory != failed && (reinterpret_cast<std::uintptr_t>(memory) & (page - 1)) == 0);
    for (std::size_t i = 0; i < page * 3; ++i) Require(memory[i] == 0);
    memory[0] = 42;
    memory[page * 2] = 73;
    {
        GuestAllocations::Mutation mutation;
        const auto range = mutation.Find(memory);
        Require(range.bytes == page * 3 && range.readable && range.writable);
    }
    Require(munmap_nid_postfix(memory + 1, page) == -1 && *__error_nid_postfix() == 22);
    Require(munmap_nid_postfix(memory, 0) == -1 && *__error_nid_postfix() == 22);
    Require(memory[0] == 42);
    Require(munmap_nid_postfix(memory + page, 1) == 0); // round to one guest page
    Require(memory[0] == 42 && memory[page * 2] == 73);
    Require(munmap_nid_postfix(memory, page) == 0);
    Require(memory[page * 2] == 73);
    Require(munmap_nid_postfix(memory + page * 2, page) == 0);
    Require(munmap_nid_postfix(memory, page) == 0);
    for (int protection : {0, 1, 3, 5}) {
        void* mapped = mmap_nid_postfix(memory, 1, protection, 0x1002, -1, 0);
        Require(mapped != failed);
        {
            GuestAllocations::Mutation mutation;
            const auto range = mutation.Find(mapped);
            Require(range.readable == ((protection & 3) != 0));
            Require(range.writable == ((protection & 2) != 0));
        }
        Require(munmap_nid_postfix(mapped, 1) == 0);
    }
    RequirePhysicalAliasing();
    RequireTrackingFastPath();
    RequirePhysicalViewSplits();
    void* writable = mmap_nid_postfix(nullptr, page, 3, 0x1002, -1, 0);
    void* readOnly = mmap_nid_postfix(nullptr, page, 1, 0x1002, -1, 0);
    Require(writable != failed && readOnly != failed);
    const auto writableAddress = reinterpret_cast<std::uint64_t>(writable);
    const auto readOnlyAddress = reinterpret_cast<std::uint64_t>(readOnly);
    Require(GuestAllocations::GuestAllocationsCovers_nid_postfix(writableAddress + 8, page - 8, true));
    Require(GuestAllocations::GuestAllocationsCovers_nid_postfix(readOnlyAddress, page, false));
    Require(!GuestAllocations::GuestAllocationsCovers_nid_postfix(readOnlyAddress, 16, true));
    Require(munmap_nid_postfix(writable, page) == 0 && munmap_nid_postfix(readOnly, page) == 0);
    Require(!GuestAllocations::GuestAllocationsCovers_nid_postfix(writableAddress, 16, false));
    Require(!GuestAllocations::GuestAllocationsCovers_nid_postfix(readOnlyAddress, 16, false));
}

#include "prx/libc/include/MemoryBackingPlatform.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <sys/mman.h>
#include <unistd.h>

namespace GuestMemoryBacking::Platform {
namespace {

void check(bool success, const char* operation) {
    if (!success) throw std::system_error(errno, std::generic_category(), operation);
}

int physicalDescriptor() {
    static const int descriptor = [] {
        const int created = memfd_create("AnyPS5 guest physical memory", MFD_CLOEXEC);
        check(created >= 0, "memfd_create guest physical memory");
        check(ftruncate(created, static_cast<off_t>(GuestPhysicalMemoryBytes)) == 0, "ftruncate guest physical memory");
        return created;
    }();
    return descriptor;
}

std::byte* physicalAlias() {
    static auto* const alias = [] {
        void* mapped = mmap(nullptr, GuestPhysicalMemoryBytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE, physicalDescriptor(), 0);
        check(mapped != MAP_FAILED, "mmap guest physical memory alias");
        return static_cast<std::byte*>(mapped);
    }();
    return alias;
}

std::uintptr_t mapView(void* address, std::size_t bytes, std::size_t alignment, int protection, int descriptor, off_t offset) {
    void* reservation = MAP_FAILED;
    std::size_t reservationBytes = 0;
    try {
        if (address == nullptr) {
            reservationBytes = bytes + alignment;
            reservation = mmap(nullptr, reservationBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        } else {
            reservationBytes = bytes;
            reservation = mmap(address, reservationBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        }
        check(reservation != MAP_FAILED, "mmap guest backing reservation");
        if (address != nullptr && reservation != address) throw std::runtime_error("fixed guest backing reservation address mismatch");
        const auto first = reinterpret_cast<std::uintptr_t>(reservation);
        const auto aligned = (first + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
        void* guest = mmap(reinterpret_cast<void*>(aligned), bytes, protection, MAP_SHARED | MAP_FIXED, descriptor, offset);
        check(guest != MAP_FAILED, "mmap guest backing view");
        const auto prefix = aligned - first;
        const auto suffix = reservationBytes - prefix - bytes;
        if (prefix != 0) check(munmap(reservation, prefix) == 0, "munmap guest backing prefix");
        if (suffix != 0) check(munmap(reinterpret_cast<void*>(aligned + bytes), suffix) == 0, "munmap guest backing suffix");
        return aligned;
    } catch (...) {
        if (reservation != MAP_FAILED) munmap(reservation, reservationBytes);
        throw;
    }
}

}

Mapping Map(void* address, std::size_t bytes, std::size_t alignment, int protection) {
    if (bytes > static_cast<std::size_t>(std::numeric_limits<off_t>::max()) || bytes > std::numeric_limits<std::size_t>::max() - alignment) throw std::overflow_error("guest backing size overflow");
    int descriptor = memfd_create("AnyPS5 guest memory", MFD_CLOEXEC);
    check(descriptor >= 0, "memfd_create guest backing");
    void* alias = MAP_FAILED;
    void* guest = MAP_FAILED;
    try {
        check(ftruncate(descriptor, static_cast<off_t>(bytes)) == 0, "ftruncate guest backing");
        alias = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, descriptor, 0);
        check(alias != MAP_FAILED, "mmap guest backing alias");
        const auto aligned = mapView(address, bytes, alignment, protection, descriptor, 0);
        guest = reinterpret_cast<void*>(aligned);
        const auto closeResult = close(descriptor);
        descriptor = -1;
        check(closeResult == 0, "close guest backing descriptor");
        return {aligned, bytes, alias, 0};
    } catch (...) {
        if (guest != MAP_FAILED) check(munmap(guest, bytes) == 0, "munmap failed guest view");
        if (alias != MAP_FAILED) check(munmap(alias, bytes) == 0, "munmap failed guest alias");
        if (descriptor >= 0) check(close(descriptor) == 0, "close failed guest backing");
        throw;
    }
}

Mapping MapPhysical(void* address, std::size_t bytes, std::size_t alignment, int protection, std::uint64_t offset) {
    if (offset > GuestPhysicalMemoryBytes || bytes > GuestPhysicalMemoryBytes - offset) throw std::out_of_range("guest physical mapping exceeds physical memory");
    if (bytes > std::numeric_limits<std::size_t>::max() - alignment) throw std::overflow_error("guest backing size overflow");
    const auto guest = mapView(address, bytes, alignment, protection, physicalDescriptor(), static_cast<off_t>(offset));
    return {guest, bytes, physicalAlias() + offset, 0, true};
}

void UnmapViewRange(std::uint64_t address, std::size_t bytes) {
    check(munmap(reinterpret_cast<void*>(address), bytes) == 0, "munmap guest view range");
}

void UnmapView(const Mapping& mapping) {
    UnmapViewRange(mapping.address, mapping.bytes);
}

void UnmapAlias(const Mapping& mapping) {
    if (mapping.physical) return;
    check(munmap(mapping.alias, mapping.bytes) == 0, "munmap guest alias");
}

void Unmap(const Mapping& mapping) {
    UnmapView(mapping);
    UnmapAlias(mapping);
}

void Protect(std::uint64_t address, std::size_t bytes, int protection) {
    check(mprotect(reinterpret_cast<void*>(address), bytes, protection) == 0, "mprotect guest backing protect");
}

void Deactivate(std::uint64_t address, std::size_t bytes) {
    check(mprotect(reinterpret_cast<void*>(address), bytes, PROT_NONE) == 0, "mprotect guest backing unmap");
}

}

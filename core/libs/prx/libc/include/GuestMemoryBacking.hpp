#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTMEMORYBACKING_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTMEMORYBACKING_HPP

#include <cstddef>
#include <cstdint>

namespace GuestMemoryBacking {

// A whole shared mapping: its guest range, the host alias of its first byte, and a serial that
// changes when the mapping is replaced.
struct GuestMemoryBackingExtentInfo {
    std::uint64_t address;
    std::uint64_t bytes;
    void* alias;
    std::uint64_t serial;
};

extern "C" {
void* GuestMemoryBackingMap_nid_postfix(void* address, std::size_t bytes, std::size_t alignment, int protection);
void GuestMemoryBackingUnmap_nid_postfix(void* address, std::size_t bytes);
void GuestMemoryBackingRequire_nid_postfix(std::uint64_t address, std::size_t bytes);
// Changes the protection of an active part of a mapping (commits a reserved range in place).
void GuestMemoryBackingActivate_nid_postfix(std::uint64_t address, std::size_t bytes, int protection);
void GuestMemoryBackingWrite_nid_postfix(std::uint64_t address, const void* source, std::size_t bytes);
// The host alias (always readable and writable) of an active guest range, or null when the range
// has no shared backing. Both views share the same pages.
void* GuestMemoryBackingAlias_nid_postfix(std::uint64_t address, std::size_t bytes);
// The shared mapping holding [address, address + bytes) (false when there is none).
bool GuestMemoryBackingExtent_nid_postfix(std::uint64_t address, std::size_t bytes, GuestMemoryBackingExtentInfo* info);
// Keeps the alias of the mapping at `address` (with that serial) for a GPU import: unmapping the
// mapping leaves the alias until every import is released. Null when the mapping is gone.
void* GuestMemoryBackingRetainAlias_nid_postfix(std::uint64_t address, std::uint64_t serial);
void GuestMemoryBackingReleaseAlias_nid_postfix(void* alias);
// Counts whole mappings unmapped so far: importers compare it to find stale imports.
std::uint64_t GuestMemoryBackingUnmapGeneration_nid_postfix();
}

}

#endif

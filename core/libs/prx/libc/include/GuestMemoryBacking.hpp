#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTMEMORYBACKING_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTMEMORYBACKING_HPP

#include <cstddef>
#include <cstdint>

namespace GuestMemoryBacking {

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
void GuestMemoryBackingActivate_nid_postfix(std::uint64_t address, std::size_t bytes, int protection);
void GuestMemoryBackingWrite_nid_postfix(std::uint64_t address, const void* source, std::size_t bytes);
void* GuestMemoryBackingAlias_nid_postfix(std::uint64_t address, std::size_t bytes);
bool GuestMemoryBackingExtent_nid_postfix(std::uint64_t address, std::size_t bytes, GuestMemoryBackingExtentInfo* info);
void* GuestMemoryBackingRetainAlias_nid_postfix(std::uint64_t address, std::uint64_t serial);
void GuestMemoryBackingReleaseAlias_nid_postfix(void* alias);
std::uint64_t GuestMemoryBackingUnmapGeneration_nid_postfix();
}

}

#endif

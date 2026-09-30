#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_KEYHASH_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_KEYHASH_HPP

#include <bit>
#include <cstdint>

namespace AgcDriver::Graphics {

inline constexpr std::uint64_t KeyHashSeed = 0xcbf29ce484222325ull;

inline std::uint64_t MixKeyHash(std::uint64_t hash, std::uint64_t value) {
    return std::rotl((hash ^ value) * 0x9e3779b97f4a7c15ull, 31);
}

inline std::uint64_t FinishKeyHash(std::uint64_t hash) {
    hash ^= hash >> 30u;
    hash *= 0xbf58476d1ce4e5b9ull;
    hash ^= hash >> 27u;
    hash *= 0x94d049bb133111ebull;
    return hash ^ (hash >> 31u);
}

}

#endif

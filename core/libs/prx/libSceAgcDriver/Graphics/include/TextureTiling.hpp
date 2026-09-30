#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURETILING_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURETILING_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace AgcDriver::Graphics {

struct TileMipLayout {
    std::uint64_t tiledOffset;
    std::uint64_t tiledSize;
    std::uint64_t linearOffset;
    std::uint64_t linearSize;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
    std::uint32_t blocksPerRow;
    std::uint32_t pitchBytes;
    bool tail;
    std::uint32_t tailX;
    std::uint32_t tailY;
    std::uint32_t blockDepth;
    std::uint64_t sliceStride;
};

struct TileSwizzleMasks {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;
};

struct TileSwizzleEquation {
    std::array<std::uint16_t, 8> x;
    std::array<std::uint16_t, 8> y;
    std::array<std::uint16_t, 4> slice;
};

std::vector<TileMipLayout> ComputeMipLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount);
std::vector<TileMipLayout> ComputeMipLayout(const GuestTextureResource& descriptor);
std::uint64_t ComputeSurfaceSize(const std::vector<TileMipLayout>& mips, std::uint32_t arrayLayers);
std::uint64_t DepthSliceBytes(std::uint32_t bytesPerElement, std::uint32_t width, std::uint32_t height);
TileSwizzleMasks ThickSwizzleMasks(TextureTileMode tileMode, std::uint32_t bytesPerElement);
TileSwizzleEquation ZOrderSwizzleEquation(std::uint32_t bytesPerElement);

}

#endif

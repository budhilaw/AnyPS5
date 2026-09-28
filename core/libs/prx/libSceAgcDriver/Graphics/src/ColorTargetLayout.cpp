#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace AgcDriver::Graphics {
namespace {

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint32_t blockOffset(std::uint32_t x, std::uint32_t y) {
    return ((y << 4u) & 0x0070u) ^ ((y << 5u) & 0x0f00u) ^ ((y << 9u) & 0x1000u) ^ ((y << 8u) & 0x4000u)
        ^ ((x << 2u) & 0x000cu) ^ ((x << 5u) & 0x0380u) ^ ((x << 4u) & 0x0400u) ^ ((x << 6u) & 0x0800u) ^ ((x << 9u) & 0xa000u);
}

}

ColorTileMode DecodeColorTileMode(std::uint32_t attrib3) {
    // RESOURCE_TYPE 0 (1D) or 1 (2D): both address a 2D-shaped surface here.
    if (!((attrib3 & 0x80002000u) == 0 && (attrib3 & 0x1fffu) == 0 && ((attrib3 >> 24u) & 3u) <= 1 && ((attrib3 >> 27u) & 7u) == 1)) {
        char message[160];
        std::snprintf(message, sizeof(message), "AGC graphics: unsupported color depth, dimension, resource level or metadata mode (CB_COLOR_ATTRIB3 0x%08x)", attrib3);
        throw std::runtime_error(message);
    }
    const auto mode = (attrib3 >> 14u) & 0x1fu;
    const auto fmaskMode = (attrib3 >> 19u) & 0x1fu;
    require(fmaskMode == 0 || fmaskMode == 0x18, "AGC graphics: unsupported color FMASK swizzle mode");
    require(mode == 0 || mode == 0x18 || mode == 0x1b, "AGC graphics: unsupported color tile mode");
    return static_cast<ColorTileMode>(mode);
}

ColorTargetLayout::ColorTargetLayout(std::uint32_t width, std::uint32_t height, ColorTileMode mode, std::uint32_t bytesPerPixel) : width(width), height(height), pitch(width), mode(mode), bytesPerPixel(bytesPerPixel), bytes(0) {
    require(width != 0 && height != 0 && width <= 16384 && height <= 16384, "AGC graphics: invalid color surface extent");
    require(bytesPerPixel == 1 || bytesPerPixel == 2 || bytesPerPixel == 4 || bytesPerPixel == 8 || bytesPerPixel == 16, "AGC graphics: unsupported color texel size");
    std::uint32_t paddedHeight = height;
    switch (mode) {
        case ColorTileMode::Linear:
            require((width * bytesPerPixel) % 256u == 0, "AGC graphics: linear surface pitch requires a width aligned to 256 bytes");
            break;
        case ColorTileMode::ZOrder64KB:
        case ColorTileMode::RenderTarget: {
            // 64 KiB blocks: 128x128 texels at 32 bits, halving the height (then width) as texels grow.
            const std::uint32_t blockWidth = bytesPerPixel <= 2 ? 256u : bytesPerPixel <= 8 ? 128u : 64u;
            const std::uint32_t blockHeight = 65536u / bytesPerPixel / blockWidth;
            pitch = (width + blockWidth - 1u) / blockWidth * blockWidth;
            paddedHeight = (height + blockHeight - 1u) / blockHeight * blockHeight;
            break;
        }
        default: throw std::runtime_error("AGC graphics: unsupported color tile mode");
    }
    const auto size = static_cast<std::uint64_t>(pitch) * paddedHeight * bytesPerPixel;
    require(size <= std::numeric_limits<std::size_t>::max(), "AGC graphics: color surface size overflow");
    bytes = static_cast<std::size_t>(size);
}

std::size_t ColorTargetLayout::offset(std::uint32_t x, std::uint32_t y) const {
    if (mode == ColorTileMode::Linear) return (static_cast<std::size_t>(y) * pitch + x) * bytesPerPixel;
    require(bytesPerPixel == 4, "AGC graphics: tiled color transfers support 32-bit texels only");
    const auto block = static_cast<std::size_t>(y / 128u) * (pitch / 128u) + x / 128u;
    return block * 65536u + blockOffset(x, y);
}

std::size_t ColorTargetLayout::Offset(std::uint32_t x, std::uint32_t y) const {
    require(x < width && y < height, "AGC graphics: color surface coordinate out of range");
    return offset(x, y);
}

void ColorTargetLayout::Detile(std::span<const std::byte> source, std::span<std::byte> destination) const {
    require(source.size() == Bytes() && destination.size() == LinearBytes(), "AGC graphics: color detile buffer size mismatch");
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::memcpy(destination.data() + (static_cast<std::size_t>(y) * width + x) * bytesPerPixel, source.data() + offset(x, y), bytesPerPixel);
        }
    }
}

void ColorTargetLayout::Tile(std::span<const std::byte> source, std::span<std::byte> destination) const {
    require(source.size() == LinearBytes() && destination.size() == Bytes(), "AGC graphics: color tile buffer size mismatch");
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::memcpy(destination.data() + offset(x, y), source.data() + (static_cast<std::size_t>(y) * width + x) * bytesPerPixel, bytesPerPixel);
        }
    }
}

}

#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COLORTARGETLAYOUT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COLORTARGETLAYOUT_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace AgcDriver::Graphics {

enum class ColorTileMode : std::uint32_t {
    Linear = 0,
    // 64KB_Z_X: kept on the GPU only (no host tiling code); sampled like a depth-swizzled texture.
    ZOrder64KB = 0x18,
    RenderTarget = 0x1b
};

ColorTileMode DecodeColorTileMode(std::uint32_t attrib3);

class ColorTargetLayout {
public:
    ColorTargetLayout(std::uint32_t width, std::uint32_t height, ColorTileMode mode, std::uint32_t bytesPerPixel = 4);
    std::size_t Bytes() const { return bytes; }
    std::size_t LinearBytes() const { return static_cast<std::size_t>(width) * height * bytesPerPixel; }
    std::uint32_t BytesPerPixel() const { return bytesPerPixel; }
    std::size_t Alignment() const { return mode == ColorTileMode::Linear ? 256u : 65536u; }
    std::size_t Offset(std::uint32_t x, std::uint32_t y) const;
    void Detile(std::span<const std::byte> source, std::span<std::byte> destination) const;
    void Tile(std::span<const std::byte> source, std::span<std::byte> destination) const;

private:
    std::size_t offset(std::uint32_t x, std::uint32_t y) const;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pitch;
    ColorTileMode mode;
    std::uint32_t bytesPerPixel;
    std::size_t bytes;
};

}

#endif

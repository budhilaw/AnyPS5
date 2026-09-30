#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace AgcDriver {
namespace {

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint32_t tileOffset(std::uint32_t x, std::uint32_t y) {
    return ((y << 4u) & 0x0070u) ^ ((y << 5u) & 0x0f00u) ^ ((y << 9u) & 0x1000u) ^ ((y << 8u) & 0x4000u) ^ ((x << 2u) & 0x000cu) ^ ((x << 5u) & 0x0380u) ^ ((x << 4u) & 0x0400u) ^ ((x << 6u) & 0x0800u) ^ ((x << 9u) & 0xa000u);
}

}

bool DisplayFormatSupported(std::uint64_t pixelFormat) {
    const auto kind = pixelFormat >> 56u;
    const auto order = pixelFormat & 0x00ffffffffffffffull;
    return (kind == 0x80u || kind == 0x81u) && (order == 0 || order == 0x22000000ull);
}

bool DisplayFormatRgba(std::uint64_t pixelFormat) {
    return (pixelFormat & 0x00ffffffffffffffull) == 0x22000000ull;
}

bool DisplayFormatTenBit(std::uint64_t pixelFormat) {
    return (pixelFormat >> 56u) == 0x81u;
}

std::size_t DisplayBufferSize(const DisplayBuffer& buffer) {
    require(buffer.width != 0 && buffer.height != 0 && buffer.width <= 16384 && buffer.height <= 16384, "VideoOut: invalid display buffer dimensions");
    if (!DisplayFormatSupported(buffer.pixelFormat)) {
        char message[80];
        std::snprintf(message, sizeof(message), "VideoOut: unsupported display pixel format 0x%llx", static_cast<unsigned long long>(buffer.pixelFormat));
        throw std::runtime_error(message);
    }
    require(buffer.address != 0 && (buffer.address & 65535u) == 0, "VideoOut: display buffer requires 64 KiB alignment");
    const auto size = static_cast<std::uint64_t>((buffer.width + 127u) / 128u) * ((buffer.height + 127u) / 128u) * 65536u;
    require(size <= std::numeric_limits<std::size_t>::max() && size <= std::numeric_limits<std::uintptr_t>::max() - buffer.address, "VideoOut: display buffer range overflow");
    return static_cast<std::size_t>(size);
}

std::vector<std::byte> DecodeDisplayBuffer(const DisplayBuffer& buffer, std::span<const std::byte> source) {
    PerformanceTimer timing("DisplayBuffer.Decode");
    require(source.size() == DisplayBufferSize(buffer), "VideoOut: invalid tiled display buffer size");
    std::vector<std::byte> pixels(static_cast<std::size_t>(buffer.width) * buffer.height * 4);
    const auto blocksPerRow = (buffer.width + 127u) / 128u;
    const bool rgba = DisplayFormatRgba(buffer.pixelFormat);
    const bool tenBit = DisplayFormatTenBit(buffer.pixelFormat);
    timing.Mark("validate_allocate");
    for (std::uint32_t y = 0; y < buffer.height; ++y) {
        for (std::uint32_t x = 0; x < buffer.width; ++x) {
            const auto tiled = (static_cast<std::size_t>(y / 128u) * blocksPerRow + x / 128u) * 65536u + tileOffset(x, y);
            const auto linear = (static_cast<std::size_t>(y) * buffer.width + x) * 4;
            if (tenBit) {
                std::uint32_t pixel = 0;
                std::memcpy(&pixel, source.data() + tiled, sizeof(pixel));
                const auto low = static_cast<std::byte>((pixel >> 2u) & 0xffu);
                const auto high = static_cast<std::byte>((pixel >> 22u) & 0xffu);
                pixels[linear] = rgba ? high : low;
                pixels[linear + 1] = static_cast<std::byte>((pixel >> 12u) & 0xffu);
                pixels[linear + 2] = rgba ? low : high;
                pixels[linear + 3] = static_cast<std::byte>((pixel >> 30u) * 85u);
                continue;
            }
            pixels[linear] = source[tiled + (rgba ? 2 : 0)];
            pixels[linear + 1] = source[tiled + 1];
            pixels[linear + 2] = source[tiled + (rgba ? 0 : 2)];
            pixels[linear + 3] = source[tiled + 3];
        }
    }
    timing.Mark("detile_convert");
    return pixels;
}

std::vector<std::byte> ReadDisplayBuffer(const DisplayBuffer& buffer) {
    const auto size = DisplayBufferSize(buffer);
    GuestMemory::CheckRange(reinterpret_cast<const void*>(buffer.address), size, 65536);
    const auto* source = reinterpret_cast<const std::byte*>(buffer.address);
    return DecodeDisplayBuffer(buffer, {source, size});
}

}

extern "C" std::size_t AgcDriverDisplayBufferSize_nid_postfix(const AgcDriver::DisplayBuffer& buffer) {
    return AgcDriver::DisplayBufferSize(buffer);
}

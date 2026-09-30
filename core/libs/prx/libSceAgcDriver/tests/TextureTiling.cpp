#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <string>
#include <string_view>

namespace {

using namespace AgcDriver::Graphics;

template<typename TAction>
void reject(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected texture tiling test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected texture tiling rejection: ") + std::string(reason));
}

GuestTextureResource volume(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t mipCount) {
    GuestTextureResource descriptor{};
    descriptor.baseAddress = 0x1409960000ull;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.depthOrLastArray = depth - 1u;
    descriptor.mipCount = mipCount;
    descriptor.lastLevel = mipCount - 1u;
    descriptor.tileMode = tileMode;
    descriptor.dimension = TextureDimension::k3D;
    descriptor.viewDimension = TextureDimension::k3D;
    descriptor.format = format;
    return descriptor;
}

std::uint32_t deposit(std::uint32_t value, std::uint32_t mask) {
    std::uint32_t result = 0;
    for (; mask != 0; mask &= mask - 1u, value >>= 1u) {
        if ((value & 1u) != 0) result |= mask & (~mask + 1u);
    }
    return result;
}

std::uint64_t volumeAddress(const std::vector<TileMipLayout>& mips, TileSwizzleMasks masks, std::uint32_t blockBytes, std::uint32_t level, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    const auto& mip = mips[level];
    const auto blockWidth = 1u << std::popcount(masks.x);
    const auto blockHeight = 1u << std::popcount(masks.y);
    const auto blockDepth = 1u << std::popcount(masks.z);
    const auto blockIndex = mip.tail ? 0u : (y / blockHeight) * mip.blocksPerRow + x / blockWidth;
    const auto swizzleX = mip.tail ? x + mip.tailX : x;
    const auto swizzleY = mip.tail ? y + mip.tailY : y;
    return mip.tiledOffset + static_cast<std::uint64_t>(z / blockDepth) * mip.sliceStride + static_cast<std::uint64_t>(blockIndex) * blockBytes + (deposit(swizzleX, masks.x) | deposit(swizzleY, masks.y) | deposit(z, masks.z));
}

std::uint64_t hashAddress(std::uint64_t hash, std::uint64_t address) {
    for (std::uint32_t byte = 0; byte < 8; ++byte) {
        hash ^= (address >> (byte * 8u)) & 0xffu;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::uint32_t addrlibPatternOffset(std::string_view pattern, std::uint32_t bits, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    std::uint32_t offset = 0;
    std::size_t position = 0;
    for (std::uint32_t bit = 0; bit < bits; ++bit) {
        const auto end = std::min(pattern.find(' ', position), pattern.size());
        auto token = pattern.substr(position, end - position);
        position = end + 1;
        while (token.size() >= 2) {
            const auto coordinate = token[0] == 'X' ? x : token[0] == 'Y' ? y : z;
            offset ^= ((coordinate >> static_cast<std::uint32_t>(token[1] - '0')) & 1u) << bit;
            token.remove_prefix(std::min<std::size_t>(3, token.size()));
        }
    }
    return offset;
}

struct AddrlibVolume {
    TextureTileMode tileMode;
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
    std::uint32_t mipCount;
    std::uint64_t surfaceBytes;
    std::uint64_t addressHash;
};

constexpr AddrlibVolume kAddrlibVolumes[] = {
    {TextureTileMode::kStandard64KB, 56, 32, 32, 32, 1, 131072, 0xf41892112e3a0f25ull},
    {TextureTileMode::kStandard64KB, 64, 4, 4, 64, 1, 262144, 0xf7d2366c6e23b525ull},
    {TextureTileMode::kStandard64KB, 56, 64, 64, 64, 7, 1572864, 0x85ab071f8ce38cb2ull},
    {TextureTileMode::kStandard64KB, 1, 128, 64, 32, 8, 393216, 0x03f405ec3b124e68ull},
    {TextureTileMode::kStandard64KB, 14, 48, 40, 20, 6, 393216, 0x6688520638856d2full},
    {TextureTileMode::kStandard64KB, 77, 16, 16, 16, 5, 131072, 0xfc5d16cc7b533bb7ull},
    {TextureTileMode::kStandard64KB, 64, 64, 32, 16, 7, 393216, 0x023014d195b37b9aull},
    {TextureTileMode::kStandard64KB, 56, 256, 256, 16, 9, 5636096, 0x2530539be8dcc982ull},
    {TextureTileMode::kStandard64KB, 56, 32, 32, 32, 6, 262144, 0x998f7e7f99b3e2b2ull},
    {TextureTileMode::kStandard64KB, 64, 4, 4, 64, 3, 262144, 0xfcd832687dbb3ba5ull},
    {TextureTileMode::kStandard4KB, 1, 16, 16, 16, 5, 8192, 0x9bdffc7954a31fe2ull},
    {TextureTileMode::kStandard4KB, 56, 32, 32, 32, 6, 180224, 0x77ebe81596169beaull},
    {TextureTileMode::kStandard4KB, 64, 20, 12, 9, 4, 73728, 0x0e237bcdd8d54cedull},
    {TextureTileMode::kStandard4KB, 77, 8, 8, 8, 4, 12288, 0xf9fdd6906dc6280full},
    {TextureTileMode::kStandard4KB, 56, 32, 32, 32, 1, 131072, 0x5cc952006f74f725ull},
    {TextureTileMode::kStandard4KB, 14, 64, 16, 8, 7, 61440, 0xe48113ee3cc593dcull},
};

struct AddrlibElement {
    std::size_t volume;
    std::uint32_t level;
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;
    std::uint64_t address;
};

constexpr AddrlibElement kAddrlibElements[] = {
    {0, 0, 1, 0, 0, 4}, {0, 0, 0, 1, 0, 8}, {0, 0, 0, 0, 1, 16}, {0, 0, 31, 0, 0, 37444}, {0, 0, 0, 31, 0, 18728},
    {0, 0, 0, 0, 15, 9360}, {0, 0, 0, 0, 16, 65536}, {0, 0, 5, 9, 17, 68124}, {0, 0, 31, 31, 31, 131068},
    {1, 0, 1, 0, 0, 8}, {1, 0, 0, 1, 0, 32}, {1, 0, 0, 0, 1, 16}, {1, 0, 3, 3, 15, 9720}, {1, 0, 0, 0, 16, 65536}, {1, 0, 2, 1, 33, 131184}, {1, 0, 3, 3, 63, 206328},
    {9, 0, 3, 3, 63, 239096}, {9, 1, 0, 0, 0, 16384}, {9, 1, 1, 1, 31, 91320}, {9, 2, 0, 0, 0, 4096}, {9, 2, 0, 0, 15, 13456},
    {2, 0, 63, 63, 63, 1572860}, {2, 1, 31, 31, 31, 524284}, {2, 2, 0, 0, 0, 32768}, {2, 2, 15, 15, 15, 49148}, {2, 4, 3, 3, 3, 4348}, {2, 6, 0, 0, 0, 2304},
    {3, 0, 127, 63, 31, 393215}, {3, 2, 31, 15, 7, 39935}, {3, 7, 0, 0, 0, 2048},
    {5, 0, 15, 15, 15, 131056}, {5, 1, 7, 7, 7, 40944}, {5, 4, 0, 0, 0, 2560},
    {11, 0, 31, 31, 31, 180220}, {11, 1, 15, 15, 15, 57340}, {11, 2, 7, 7, 7, 4092}, {11, 5, 0, 0, 0, 256},
    {12, 0, 19, 11, 8, 69992}, {12, 1, 9, 5, 3, 10424}, {12, 3, 1, 0, 0, 776},
    {10, 0, 15, 15, 15, 8191}, {10, 1, 7, 7, 7, 2559}, {10, 4, 0, 0, 0, 256},
};

constexpr std::string_view kAddrlibThickStandardPatterns[5] = {
    "X0 X1 Z0 Y0 Z1 Y1 X2 Z2 Y2 X3 Z3 Y3 X4 Z4 Y4 X5",
    "0 X0 Z0 Y0 Z1 Y1 X1 Z2 Y2 X2 Z3 Y3 X3 Z4 Y4 X4",
    "0 0 X0 Y0 Z0 Y1 X1 Z1 Y2 X2 Z2 Y3 X3 Z3 Y4 X4",
    "0 0 0 X0 Z0 Y0 X1 Z1 Y1 X2 Z2 Y2 X3 Z3 Y3 X4",
    "0 0 0 0 Z0 Y0 X0 Z1 Y1 X1 Z2 Y2 X2 Z3 Y3 X3",
};

constexpr std::array<std::array<std::uint32_t, 3>, 5> kAddrlibBlock4KLog2{{{4, 4, 4}, {3, 4, 4}, {3, 4, 3}, {3, 3, 3}, {2, 3, 3}}};
constexpr std::array<std::array<std::uint32_t, 3>, 5> kAddrlibBlock64KLog2{{{6, 5, 5}, {5, 5, 5}, {5, 5, 4}, {5, 4, 4}, {4, 4, 4}}};

void volumeTilingTests() {
    for (std::uint32_t index = 0; index < 5; ++index) {
        const auto bytesPerElement = 1u << index;
        for (const auto tileMode : {TextureTileMode::kStandard4KB, TextureTileMode::kStandard64KB}) {
            const auto masks = ThickSwizzleMasks(tileMode, bytesPerElement);
            const auto& log2Block = tileMode == TextureTileMode::kStandard4KB ? kAddrlibBlock4KLog2[index] : kAddrlibBlock64KLog2[index];
            Require(static_cast<std::uint32_t>(std::popcount(masks.x)) == log2Block[0] && static_cast<std::uint32_t>(std::popcount(masks.y)) == log2Block[1] && static_cast<std::uint32_t>(std::popcount(masks.z)) == log2Block[2], "thick block dimensions must match addrlib Block4K_Log2_3d and Block64K_Log2_3d");
            const auto blockBits = tileMode == TextureTileMode::kStandard4KB ? 12u : 16u;
            for (std::uint32_t z = 0; z < (1u << log2Block[2]); ++z) {
                for (std::uint32_t y = 0; y < (1u << log2Block[1]); ++y) {
                    for (std::uint32_t x = 0; x < (1u << log2Block[0]); ++x) {
                        Require((deposit(x, masks.x) | deposit(y, masks.y) | deposit(z, masks.z)) == addrlibPatternOffset(kAddrlibThickStandardPatterns[index], blockBits, x, y, z), "thick swizzle offsets must match the addrlib GFX10 SW_4K_S3 and SW_64K_S3 patterns");
                    }
                }
            }
        }
    }

    std::array<std::vector<TileMipLayout>, std::size(kAddrlibVolumes)> layouts;
    for (std::size_t index = 0; index < std::size(kAddrlibVolumes); ++index) {
        const auto& reference = kAddrlibVolumes[index];
        layouts[index] = ComputeMipLayout(volume(reference.tileMode, reference.format, reference.width, reference.height, reference.depth, reference.mipCount));
        const auto& mips = layouts[index];
        Require(ComputeSurfaceSize(mips, reference.depth) == reference.surfaceBytes, "thick volume surface size must match addrlib");
        const auto masks = ThickSwizzleMasks(reference.tileMode, BytesPerElement(reference.format));
        const auto blockBytes = reference.tileMode == TextureTileMode::kStandard4KB ? 4096u : 65536u;
        std::uint64_t hash = 14695981039346656037ull;
        for (std::uint32_t level = 0; level < reference.mipCount; ++level) {
            const auto width = std::max(reference.width >> level, 1u);
            const auto height = std::max(reference.height >> level, 1u);
            const auto depth = std::max(reference.depth >> level, 1u);
            Require(mips[level].width == width && mips[level].height == height && mips[level].depth == depth, "thick volume mip dimensions changed");
            const auto packedBytes = static_cast<std::uint64_t>(mips[level].pitchBytes) * height * depth;
            Require(mips[level].linearSize >= packedBytes && mips[level].linearSize - packedBytes < 4u && mips[level].linearSize % 4u == 0, "thick volume mips must detile every slice");
            for (std::uint32_t z = 0; z < depth; ++z) {
                for (std::uint32_t y = 0; y < height; ++y) {
                    for (std::uint32_t x = 0; x < width; ++x) hash = hashAddress(hash, volumeAddress(mips, masks, blockBytes, level, x, y, z));
                }
            }
        }
        Require(hash == reference.addressHash, "thick volume element addresses must match addrlib Addr2ComputeSurfaceAddrFromCoord");
    }
    for (const auto& element : kAddrlibElements) {
        const auto& reference = kAddrlibVolumes[element.volume];
        const auto masks = ThickSwizzleMasks(reference.tileMode, BytesPerElement(reference.format));
        const auto blockBytes = reference.tileMode == TextureTileMode::kStandard4KB ? 4096u : 65536u;
        Require(volumeAddress(layouts[element.volume], masks, blockBytes, element.level, element.x, element.y, element.z) == element.address, "thick volume element address must match the addrlib reference value");
    }

    {
        const auto& lut = layouts[0];
        Require(lut.size() == 1 && lut[0].blockDepth == 16 && lut[0].sliceStride == 65536 && lut[0].tiledSize == 65536 && !lut[0].tail, "the 32x32x32 RGBA8 LUT must use 32x32x16 thick blocks");
        Require(lut[0].depth == 32 && lut[0].pitchBytes == 128 && lut[0].linearSize == 131072, "the 32x32x32 RGBA8 LUT must detile all 32 slices");
        const auto& rg32f = layouts[1];
        Require(rg32f[0].blockDepth == 16 && rg32f[0].blocksPerRow == 1 && rg32f[0].sliceStride == 65536, "the 4x4x64 RG32F volume must use 32x16x16 thick blocks");
        const auto& tailOnly = layouts[9];
        Require(tailOnly[0].tail && tailOnly[0].tailX == 16 && tailOnly[0].tailY == 0 && tailOnly[1].tailX == 0 && tailOnly[1].tailY == 8 && tailOnly[2].tailX == 8 && tailOnly[2].tailY == 0, "a 4x4x64 RG32F mip chain must sit in the addrlib thick mip tail");
        const auto& chain = layouts[2];
        Require(!chain[1].tail && chain[2].tail && chain[0].tiledOffset == 131072 && chain[1].tiledOffset == 65536 && chain[0].sliceStride == 393216, "64KB thick mip chains must place the tail block first in every slab");
        Require(chain[2].tailX == 16 && chain[3].tailY == 16 && chain[4].tailX == 8 && chain[5].tailX == 4 && chain[5].tailY == 8 && chain[6].tailY == 12, "64KB thick mip tail coordinates must match addrlib");
        const auto& small = layouts[10];
        Require(!small[0].tail && small[1].tail && small[1].tailY == 8 && small[2].tailX == 8 && small[2].tailY == 4 && small[0].tiledOffset == 4096, "4KB thick mip tail coordinates must match addrlib");
    }

    for (const auto tileMode : {TextureTileMode::RenderTarget64KB, TextureTileMode::Depth64KB, TextureTileMode::kStandard256B, TextureTileMode::kLinear}) {
        const auto descriptor = volume(tileMode, 56, 32, 32, 32, 1);
        const auto mips = ComputeMipLayout(descriptor);
        const auto slices = ComputeMipLayout(tileMode, 56, 32, 32, 1);
        Require(mips.size() == 1 && mips[0].blockDepth == 1 && mips[0].depth == 1 && mips[0].tiledSize == slices[0].tiledSize, "3D Z, R and 256B swizzles must keep the thin per-slice layout");
        Require(ComputeSurfaceSize(mips, 32) == ComputeSurfaceSize(slices, 1) * 32, "thin 3D surfaces must stay one slice per depth level");
    }
    {
        auto descriptor = volume(TextureTileMode::kStandard64KB, 56, 64, 64, 6, 7);
        descriptor.dimension = TextureDimension::k2DArray;
        descriptor.viewDimension = TextureDimension::k2DArray;
        const auto layered = ComputeMipLayout(descriptor);
        const auto plain = ComputeMipLayout(TextureTileMode::kStandard64KB, 56, 64, 64, 7);
        Require(layered.size() == plain.size(), "array textures must keep the 2D mip chain");
        for (std::size_t level = 0; level < plain.size(); ++level) {
            Require(layered[level].tiledOffset == plain[level].tiledOffset && layered[level].tiledSize == plain[level].tiledSize && layered[level].depth == 1 && layered[level].blockDepth == 1, "array textures must keep the 2D mip chain");
        }
        Require(ComputeSurfaceSize(layered, 6) == ComputeSurfaceSize(plain, 1) * 6, "array texture surfaces must stay one slice per layer");
    }
    reject([] { ThickSwizzleMasks(TextureTileMode::RenderTarget64KB, 4); }, "thick volume tiling applies only");
    reject([] { ThickSwizzleMasks(TextureTileMode::kStandard64KB, 12); }, "unsupported bytes per element");
}

constexpr std::string_view kAddrlibZOrder64KBPatterns[5] = {
    "X0 Y0 X1 Y1 X2 Y2 X3 Y4 X3^Y3^S3 X4^Y4^S2 X6^Y5^S1 X5^Y6^S0 Y6 X6 Y7 X7",
    "0 X0 Y0 X1 Y1 X2 Y2 X3 X3^Y3^S3 X4^Y4^S2 X6^Y5^S1 X5^Y6^S0 Y4 X6 Y6 X7",
    "0 0 X0 Y0 X1 Y1 X2 Y2 X3^Y3^S3 X4^Y4^S2 X6^Y5^S1 X5^Y6^S0 Y3 X4 Y6 X6",
    "0 0 0 X0 Y0 X1 Y1 X2 X3^Y3^S3 X4^Y4^S2 X6^Y5^S1 X5^Y6^S0 Y2 X3 Y4 X6",
    "0 0 0 0 X0 Y0 X1 Y1 X3^Y3^S3 X4^Y4^S2 X6^Y5^S1 X5^Y6^S0 Y2 X2 Y3 X4",
};

constexpr std::array<std::array<std::uint32_t, 2>, 5> kAddrlibBlock64KThinLog2{{{8, 8}, {8, 7}, {7, 7}, {7, 6}, {6, 6}}};

std::uint32_t equationOffset(const TileSwizzleEquation& equation, std::uint32_t x, std::uint32_t y, std::uint32_t slice) {
    std::uint32_t offset = 0;
    for (std::uint32_t bit = 0; bit < 8u; ++bit) {
        if (((x >> bit) & 1u) != 0) offset ^= equation.x[bit];
        if (((y >> bit) & 1u) != 0) offset ^= equation.y[bit];
    }
    for (std::uint32_t bit = 0; bit < 4u; ++bit) {
        if (((slice >> bit) & 1u) != 0) offset ^= equation.slice[bit];
    }
    return offset;
}

struct AddrlibZOrderSurface {
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t mipCount;
    std::uint32_t layers;
    bool volume;
    std::uint64_t surfaceBytes;
    std::uint32_t firstTailLevel;
};

constexpr AddrlibZOrderSurface kAddrlibZOrderSurfaces[] = {
    {1, 256, 256, 9, 1, false, 196608, 2},
    {1, 300, 200, 9, 3, false, 983040, 3},
    {5, 1280, 720, 2, 1, false, 1376256, 2},
    {1, 37, 21, 6, 2, false, 131072, 0},
    {14, 256, 128, 9, 1, false, 196608, 2},
    {7, 200, 100, 8, 4, false, 786432, 2},
    {7, 130, 129, 8, 1, false, 262144, 2},
    {14, 97, 66, 3, 10, false, 1310720, 1},
    {22, 64, 64, 7, 6, false, 393216, 0},
    {22, 640, 360, 1, 10, false, 9830400, 1},
    {56, 130, 129, 8, 2, false, 786432, 2},
    {56, 257, 17, 9, 3, false, 1376256, 3},
    {64, 130, 129, 5, 2, false, 1179648, 2},
    {64, 65, 31, 7, 1, false, 131072, 1},
    {77, 100, 60, 7, 3, false, 786432, 2},
    {77, 40, 40, 6, 17, false, 2228224, 1},
    {7, 512, 512, 1, 4, false, 2097152, 1},
    {22, 1280, 720, 1, 2, false, 7864320, 1},
    {22, 40, 30, 3, 5, true, 327680, 0},
};

GuestTextureResource zOrderDescriptor(const AddrlibZOrderSurface& surface) {
    auto descriptor = volume(TextureTileMode::Depth64KB, surface.format, surface.width, surface.height, surface.layers, surface.mipCount);
    if (!surface.volume) {
        descriptor.dimension = TextureDimension::k2DArray;
        descriptor.viewDimension = TextureDimension::k2DArray;
    }
    return descriptor;
}

std::array<std::uint32_t, 8> depthDescriptorWords(std::uint32_t swizzleMode) {
    constexpr std::uint32_t width = 2560;
    constexpr std::uint32_t height = 1440;
    std::array<std::uint32_t, 8> words{};
    words[0] = static_cast<std::uint32_t>(0x1468930000ull >> 8u);
    words[1] = (22u << 20u) | (((width - 1u) & 3u) << 30u);
    words[2] = ((width - 1u) >> 2u) | ((height - 1u) << 14u);
    words[3] = 4u | (5u << 3u) | (6u << 6u) | (7u << 9u) | (swizzleMode << 20u) | (9u << 28u);
    return words;
}

void zOrderTilingTests() {
    for (std::uint32_t index = 0; index < 5; ++index) {
        const auto bytesPerElement = 1u << index;
        const auto equation = ZOrderSwizzleEquation(bytesPerElement);
        const auto& pattern = kAddrlibZOrder64KBPatterns[index];
        for (std::uint32_t bit = 0; bit < 16u; ++bit) {
            const auto unit = 1u << bit;
            Require(equationOffset(equation, unit, 0, 0) == addrlibPatternOffset(pattern, 16, unit, 0, 0) && equationOffset(equation, 0, unit, 0) == addrlibPatternOffset(pattern, 16, 0, unit, 0) && equationOffset(equation, 0, 0, unit) == addrlibPatternOffset(pattern, 16, 0, 0, unit), "every x, y and slice bit of the Z-order swizzle must match the addrlib GFX10 SW_64KB_Z_X pattern for 16 pipes");
        }
        const auto blockWidth = 1u << kAddrlibBlock64KThinLog2[index][0];
        const auto blockHeight = 1u << kAddrlibBlock64KThinLog2[index][1];
        std::vector<bool> seen(65536u / bytesPerElement);
        for (const std::uint32_t slice : {0u, 5u, 10u, 15u}) {
            std::fill(seen.begin(), seen.end(), false);
            for (std::uint32_t y = 0; y < blockHeight; ++y) {
                for (std::uint32_t x = 0; x < blockWidth; ++x) {
                    const auto offset = equationOffset(equation, x, y, slice);
                    Require(offset == addrlibPatternOffset(pattern, 16, x, y, slice), "Z-order swizzle offsets must match the addrlib SW_64KB_Z_X pattern");
                    Require(offset % bytesPerElement == 0 && offset < 65536u && !seen[offset / bytesPerElement], "the Z-order swizzle must place every element of a 64KB block at its own element-aligned offset");
                    seen[offset / bytesPerElement] = true;
                }
            }
        }
    }
    reject([] { ZOrderSwizzleEquation(3); }, "unsupported bytes per element");
    reject([] { ZOrderSwizzleEquation(32); }, "unsupported bytes per element");

    for (const auto& reference : kAddrlibZOrderSurfaces) {
        const auto mips = ComputeMipLayout(zOrderDescriptor(reference));
        Require(mips.size() == reference.mipCount && ComputeSurfaceSize(mips, reference.layers) == reference.surfaceBytes, "64KB_Z_X surface sizes must match addrlib for the GFX1013 configuration");
        for (std::uint32_t level = 0; level < reference.mipCount; ++level) {
            Require(mips[level].tail == (level >= reference.firstTailLevel) && mips[level].blockDepth == 1 && mips[level].depth == 1, "64KB_Z_X mip chains must enter the mip tail at the addrlib level and stay thin");
        }
    }
    {
        const auto zOrder = ComputeMipLayout(zOrderDescriptor(kAddrlibZOrderSurfaces[0]));
        const auto renderTarget = ComputeMipLayout(TextureTileMode::RenderTarget64KB, 1, 256, 256, 9);
        Require(!zOrder[1].tail && zOrder[2].tail && renderTarget[1].tail, "8-bit Z-order mip chains must enter the mip tail one level later than the other 64KB swizzles");
        Require(zOrder[0].tiledOffset == 131072 && zOrder[1].tiledOffset == 65536 && zOrder[2].tiledOffset == 0 && zOrder[2].tailX == 128 && zOrder[2].tailY == 0 && zOrder[3].tailX == 0 && zOrder[3].tailY == 128, "the 8-bit Z-order mip chain must match the addrlib mip offsets and tail coordinates");
        const auto sixteenBit = ComputeMipLayout(zOrderDescriptor(kAddrlibZOrderSurfaces[4]));
        Require(!sixteenBit[1].tail && sixteenBit[2].tail && ComputeMipLayout(TextureTileMode::RenderTarget64KB, 14, 256, 128, 9)[1].tail, "16-bit Z-order mip chains must enter the mip tail one level later than the other 64KB swizzles");
        const auto depth = ComputeMipLayout(zOrderDescriptor(kAddrlibZOrderSurfaces[8]));
        const auto renderDepth = ComputeMipLayout(TextureTileMode::RenderTarget64KB, 22, 64, 64, 7);
        for (std::size_t level = 0; level < depth.size(); ++level) Require(depth[level].tail && depth[level].tailX == renderDepth[level].tailX && depth[level].tailY == renderDepth[level].tailY && depth[level].tiledOffset == renderDepth[level].tiledOffset, "32-bit Z-order mip chains must keep the 64KB mip tail of the other swizzles");
    }

    Require(DecodeTextureResource(depthDescriptorWords(0x18)).tileMode == TextureTileMode::Depth64KB, "swizzle mode 0x18 (SW_64KB_Z_X) must decode to the Z-order tile mode");
    reject([] { DecodeTextureResource(depthDescriptorWords(0x08)); }, "unsupported tile mode 8");
}

}

void RunTextureTilingTests() {
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 2);
        Require(mips.size() == 2, "linear mip chain must contain the requested mip count");
        Require(mips[0].tiledOffset == 512 && mips[0].tiledSize == 1024, "linear mip 0 offset or size changed");
        Require(mips[0].width == 4 && mips[0].height == 4, "linear mip 0 dimensions changed");
        Require(mips[0].blocksPerRow == 256 && mips[0].pitchBytes == 256, "linear mip 0 row layout changed");
        Require(!mips[0].tail, "linear mips must never fall into a mip tail");
        Require(mips[1].tiledOffset == 0 && mips[1].tiledSize == 512, "linear mip 1 offset or size changed");
        Require(mips[1].width == 2 && mips[1].height == 2, "linear mip 1 dimensions changed");
        Require(mips[1].linearOffset == mips[1].tiledOffset && mips[1].linearSize == mips[1].tiledSize, "linear tiling must keep linear and tiled layout identical");
    }
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kLinear, 169, 8, 8, 1);
        Require(mips.size() == 1, "compressed linear layout must contain one mip");
        Require(mips[0].width == 2 && mips[0].height == 2, "compressed linear mip block dimensions changed");
        Require(mips[0].blocksPerRow == 32 && mips[0].pitchBytes == 256, "compressed linear mip row layout changed");
        Require(mips[0].tiledSize == 512 && mips[0].linearSize == 512, "compressed linear mip size changed");
    }
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kStandard256B, 1, 64, 64, 1);
        Require(mips.size() == 1, "standard 256B layout must contain one mip");
        Require(mips[0].tiledOffset == 0 && mips[0].tiledSize == 4096, "standard 256B mip 0 offset or size changed");
        Require(mips[0].width == 64 && mips[0].height == 64, "standard 256B mip 0 dimensions changed");
        Require(mips[0].blocksPerRow == 4 && mips[0].pitchBytes == 64, "standard 256B mip 0 row layout changed");
        Require(!mips[0].tail, "standard 256B textures must never use a mip tail");

        const auto surfaceSize = ComputeSurfaceSize(mips, 3);
        Require(surfaceSize == 4096ull * 3ull, "surface size must multiply the slice size by the array layer count");
    }
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kStandard256B, 1, 32, 32, 6);
        Require(mips.size() == 6, "standard 256B mip chain must contain the requested mip count");
        for (const auto& mip : mips) Require(!mip.tail, "standard 256B tile mode must never produce a mip tail");
    }
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kStandard64KB, 1, 1024, 1024, 11);
        Require(mips.size() == 11, "standard 64KB mip chain must contain the requested mip count");
        auto tailSeen = false;
        for (const auto& mip : mips) {
            Require(mip.width != 0 && mip.height != 0, "every standard 64KB mip must have nonzero dimensions");
            Require(mip.tiledSize != 0 && mip.linearSize != 0, "every standard 64KB mip must have a nonzero size");
            if (mip.tail) {
                tailSeen = true;
                Require(mip.blocksPerRow == 1, "mip tail levels must report a single block per row");
                Require(mip.tiledOffset == 0, "mip tail levels must share the tiled tail block offset");
            }
        }
        Require(tailSeen, "a deep standard 64KB mip chain must fall into the mip tail");
        Require(!mips.front().tail, "the base level of a deep mip chain must not be in the mip tail");
    }
    {
        const auto mips = ComputeMipLayout(TextureTileMode::kStandard4KB, 1, 512, 512, 10);
        Require(mips.size() == 10, "standard 4KB mip chain must contain the requested mip count");
        auto tailSeen = false;
        for (const auto& mip : mips) {
            if (mip.tail) tailSeen = true;
        }
        Require(tailSeen, "a deep standard 4KB mip chain must fall into the mip tail");
    }

    {
        const auto mips = ComputeMipLayout(TextureTileMode::RenderTarget64KB, 56, 257, 129, 1);
        Require(mips[0].blocksPerRow == 3 && mips[0].tiledSize == 393216, "render target surfaces must pad to complete 128 by 128 blocks for 32-bit pixels");
        Require(mips[0].pitchBytes == 1028 && mips[0].linearSize == 132612, "detiled render target rows must use the actual texture width");
        Require(ComputeSurfaceSize(mips, 6) == 2359296, "render target cube faces must retain the padded guest slice stride");
    }
    for (const auto format : std::array<std::uint32_t, 5>{1, 7, 56, 71, 77}) {
        const auto mips = ComputeMipLayout(TextureTileMode::RenderTarget64KB, format, 1024, 513, 11);
        std::uint64_t linearEnd = 0;
        bool tailSeen = false;
        for (const auto& mip : mips) {
            Require(mip.linearOffset >= linearEnd && mip.linearOffset % 4 == 0, "detiled mip levels must occupy separate word-aligned ranges");
            Require(mip.linearSize >= static_cast<std::uint64_t>(mip.pitchBytes) * mip.height, "detiled mip allocation must contain every row");
            Require(mip.linearSize % 4 == 0, "detiled mip sizes must preserve word alignment between array layers");
            linearEnd = mip.linearOffset + mip.linearSize;
            if (mip.tail) {
                tailSeen = true;
                Require(mip.tiledOffset == 0 && mip.tiledSize == 65536, "render target mip tails must share one guest 64KB block");
            }
        }
        Require(tailSeen && !mips.front().tail, "render target mip chains must cover both regular blocks and mip tails");
    }
    reject([] { ComputeMipLayout(TextureTileMode::RenderTarget64KB, 169, 64, 64, 1); }, "block compressed formats");
    reject([] { ComputeMipLayout(TextureTileMode::RenderTarget64KB, 132, 64, 64, 1); }, "does not support render target tiling");
    reject([] { ComputeMipLayout(TextureTileMode::RenderTarget64KB, 74, 64, 64, 1); }, "unsupported bytes per element");

    reject([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 0, 4, 1); }, "zero-sized texture");
    reject([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 0, 1); }, "zero-sized texture");
    reject([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 0); }, "mip count is out of range");
    reject([] { ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 17); }, "mip count is out of range");

    reject([] { ComputeSurfaceSize({}, 1); }, "empty mip chain");
    reject([] { ComputeSurfaceSize(ComputeMipLayout(TextureTileMode::kLinear, 1, 4, 4, 1), 0); }, "zero array layers");
    volumeTilingTests();
    zOrderTilingTests();
}

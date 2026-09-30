#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace AgcDriver::Graphics {

namespace {

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment) {
    return (value + alignment - 1u) / alignment * alignment;
}

std::uint32_t ShiftCeil(std::uint32_t value, std::uint32_t shift) {
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(value) + (1ull << shift) - 1ull) >> shift);
}

std::uint32_t CalcLinearBlockWidth(std::uint32_t bytesPerElement) {
    return 256u / bytesPerElement;
}

struct BlockLayout {
    std::uint32_t blockSize;
    std::uint32_t blockWidth;
    std::uint32_t blockHeight;
    std::uint32_t blockDepth;
};

struct Log2BlockDimensions {
    std::uint8_t width;
    std::uint8_t height;
};

constexpr Log2BlockDimensions kLog2BlockThin256B[] = {{4, 4}, {4, 3}, {3, 3}, {3, 2}, {2, 2}};
constexpr Log2BlockDimensions kLog2BlockThin4KB[] = {{6, 6}, {6, 5}, {5, 5}, {5, 4}, {4, 4}};
constexpr Log2BlockDimensions kLog2BlockThin64KB[] = {{8, 8}, {8, 7}, {7, 7}, {7, 6}, {6, 6}};

BlockLayout GetBlockLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement) {
    Require(std::has_single_bit(bytesPerElement) && bytesPerElement <= 16u, "unsupported bytes per element for tiled texture geometry");
    const auto index = static_cast<std::size_t>(std::countr_zero(bytesPerElement));
    switch (tileMode) {
        case TextureTileMode::kLinear: throw std::runtime_error("AGC graphics: GetBlockLayout does not apply to linear tiling");
        case TextureTileMode::kStandard256B: return {256u, 1u << kLog2BlockThin256B[index].width, 1u << kLog2BlockThin256B[index].height, 1u};
        case TextureTileMode::kStandard4KB: return {4096u, 1u << kLog2BlockThin4KB[index].width, 1u << kLog2BlockThin4KB[index].height, 1u};
        case TextureTileMode::RenderTarget64KB:
        case TextureTileMode::Depth64KB:
        case TextureTileMode::kStandard64KB: return {65536u, 1u << kLog2BlockThin64KB[index].width, 1u << kLog2BlockThin64KB[index].height, 1u};
    }
    throw std::runtime_error("AGC graphics: GetBlockLayout encountered an unknown tile mode");
}

enum class SwizzleChannel : std::uint8_t {
    kElement,
    kX,
    kY,
    kZ
};

struct SwizzleBit {
    SwizzleChannel channel;
    std::uint8_t index;
};

constexpr SwizzleBit kElementByte{SwizzleChannel::kElement, 0};

constexpr SwizzleBit X(std::uint8_t index) {
    return {SwizzleChannel::kX, index};
}

constexpr SwizzleBit Y(std::uint8_t index) {
    return {SwizzleChannel::kY, index};
}

constexpr SwizzleBit Z(std::uint8_t index) {
    return {SwizzleChannel::kZ, index};
}

constexpr SwizzleBit kThickStandardSwizzle[5][16] = {
    {X(0), X(1), Z(0), Y(0), Z(1), Y(1), X(2), Z(2), Y(2), X(3), Z(3), Y(3), X(4), Z(4), Y(4), X(5)},
    {kElementByte, X(0), Z(0), Y(0), Z(1), Y(1), X(1), Z(2), Y(2), X(2), Z(3), Y(3), X(3), Z(4), Y(4), X(4)},
    {kElementByte, kElementByte, X(0), Y(0), Z(0), Y(1), X(1), Z(1), Y(2), X(2), Z(2), Y(3), X(3), Z(3), Y(4), X(4)},
    {kElementByte, kElementByte, kElementByte, X(0), Z(0), Y(0), X(1), Z(1), Y(1), X(2), Z(2), Y(2), X(3), Z(3), Y(3), X(4)},
    {kElementByte, kElementByte, kElementByte, kElementByte, Z(0), Y(0), X(0), Z(1), Y(1), X(1), Z(2), Y(2), X(2), Z(3), Y(3), X(3)},
};

constexpr bool DepositsInOrder(const SwizzleBit (&pattern)[16], std::uint32_t elementBits) {
    std::uint8_t next[4] = {};
    for (std::uint32_t bit = 0; bit < 16u; ++bit) {
        const auto channel = static_cast<std::size_t>(pattern[bit].channel);
        if ((pattern[bit].channel == SwizzleChannel::kElement) != (bit < elementBits)) return false;
        if (pattern[bit].channel != SwizzleChannel::kElement && pattern[bit].index != next[channel]++) return false;
    }
    return true;
}

static_assert(DepositsInOrder(kThickStandardSwizzle[0], 0) && DepositsInOrder(kThickStandardSwizzle[1], 1) && DepositsInOrder(kThickStandardSwizzle[2], 2) && DepositsInOrder(kThickStandardSwizzle[3], 3) && DepositsInOrder(kThickStandardSwizzle[4], 4));

std::uint32_t ThickBlockBits(TextureTileMode tileMode) {
    switch (tileMode) {
        case TextureTileMode::kStandard4KB: return 12u;
        case TextureTileMode::kStandard64KB: return 16u;
        case TextureTileMode::kLinear:
        case TextureTileMode::kStandard256B:
        case TextureTileMode::RenderTarget64KB:
        case TextureTileMode::Depth64KB: break;
    }
    throw std::runtime_error("AGC graphics: thick volume tiling applies only to the standard 4KB and 64KB swizzle modes");
}

BlockLayout GetThickBlockLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement) {
    const auto masks = ThickSwizzleMasks(tileMode, bytesPerElement);
    return {1u << ThickBlockBits(tileMode), 1u << std::popcount(masks.x), 1u << std::popcount(masks.y), 1u << std::popcount(masks.z)};
}

bool UsesThickTiling(const GuestTextureResource& descriptor) {
    return descriptor.dimension == TextureDimension::k3D && (descriptor.tileMode == TextureTileMode::kStandard4KB || descriptor.tileMode == TextureTileMode::kStandard64KB);
}

struct MipTailLocation {
    std::uint32_t x;
    std::uint32_t y;
};

constexpr MipTailLocation kMipTailThin4KB[5][8] = {
    {{32, 0}, {16, 32}, {0, 48}, {0, 32}, {16, 16}, {16, 0}, {0, 16}, {0, 0}},
    {{32, 0}, {16, 16}, {0, 24}, {0, 16}, {16, 8}, {16, 0}, {0, 8}, {0, 0}},
    {{16, 0}, {8, 16}, {0, 24}, {0, 16}, {8, 8}, {8, 0}, {0, 8}, {0, 0}},
    {{16, 0}, {8, 8}, {0, 12}, {0, 8}, {8, 4}, {8, 0}, {0, 4}, {0, 0}},
    {{8, 0}, {4, 8}, {0, 12}, {0, 8}, {4, 4}, {4, 0}, {0, 4}, {0, 0}},
};

constexpr MipTailLocation kMipTailThin64KB[5][12] = {
    {{128, 0}, {0, 128}, {64, 0}, {0, 64}, {32, 0}, {16, 32}, {0, 48}, {0, 32}, {16, 16}, {16, 0}, {0, 16}, {0, 0}},
    {{128, 0}, {0, 64}, {64, 0}, {0, 32}, {32, 0}, {16, 16}, {0, 24}, {0, 16}, {16, 8}, {16, 0}, {0, 8}, {0, 0}},
    {{64, 0}, {0, 64}, {32, 0}, {0, 32}, {16, 0}, {8, 16}, {0, 24}, {0, 16}, {8, 8}, {8, 0}, {0, 8}, {0, 0}},
    {{64, 0}, {0, 32}, {32, 0}, {0, 16}, {16, 0}, {8, 8}, {0, 12}, {0, 8}, {8, 4}, {8, 0}, {0, 4}, {0, 0}},
    {{32, 0}, {0, 32}, {16, 0}, {0, 16}, {8, 0}, {4, 8}, {0, 12}, {0, 8}, {4, 4}, {4, 0}, {0, 4}, {0, 0}},
};

constexpr MipTailLocation kMipTailThick4KB[5][5] = {
    {{0, 8}, {8, 4}, {8, 0}, {0, 4}, {0, 0}},
    {{0, 8}, {4, 4}, {4, 0}, {0, 4}, {0, 0}},
    {{0, 8}, {4, 4}, {4, 0}, {0, 4}, {0, 0}},
    {{0, 4}, {4, 2}, {4, 0}, {0, 2}, {0, 0}},
    {{0, 4}, {2, 2}, {2, 0}, {0, 2}, {0, 0}},
};

constexpr MipTailLocation kMipTailThick64KB[5][10] = {
    {{32, 0}, {0, 16}, {16, 0}, {8, 8}, {0, 12}, {0, 8}, {8, 4}, {8, 0}, {0, 4}, {0, 0}},
    {{16, 0}, {0, 16}, {8, 0}, {4, 8}, {0, 12}, {0, 8}, {4, 4}, {4, 0}, {0, 4}, {0, 0}},
    {{16, 0}, {0, 16}, {8, 0}, {4, 8}, {0, 12}, {0, 8}, {4, 4}, {4, 0}, {0, 4}, {0, 0}},
    {{16, 0}, {0, 8}, {8, 0}, {4, 4}, {0, 6}, {0, 4}, {4, 2}, {4, 0}, {0, 2}, {0, 0}},
    {{8, 0}, {0, 8}, {4, 0}, {2, 4}, {0, 6}, {0, 4}, {2, 2}, {2, 0}, {0, 2}, {0, 0}},
};

struct MipTailLayout {
    const MipTailLocation* locations;
    std::uint32_t maxLevels;
    std::uint32_t widthLimit;
    std::uint32_t heightLimit;
};

template<std::size_t TLevels>
MipTailLayout MakeMipTailLayout(const MipTailLocation (&locations)[TLevels], std::uint32_t widthLimit, std::uint32_t heightLimit) {
    return {locations, static_cast<std::uint32_t>(TLevels), widthLimit, heightLimit};
}

bool GetMipTailLayout(TextureTileMode tileMode, const BlockLayout& block, std::uint32_t bytesPerElement, MipTailLayout& out) {
    const auto index = static_cast<std::size_t>(std::countr_zero(bytesPerElement));
    if (block.blockDepth > 1u) {
        out = tileMode == TextureTileMode::kStandard4KB ? MakeMipTailLayout(kMipTailThick4KB[index], block.blockWidth, block.blockHeight >> 1u) : MakeMipTailLayout(kMipTailThick64KB[index], block.blockWidth >> 1u, block.blockHeight);
        return true;
    }
    switch (tileMode) {
        case TextureTileMode::kLinear:
        case TextureTileMode::kStandard256B: return false;
        case TextureTileMode::kStandard4KB:
            out = MakeMipTailLayout(kMipTailThin4KB[index], block.blockWidth >> 1u, block.blockHeight);
            return true;
        case TextureTileMode::RenderTarget64KB:
        case TextureTileMode::Depth64KB:
        case TextureTileMode::kStandard64KB:
            out = MakeMipTailLayout(kMipTailThin64KB[index], block.blockWidth >> 1u, block.blockHeight);
            return true;
    }
    throw std::runtime_error("AGC graphics: GetMipTailLayout encountered an unknown tile mode");
}

std::uint32_t TexelLevelDimension(std::uint32_t guestDimension, std::uint32_t level, std::uint32_t texelScale) {
    return std::max((std::max(guestDimension >> level, 1u) + texelScale - 1u) / texelScale, 1u);
}

std::vector<TileMipLayout> ComputeLinearMipLayout(std::uint32_t bytesPerElement, std::uint32_t texelWidth, std::uint32_t texelHeight, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount) {
    const auto compressed = texelWidth != 1u || texelHeight != 1u;
    const auto elementsWidth0 = (width + texelWidth - 1u) / texelWidth;
    const auto elementsHeight0 = (height + texelHeight - 1u) / texelHeight;
    const auto blockWidth = CalcLinearBlockWidth(bytesPerElement);

    std::vector<TileMipLayout> mips(mipCount);
    std::uint64_t offset = 0;
    for (auto level = mipCount; level-- > 0;) {
        const auto elementsLevelWidth = std::max(ShiftCeil(elementsWidth0, level), 1u);
        const auto elementsLevelHeight = std::max(ShiftCeil(elementsHeight0, level), 1u);
        const auto paddedElementsWidth = AlignUp(elementsLevelWidth, blockWidth);
        const auto size = static_cast<std::uint64_t>(paddedElementsWidth) * elementsLevelHeight * bytesPerElement;
        Require(size != 0, "computed a zero-sized linear texture mip level");

        auto pitchBytes = paddedElementsWidth * bytesPerElement;
        if (compressed) pitchBytes = std::max(pitchBytes, 32u);

        auto& mip = mips[level];
        mip.tiledOffset = offset;
        mip.tiledSize = size;
        mip.linearOffset = offset;
        mip.linearSize = size;
        mip.width = TexelLevelDimension(width, level, texelWidth);
        mip.height = TexelLevelDimension(height, level, texelHeight);
        mip.depth = 1u;
        mip.blocksPerRow = paddedElementsWidth;
        mip.pitchBytes = pitchBytes;
        mip.tail = false;
        mip.tailX = 0;
        mip.tailY = 0;
        mip.blockDepth = 1u;
        offset += size;
    }
    for (auto& mip : mips) mip.sliceStride = offset;
    return mips;
}

std::vector<TileMipLayout> ComputeTiledMipLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement, std::uint32_t texelWidth, std::uint32_t texelHeight, std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t mipCount, bool thick) {
    const auto block = thick ? GetThickBlockLayout(tileMode, bytesPerElement) : GetBlockLayout(tileMode, bytesPerElement);
    const auto elementsWidth0 = (width + texelWidth - 1u) / texelWidth;
    const auto elementsHeight0 = (height + texelHeight - 1u) / texelHeight;

    MipTailLayout tail{};
    const auto hasTail = GetMipTailLayout(tileMode, block, bytesPerElement, tail);

    auto firstTailLevel = mipCount;
    if (hasTail && mipCount > 1) {
        for (std::uint32_t level = 0; level < mipCount; ++level) {
            if (ShiftCeil(elementsWidth0, level) <= tail.widthLimit && ShiftCeil(elementsHeight0, level) <= tail.heightLimit && mipCount - level <= tail.maxLevels) {
                firstTailLevel = level;
                break;
            }
        }
    }

    std::vector<TileMipLayout> mips(mipCount);
    std::uint64_t blockSliceSize = 0;

    for (std::uint32_t level = 0; level < firstTailLevel; ++level) {
        auto& mip = mips[level];
        mip.width = TexelLevelDimension(width, level, texelWidth);
        mip.height = TexelLevelDimension(height, level, texelHeight);
        const auto paddedWidth = AlignUp(std::max(ShiftCeil(elementsWidth0, level), 1u), block.blockWidth);
        const auto paddedHeight = AlignUp(std::max(ShiftCeil(elementsHeight0, level), 1u), block.blockHeight);
        mip.blocksPerRow = paddedWidth / block.blockWidth;
        mip.tiledSize = static_cast<std::uint64_t>(paddedWidth) * paddedHeight * bytesPerElement * block.blockDepth;
        mip.linearSize = static_cast<std::uint64_t>(mip.width) * mip.height * bytesPerElement;
        mip.tail = false;
        mip.tailX = 0;
        mip.tailY = 0;
        blockSliceSize += mip.tiledSize;
    }

    if (firstTailLevel < mipCount) blockSliceSize += block.blockSize;

    for (auto level = firstTailLevel; level < mipCount; ++level) {
        auto& mip = mips[level];
        mip.width = TexelLevelDimension(width, level, texelWidth);
        mip.height = TexelLevelDimension(height, level, texelHeight);
        mip.blocksPerRow = 1u;
        mip.tiledSize = block.blockSize;
        mip.linearSize = static_cast<std::uint64_t>(mip.width) * mip.height * bytesPerElement;
        mip.tail = true;
        mip.tailX = tail.locations[level - firstTailLevel].x;
        mip.tailY = tail.locations[level - firstTailLevel].y;
    }

    std::uint64_t offset = firstTailLevel < mipCount ? block.blockSize : 0;
    for (auto level = firstTailLevel; level-- > 0;) {
        mips[level].tiledOffset = offset;
        offset += mips[level].tiledSize;
    }
    for (auto level = firstTailLevel; level < mipCount; ++level) {
        mips[level].tiledOffset = 0;
    }

    Require(offset == blockSliceSize, "tiled texture mip chain geometry is inconsistent");
    for (const auto& mip : mips) {
        Require(mip.width != 0 && mip.height != 0, "computed a zero-sized tiled texture mip level");
        Require(mip.tiledSize != 0 && mip.linearSize != 0, "computed a zero-sized tiled texture mip level");
    }
    std::uint64_t linearOffset = 0;
    for (std::uint32_t level = 0; level < mipCount; ++level) {
        auto& mip = mips[level];
        const auto alignment = std::max(bytesPerElement, 4u);
        mip.depth = thick ? std::max(depth >> level, 1u) : 1u;
        mip.blockDepth = block.blockDepth;
        mip.sliceStride = blockSliceSize;
        linearOffset = (linearOffset + alignment - 1u) / alignment * alignment;
        mip.linearOffset = linearOffset;
        mip.pitchBytes = mip.width * bytesPerElement;
        mip.linearSize = (static_cast<std::uint64_t>(mip.pitchBytes) * mip.height * mip.depth + alignment - 1u) / alignment * alignment;
        linearOffset += mip.linearSize;
    }
    return mips;
}

std::vector<TileMipLayout> ComputeLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t mipCount, bool thick) {
    Require(width != 0 && height != 0 && depth != 0, "cannot compute mip layout for a zero-sized texture");
    Require(mipCount != 0 && mipCount <= 16u, "texture mip count is out of range");

    const auto bytesPerElement = BytesPerElement(format);
    const auto texelWidth = BlockWidth(format);
    const auto texelHeight = BlockHeight(format);

    if (tileMode == TextureTileMode::RenderTarget64KB) {
        Require(!IsBlockCompressed(format), "render target tiling does not support block compressed formats");
        Require(format != 128 && format != 129 && format != 132, "texture format does not support render target tiling");
    }
    if (tileMode == TextureTileMode::kLinear) return ComputeLinearMipLayout(bytesPerElement, texelWidth, texelHeight, width, height, mipCount);
    return ComputeTiledMipLayout(tileMode, bytesPerElement, texelWidth, texelHeight, width, height, depth, mipCount, thick);
}

}

std::vector<TileMipLayout> ComputeMipLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount) {
    return ComputeLayout(tileMode, format, width, height, 1u, mipCount, false);
}

std::vector<TileMipLayout> ComputeMipLayout(const GuestTextureResource& descriptor) {
    const auto thick = UsesThickTiling(descriptor);
    return ComputeLayout(descriptor.tileMode, descriptor.format, descriptor.width, descriptor.height, thick ? descriptor.depthOrLastArray + 1u : 1u, descriptor.mipCount, thick);
}

TileSwizzleMasks ThickSwizzleMasks(TextureTileMode tileMode, std::uint32_t bytesPerElement) {
    Require(std::has_single_bit(bytesPerElement) && bytesPerElement <= 16u, "unsupported bytes per element for thick volume tiling");
    const auto blockBits = ThickBlockBits(tileMode);
    const auto& pattern = kThickStandardSwizzle[std::countr_zero(bytesPerElement)];
    TileSwizzleMasks masks{0, 0, 0};
    for (std::uint32_t bit = 0; bit < blockBits; ++bit) {
        switch (pattern[bit].channel) {
            case SwizzleChannel::kElement: break;
            case SwizzleChannel::kX: masks.x |= 1u << bit; break;
            case SwizzleChannel::kY: masks.y |= 1u << bit; break;
            case SwizzleChannel::kZ: masks.z |= 1u << bit; break;
        }
    }
    return masks;
}

std::uint64_t DepthSliceBytes(std::uint32_t bytesPerElement, std::uint32_t width, std::uint32_t height) {
    const auto block = GetBlockLayout(TextureTileMode::Depth64KB, bytesPerElement);
    const auto columns = (static_cast<std::uint64_t>(width) + block.blockWidth - 1u) / block.blockWidth;
    const auto rows = (static_cast<std::uint64_t>(height) + block.blockHeight - 1u) / block.blockHeight;
    return columns * rows * block.blockSize;
}

std::uint64_t ComputeSurfaceSize(const std::vector<TileMipLayout>& mips, std::uint32_t arrayLayers) {
    Require(!mips.empty(), "cannot compute surface size for an empty mip chain");
    Require(arrayLayers != 0, "cannot compute surface size for zero array layers");

    std::uint64_t sliceSize = 0;
    for (const auto& mip : mips) {
        Require(mip.tiledSize != 0, "encountered a zero-sized mip level while computing surface size");
        sliceSize = std::max(sliceSize, mip.tiledOffset + mip.tiledSize);
    }

    const auto blockDepth = std::max(mips.front().blockDepth, 1u);
    const auto slices = arrayLayers / blockDepth + (arrayLayers % blockDepth != 0 ? 1u : 0u);
    Require(sliceSize <= UINT64_MAX / slices, "tiled texture surface size overflows");
    return sliceSize * slices;
}

}

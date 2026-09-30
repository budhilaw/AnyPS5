#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

struct Push {
    std::uint32_t srcBase;
    std::uint32_t dstBase;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pitchBytes;
    std::uint32_t blocksPerRow;
    std::uint32_t tail;
    std::uint32_t tailX;
    std::uint32_t tailY;
    std::uint32_t elementBytes;
    std::uint32_t arrayLayer;
    std::uint32_t depth;
    std::uint32_t sliceStride;
    std::uint32_t swizzleX;
    std::uint32_t swizzleY;
    std::uint32_t swizzleZ;
    std::uint32_t encode;
};

Push decodePush(const std::vector<std::byte>& bytes) {
    Push push{};
    Require(bytes.size() == sizeof(Push), "unexpected texture detiling push constant size");
    std::memcpy(&push, bytes.data(), sizeof(Push));
    return push;
}

TileMipLayout makeLayout(std::uint32_t width, std::uint32_t height, std::uint32_t pitchBytes, std::uint32_t blocksPerRow, std::uint64_t tiledSize, std::uint64_t linearSize) {
    TileMipLayout layout{};
    layout.width = width;
    layout.height = height;
    layout.depth = 1;
    layout.pitchBytes = pitchBytes;
    layout.blocksPerRow = blocksPerRow;
    layout.tiledSize = tiledSize;
    layout.linearSize = linearSize;
    layout.tail = false;
    layout.tailX = 0;
    layout.tailY = 0;
    layout.blockDepth = 1;
    return layout;
}

template<typename TAction>
void reject(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected texture detiler test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected texture detiler rejection: ") + std::string(reason));
}

std::uint32_t deposit(std::uint32_t value, std::uint32_t mask) {
    std::uint32_t result = 0;
    for (; mask != 0; mask &= mask - 1u, value >>= 1u) {
        if ((value & 1u) != 0) result |= mask & (~mask + 1u);
    }
    return result;
}

struct Swizzle {
    std::uint32_t elementBytes;
    std::uint32_t blockBytes;
    std::uint32_t family;
};

Swizzle expectedSwizzle(TextureTileMode tileMode, std::uint32_t elementBytes, bool thick) {
    switch (tileMode) {
        case TextureTileMode::kLinear: return {elementBytes, 0u, 0u};
        case TextureTileMode::kStandard256B: return {elementBytes, 256u, 1u};
        case TextureTileMode::kStandard4KB: return {elementBytes, 4096u, thick ? 3u : 1u};
        case TextureTileMode::kStandard64KB: return {elementBytes, 65536u, thick ? 3u : 1u};
        case TextureTileMode::RenderTarget64KB: return {elementBytes, 65536u, 2u};
        case TextureTileMode::Depth64KB: return {elementBytes, 65536u, 1u};
    }
    throw std::runtime_error("texture detiler test encountered an unknown tile mode");
}

std::uint32_t standardOffset(std::uint32_t elementBytes, std::uint32_t x, std::uint32_t y) {
    switch (elementBytes) {
        case 1: return ((y << 4) & 0x1f0u) ^ ((y << 5) & 0x400u) ^ (x & 0x00fu) ^ ((x << 5) & 0x200u) ^ ((x << 6) & 0x800u);
        case 2: return ((y << 4) & 0x070u) ^ ((y << 5) & 0x100u) ^ ((y << 6) & 0x400u) ^ ((x << 1) & 0x00eu) ^ ((x << 4) & 0x080u) ^ ((x << 5) & 0x200u) ^ ((x << 6) & 0x800u);
        case 4: return ((y << 4) & 0x070u) ^ ((y << 5) & 0x100u) ^ ((y << 6) & 0x400u) ^ ((x << 2) & 0x00cu) ^ ((x << 5) & 0x080u) ^ ((x << 6) & 0x200u) ^ ((x << 7) & 0x800u);
        case 8: return ((y << 4) & 0x030u) ^ ((y << 6) & 0x100u) ^ ((y << 7) & 0x400u) ^ ((x << 3) & 0x008u) ^ ((x << 5) & 0x0c0u) ^ ((x << 6) & 0x200u) ^ ((x << 7) & 0x800u);
        default: return ((y << 4) & 0x030u) ^ ((y << 6) & 0x100u) ^ ((y << 7) & 0x400u) ^ ((x << 6) & 0x0c0u) ^ ((x << 7) & 0x200u) ^ ((x << 8) & 0x800u);
    }
}

std::uint32_t standard64Extra(std::uint32_t elementBytes, std::uint32_t x, std::uint32_t y) {
    switch (elementBytes) {
        case 1: return ((x << 7) & 0x2000u) ^ ((x << 8) & 0x8000u) ^ ((y << 6) & 0x1000u) ^ ((y << 7) & 0x4000u);
        case 2: return ((x << 7) & 0x2000u) ^ ((x << 8) & 0x8000u) ^ ((y << 7) & 0x1000u) ^ ((y << 8) & 0x4000u);
        case 4: return ((x << 8) & 0x2000u) ^ ((x << 9) & 0x8000u) ^ ((y << 7) & 0x1000u) ^ ((y << 8) & 0x4000u);
        case 8: return ((x << 8) & 0x2000u) ^ ((x << 9) & 0x8000u) ^ ((y << 8) & 0x1000u) ^ ((y << 9) & 0x4000u);
        default: return ((x << 9) & 0x2000u) ^ ((x << 10) & 0x8000u) ^ ((y << 8) & 0x1000u) ^ ((y << 9) & 0x4000u);
    }
}

std::uint32_t renderTargetOffset(std::uint32_t elementBytes, std::uint32_t x, std::uint32_t y, std::uint32_t layer) {
    std::uint32_t offset = 0;
    switch (elementBytes) {
        case 1:
            offset = ((y << 2) & 0x0008u) ^ ((y << 4) & 0x0010u) ^ ((y << 3) & 0x00a0u) ^ ((y << 5) & 0x0f00u) ^ ((y << 6) & 0x1000u) ^ ((y << 7) & 0x4000u) ^
                     (x & 0x0007u) ^ ((x << 3) & 0x0040u) ^ ((x << 5) & 0x0300u) ^ ((x << 4) & 0x0400u) ^ ((x << 6) & 0x0800u) ^ ((x << 7) & 0x2000u) ^ ((x << 8) & 0x8000u);
            break;
        case 2:
            offset = ((y << 4) & 0x0070u) ^ ((y << 5) & 0x0f00u) ^ ((y << 8) & 0x5000u) ^
                     ((x << 1) & 0x000eu) ^ ((x << 4) & 0x0480u) ^ ((x << 5) & 0x0300u) ^ ((x << 6) & 0x0800u) ^ ((x << 7) & 0x2000u) ^ ((x << 8) & 0x8000u);
            break;
        case 4:
            offset = ((y << 4) & 0x0070u) ^ ((y << 5) & 0x0f00u) ^ ((y << 9) & 0x1000u) ^ ((y << 8) & 0x4000u) ^
                     ((x << 2) & 0x000cu) ^ ((x << 5) & 0x0380u) ^ ((x << 4) & 0x0400u) ^ ((x << 6) & 0x0800u) ^ ((x << 9) & 0xa000u);
            break;
        case 8:
            offset = ((y << 4) & 0x0010u) ^ ((y << 6) & 0x0080u) ^ ((y << 5) & 0x0f00u) ^ ((y << 10) & 0x5000u) ^
                     ((x << 3) & 0x0008u) ^ ((x << 4) & 0x0460u) ^ ((x << 5) & 0x0300u) ^ ((x << 6) & 0x0800u) ^ ((x << 10) & 0x2000u) ^ ((x << 9) & 0x8000u);
            break;
        case 16:
            offset = ((x << 4) & 0x0410u) ^ ((x << 5) & 0x0340u) ^ ((x << 6) & 0x0800u) ^ ((x << 11) & 0xa000u) ^
                     ((y << 5) & 0x0f20u) ^ ((y << 6) & 0x0080u) ^ ((y << 10) & 0x1000u) ^ ((y << 11) & 0x4000u);
            break;
    }
    return offset ^ ((layer & 8u) << 5u) ^ ((layer & 4u) << 7u) ^ ((layer & 2u) << 9u) ^ ((layer & 1u) << 11u);
}

std::uint32_t emulatedBlockWidth(const Swizzle& swizzle) {
    const auto small = swizzle.elementBytes <= 2u;
    const auto medium = swizzle.elementBytes <= 8u;
    if (swizzle.blockBytes <= 256u) return small ? 16u : medium ? 8u : 4u;
    if (swizzle.blockBytes <= 4096u) return small ? 64u : medium ? 32u : 16u;
    return small ? 256u : medium ? 128u : 64u;
}

std::uint32_t emulatedTiledOffset(const Push& push, const Swizzle& swizzle, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t linearOffset) {
    if (swizzle.family == 0) return linearOffset;
    const auto swizzleX = push.tail != 0 ? x + push.tailX : x;
    const auto swizzleY = push.tail != 0 ? y + push.tailY : y;
    if (swizzle.family == 3) {
        const auto blockWidth = 1u << std::popcount(push.swizzleX);
        const auto blockHeight = 1u << std::popcount(push.swizzleY);
        const auto blockDepth = 1u << std::popcount(push.swizzleZ);
        const auto blockIndex = push.tail != 0 ? 0u : (y / blockHeight) * push.blocksPerRow + x / blockWidth;
        return (z / blockDepth) * push.sliceStride + blockIndex * swizzle.blockBytes + (deposit(swizzleX, push.swizzleX) | deposit(swizzleY, push.swizzleY) | deposit(z, push.swizzleZ));
    }
    const auto blockWidth = emulatedBlockWidth(swizzle);
    const auto blockHeight = swizzle.blockBytes / (blockWidth * swizzle.elementBytes);
    const auto blockIndex = push.tail != 0 ? 0u : (y / blockHeight) * push.blocksPerRow + x / blockWidth;
    auto offset = 0u;
    if (swizzle.family == 2) {
        offset = renderTargetOffset(swizzle.elementBytes, swizzleX, swizzleY, push.arrayLayer);
    } else {
        offset = standardOffset(swizzle.elementBytes, swizzleX, swizzleY);
        if (swizzle.blockBytes > 4096u) offset ^= standard64Extra(swizzle.elementBytes, swizzleX, swizzleY);
        offset &= swizzle.blockBytes - 1u;
    }
    return blockIndex * swizzle.blockBytes + offset;
}

void emulateDispatch(const TextureDetilerTestAccess& access, const Swizzle& swizzle) {
    const auto capture = access.lastDispatch();
    const auto push = decodePush(capture.pushConstants);
    const auto elementBytes = swizzle.elementBytes;
    Require(push.elementBytes == elementBytes, "texture tiling dispatch pushed the wrong element size");
    Require(capture.groupsX * 8u >= push.width && capture.groupsY * 8u >= push.height && capture.groupsZ == push.depth, "texture tiling must dispatch every element of every slice");
    const auto& source = access.bytes(capture.sourceBuffer);
    auto& destination = access.bytes(capture.destinationBuffer);
    const auto wordAlignment = std::min(elementBytes, 4u);
    for (std::uint32_t z = 0; z < push.depth; ++z) {
        for (std::uint32_t y = 0; y < push.height; ++y) {
            for (std::uint32_t x = 0; x < push.width; ++x) {
                const auto linearOffset = (z * push.height + y) * push.pitchBytes + x * elementBytes;
                const auto tiledOffset = emulatedTiledOffset(push, swizzle, x, y, z, linearOffset);
                const auto read = push.srcBase + (push.encode != 0 ? linearOffset : tiledOffset);
                const auto write = push.dstBase + (push.encode != 0 ? tiledOffset : linearOffset);
                Require(read % wordAlignment == 0 && write % wordAlignment == 0, "texture tiling must never split an element across shader words");
                Require(read + elementBytes <= capture.sourceRange && write + elementBytes <= capture.destinationRange, "texture tiling must stay inside the bound buffer ranges");
                Require(capture.sourceOffset + read + elementBytes <= source.size() && capture.destinationOffset + write + elementBytes <= destination.size(), "texture tiling must stay inside the bound buffers");
                std::memcpy(destination.data() + capture.destinationOffset + write, source.data() + capture.sourceOffset + read, elementBytes);
            }
        }
    }
}

struct AddrlibVolume {
    TextureTileMode tileMode;
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
    std::uint32_t mipCount;
    std::uint64_t addressHash;
};

constexpr AddrlibVolume kAddrlibVolumes[] = {
    {TextureTileMode::kStandard64KB, 56, 32, 32, 32, 1, 0xf41892112e3a0f25ull},
    {TextureTileMode::kStandard64KB, 64, 4, 4, 64, 1, 0xf7d2366c6e23b525ull},
    {TextureTileMode::kStandard64KB, 64, 4, 4, 64, 3, 0xfcd832687dbb3ba5ull},
    {TextureTileMode::kStandard64KB, 77, 16, 16, 16, 5, 0xfc5d16cc7b533bb7ull},
    {TextureTileMode::kStandard64KB, 56, 64, 64, 64, 7, 0x85ab071f8ce38cb2ull},
    {TextureTileMode::kStandard4KB, 56, 32, 32, 32, 6, 0x77ebe81596169beaull},
    {TextureTileMode::kStandard4KB, 64, 20, 12, 9, 4, 0x0e237bcdd8d54cedull},
};

GuestTextureResource volumeDescriptor(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t mipCount) {
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

std::uint64_t hashAddress(std::uint64_t hash, std::uint64_t address) {
    for (std::uint32_t byte = 0; byte < 8; ++byte) {
        hash ^= (address >> (byte * 8u)) & 0xffu;
        hash *= 1099511628211ull;
    }
    return hash;
}

void volumeDetileTests(const Context& context, const TextureDetilerTestAccess& access) {
    auto volumeContext = context;
    volumeContext.limits.maxStorageBufferRange = 1u << 24;
    TextureDetiler detiler(volumeContext);
    const auto commands = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1));
    for (const auto& reference : kAddrlibVolumes) {
        const auto mips = ComputeMipLayout(volumeDescriptor(reference.tileMode, reference.format, reference.width, reference.height, reference.depth, reference.mipCount));
        const auto elementBytes = BytesPerElement(reference.format);
        const auto guestBytes = ComputeSurfaceSize(mips, reference.depth);
        std::uint64_t linearBytes = 0;
        for (const auto& mip : mips) linearBytes = std::max(linearBytes, mip.linearOffset + mip.linearSize);
        const auto source = access.makeBuffer(guestBytes);
        const auto destination = access.makeBuffer(linearBytes);
        auto& tiled = access.bytes(source);
        for (std::uint64_t address = 0; address < guestBytes; address += elementBytes) {
            const auto value = static_cast<std::uint32_t>(address);
            std::memcpy(tiled.data() + address, &value, sizeof(value));
        }
        for (const auto& mip : mips) {
            detiler.Dispatch(commands, reference.tileMode, elementBytes, source, mip.tiledOffset, destination, mip.linearOffset, mip, 0);
            emulateDispatch(access, expectedSwizzle(reference.tileMode, elementBytes, true));
        }
        const auto& linear = access.bytes(destination);
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto& mip : mips) {
            for (std::uint32_t z = 0; z < mip.depth; ++z) {
                for (std::uint32_t y = 0; y < mip.height; ++y) {
                    for (std::uint32_t x = 0; x < mip.width; ++x) {
                        std::uint32_t address = 0;
                        std::memcpy(&address, linear.data() + mip.linearOffset + (static_cast<std::uint64_t>(z) * mip.height + y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * elementBytes, sizeof(address));
                        hash = hashAddress(hash, address);
                    }
                }
            }
        }
        Require(hash == reference.addressHash, "a synthetic volume tiled like addrlib must detile back into linear element order");
    }

    const auto lutMips = ComputeMipLayout(volumeDescriptor(TextureTileMode::kStandard64KB, 56, 32, 32, 32, 1));
    const auto source = access.makeBuffer(ComputeSurfaceSize(lutMips, 32));
    const auto destination = access.makeBuffer(lutMips[0].linearSize);
    TextureDetiler fresh(volumeContext);
    const auto pipelines = access.pipelineCount();
    fresh.Dispatch(commands, TextureTileMode::kStandard64KB, 4, source, 0, destination, 0, lutMips[0], 0);
    Require(access.pipelineCount() == pipelines + 1, "thick detiling must create its own pipeline");
    const auto specialization = access.lastSpecialization();
    Require(specialization[0] == 4 && specialization[1] == 65536 && specialization[2] == 3, "thick detiling must select the thick swizzle family");
    const auto capture = access.lastDispatch();
    const auto push = decodePush(capture.pushConstants);
    Require(capture.groupsX == 4 && capture.groupsY == 4 && capture.groupsZ == 32, "the 32x32x32 LUT must be detiled by one dispatch over x, y and z");
    Require(push.depth == 32 && push.sliceStride == 65536 && push.swizzleX == 0x9244 && push.swizzleY == 0x4928 && push.swizzleZ == 0x2490, "the 32x32x32 LUT must push the addrlib SW_64K_S3 swizzle for 4-byte elements");
    Require(capture.sourceRange == 131072 && capture.destinationRange == 131072, "the 32x32x32 LUT must read 128 KB of guest memory");
    Require(push.encode == 0, "thick detiling must select the detile direction");
    fresh.Encode(commands, TextureTileMode::kStandard64KB, 4, destination, 0, source, 0, lutMips[0], 0);
    const auto encodeCapture = access.lastDispatch();
    const auto encodePush = decodePush(encodeCapture.pushConstants);
    Require(access.pipelineCount() == pipelines + 1, "thick encoding must reuse the thick detiling pipeline");
    Require(encodeCapture.sourceBuffer == destination && encodeCapture.destinationBuffer == source && encodeCapture.sourceRange == 131072 && encodeCapture.destinationRange == 131072, "the 32x32x32 LUT must be encoded from its linear slices into 128 KB of guest memory");
    Require(encodeCapture.groupsX == 4 && encodeCapture.groupsY == 4 && encodeCapture.groupsZ == 32 && encodePush.encode == 1, "the 32x32x32 LUT must be encoded by one dispatch over x, y and z");
    Require(encodePush.depth == 32 && encodePush.sliceStride == 65536 && encodePush.swizzleX == 0x9244 && encodePush.swizzleY == 0x4928 && encodePush.swizzleZ == 0x2490, "thick encoding must push the same swizzle as thick detiling");
    fresh.Dispatch(commands, TextureTileMode::kStandard64KB, 4, source, 0, destination, 0, makeLayout(32, 32, 128, 1, 65536, 4096), 0);
    Require(access.pipelineCount() == pipelines + 2 && access.lastSpecialization()[2] == 1, "2D standard detiling must keep its own pipeline next to the thick one");
    Require(decodePush(access.lastDispatch().pushConstants).depth == 1 && access.lastDispatch().groupsZ == 1, "2D detiling must stay a single slice dispatch");
    auto slices = makeLayout(32, 32, 128, 1, 65536, 8192);
    slices.depth = 2;
    reject([&] { fresh.Dispatch(commands, TextureTileMode::kStandard64KB, 4, source, 0, destination, 0, slices, 0); }, "several slices only for thick volume layouts");
    reject([&] { fresh.Encode(commands, TextureTileMode::kStandard64KB, 4, destination, 0, source, 0, slices, 0); }, "several slices only for thick volume layouts");
    const auto createdPipelines = access.pipelineCount();
    reject([&] { fresh.Dispatch(commands, TextureTileMode::RenderTarget64KB, 4, source, 0, destination, 0, lutMips[0], 0); }, "thick volume tiling applies only");
    reject([&] { fresh.Encode(commands, TextureTileMode::RenderTarget64KB, 4, destination, 0, source, 0, lutMips[0], 0); }, "thick volume tiling applies only");
    Require(access.pipelineCount() == createdPipelines, "a rejected thick layout must not create a pipeline");
}

void volumeEncodeTests(const Context& context, const TextureDetilerTestAccess& access) {
    auto volumeContext = context;
    volumeContext.limits.maxStorageBufferRange = 1u << 24;
    TextureDetiler detiler(volumeContext);
    const auto commands = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1));
    for (const auto& reference : kAddrlibVolumes) {
        const auto mips = ComputeMipLayout(volumeDescriptor(reference.tileMode, reference.format, reference.width, reference.height, reference.depth, reference.mipCount));
        const auto elementBytes = BytesPerElement(reference.format);
        const auto guestBytes = ComputeSurfaceSize(mips, reference.depth);
        std::uint64_t linearBytes = 0;
        for (const auto& mip : mips) linearBytes = std::max(linearBytes, mip.linearOffset + mip.linearSize);
        const auto source = access.makeBuffer(linearBytes);
        const auto destination = access.makeBuffer(guestBytes);
        auto& linear = access.bytes(source);
        std::uint32_t elements = 0;
        for (const auto& mip : mips) {
            for (std::uint32_t z = 0; z < mip.depth; ++z) {
                for (std::uint32_t y = 0; y < mip.height; ++y) {
                    for (std::uint32_t x = 0; x < mip.width; ++x, ++elements) {
                        const auto offset = mip.linearOffset + (static_cast<std::uint64_t>(z) * mip.height + y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * elementBytes;
                        for (std::uint32_t word = 0; word < elementBytes; word += 4u) std::memcpy(linear.data() + offset + word, &elements, sizeof(elements));
                    }
                }
            }
        }
        auto& tiled = access.bytes(destination);
        std::fill(tiled.begin(), tiled.end(), std::byte{0xff});
        for (const auto& mip : mips) {
            detiler.Encode(commands, reference.tileMode, elementBytes, source, mip.linearOffset, destination, mip.tiledOffset, mip, 0);
            emulateDispatch(access, expectedSwizzle(reference.tileMode, elementBytes, true));
        }
        std::vector<std::uint64_t> addresses(elements, UINT64_MAX);
        for (std::uint64_t address = 0; address < guestBytes; address += elementBytes) {
            std::uint32_t value = 0;
            std::memcpy(&value, tiled.data() + address, sizeof(value));
            if (value == UINT32_MAX) continue;
            Require(value < elements && addresses[value] == UINT64_MAX, "every element of an encoded volume must land at exactly one guest address");
            for (std::uint32_t word = 4; word < elementBytes; word += 4u) {
                std::uint32_t tailWord = 0;
                std::memcpy(&tailWord, tiled.data() + address + word, sizeof(tailWord));
                Require(tailWord == value, "an encoded volume element must stay contiguous");
            }
            addresses[value] = address;
        }
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto address : addresses) {
            Require(address != UINT64_MAX, "every element of a volume must be encoded");
            hash = hashAddress(hash, address);
        }
        Require(hash == reference.addressHash, "a volume encoded from linear element order must land at the addrlib element addresses");
    }
}

constexpr std::string_view kAddrlibStandard64KBPatterns[5] = {
    "X0 X1 X2 X3 Y0 Y1 Y2 Y3 Y4 X4 Y5 X5 Y6 X6 Y7 X7",
    "0 X0 X1 X2 Y0 Y1 Y2 X3 Y3 X4 Y4 X5 Y5 X6 Y6 X7",
    "0 0 X0 X1 Y0 Y1 Y2 X2 Y3 X3 Y4 X4 Y5 X5 Y6 X6",
    "0 0 0 X0 Y0 Y1 X1 X2 Y2 X3 Y3 X4 Y4 X5 Y5 X6",
    "0 0 0 0 Y0 Y1 X0 X1 Y2 X2 Y3 X3 Y4 X4 Y5 X5",
};

constexpr std::string_view kAddrlibRenderTarget64KBSixteenPipePatterns[5] = {
    "X0 X1 X2 Y1 Y0 Y2 X3 Y4 X3^Y3^S3 X4^Y4^S2 X6^Y5^S1 X5^Y6^S0 Y6 X6 Y7 X7",
    "0 X0 X1 X2 Y0 Y1 Y2 X3 X3^Y3^S3 X4^Y4^S2 X6^Y5^S1 X5^Y6^S0 Y4 X6 Y6 X7",
    "0 0 X0 X1 Y0 Y1 Y2 X2 X3^Y3^S3 X4^Y4^S2 X6^Y5^S1 X5^Y6^S0 Y3 X4 Y6 X6",
    "0 0 0 X0 Y0 X1 X2 Y1 X3^Y3^S3 X4^Y4^S2 X6^Y5^S1 X5^Y6^S0 Y2 X3 Y4 X6",
    "0 0 0 0 X0 Y0 X1 Y1 X3^Y3^S3 X4^Y4^S2 X6^Y5^S1 X5^Y6^S0 Y2 X2 Y3 X4",
};

constexpr std::string_view kAddrlibThickStandardPatterns[5] = {
    "X0 X1 Z0 Y0 Z1 Y1 X2 Z2 Y2 X3 Z3 Y3 X4 Z4 Y4 X5",
    "0 X0 Z0 Y0 Z1 Y1 X1 Z2 Y2 X2 Z3 Y3 X3 Z4 Y4 X4",
    "0 0 X0 Y0 Z0 Y1 X1 Z1 Y2 X2 Z2 Y3 X3 Z3 Y4 X4",
    "0 0 0 X0 Z0 Y0 X1 Z1 Y1 X2 Z2 Y2 X3 Z3 Y3 X4",
    "0 0 0 0 Z0 Y0 X0 Z1 Y1 X1 Z2 Y2 X2 Z3 Y3 X3",
};

template<typename TVisitor>
void visitPatternTerms(std::string_view pattern, std::uint32_t bits, TVisitor visitor) {
    std::size_t position = 0;
    for (std::uint32_t bit = 0; bit < bits; ++bit) {
        const auto end = std::min(pattern.find(' ', position), pattern.size());
        auto token = pattern.substr(position, end - position);
        position = end + 1;
        while (token.size() >= 2) {
            visitor(bit, token[0], static_cast<std::uint32_t>(token[1] - '0'));
            token.remove_prefix(std::min<std::size_t>(3, token.size()));
        }
    }
}

std::uint32_t patternOffset(std::string_view pattern, std::uint32_t bits, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    std::uint32_t offset = 0;
    visitPatternTerms(pattern, bits, [&](std::uint32_t bit, char channel, std::uint32_t index) {
        const auto coordinate = channel == 'X' ? x : channel == 'Y' ? y : z;
        offset ^= ((coordinate >> index) & 1u) << bit;
    });
    return offset;
}

std::uint32_t patternExtent(std::string_view pattern, std::uint32_t bits, char channel) {
    std::uint32_t extent = 1;
    visitPatternTerms(pattern, bits, [&](std::uint32_t, char term, std::uint32_t index) {
        if (term == channel) extent = std::max(extent, 2u << index);
    });
    return extent;
}

struct EncodeCase {
    TextureTileMode tileMode;
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
    std::uint32_t mipCount;
    std::uint32_t layers;
};

constexpr EncodeCase kEncodeCases[] = {
    {TextureTileMode::kStandard256B, 1, 37, 21, 1, 2, 2},
    {TextureTileMode::kStandard256B, 7, 19, 11, 1, 3, 1},
    {TextureTileMode::kStandard256B, 77, 13, 7, 1, 1, 3},
    {TextureTileMode::kStandard4KB, 14, 67, 35, 1, 4, 2},
    {TextureTileMode::kStandard4KB, 56, 100, 60, 1, 7, 1},
    {TextureTileMode::kStandard4KB, 64, 41, 29, 1, 6, 1},
    {TextureTileMode::kStandard4KB, 169, 100, 60, 1, 4, 2},
    {TextureTileMode::kStandard64KB, 1, 300, 200, 1, 9, 1},
    {TextureTileMode::kStandard64KB, 7, 129, 67, 1, 3, 2},
    {TextureTileMode::kStandard64KB, 56, 257, 130, 1, 3, 2},
    {TextureTileMode::kStandard64KB, 71, 97, 33, 1, 2, 1},
    {TextureTileMode::kStandard64KB, 77, 70, 45, 1, 7, 1},
    {TextureTileMode::kStandard64KB, 181, 256, 128, 1, 5, 1},
    {TextureTileMode::RenderTarget64KB, 1, 130, 129, 1, 1, 3},
    {TextureTileMode::RenderTarget64KB, 14, 97, 66, 1, 2, 2},
    {TextureTileMode::RenderTarget64KB, 56, 130, 129, 1, 1, 1},
    {TextureTileMode::RenderTarget64KB, 56, 257, 17, 1, 1, 1},
    {TextureTileMode::RenderTarget64KB, 56, 17, 9, 1, 1, 10},
    {TextureTileMode::RenderTarget64KB, 56, 300, 150, 1, 9, 1},
    {TextureTileMode::RenderTarget64KB, 64, 65, 33, 1, 3, 2},
    {TextureTileMode::RenderTarget64KB, 77, 64, 65, 1, 1, 5},
    {TextureTileMode::kLinear, 1, 37, 19, 1, 3, 2},
    {TextureTileMode::kLinear, 7, 5, 3, 1, 1, 1},
    {TextureTileMode::kLinear, 56, 70, 9, 1, 1, 1},
    {TextureTileMode::kLinear, 64, 33, 17, 1, 2, 1},
    {TextureTileMode::kLinear, 77, 9, 5, 1, 1, 3},
    {TextureTileMode::kLinear, 169, 8, 8, 1, 1, 1},
    {TextureTileMode::Depth64KB, 1, 70, 33, 1, 1, 1},
    {TextureTileMode::Depth64KB, 14, 33, 70, 1, 2, 1},
    {TextureTileMode::Depth64KB, 22, 130, 129, 1, 1, 2},
    {TextureTileMode::Depth64KB, 64, 65, 31, 1, 1, 1},
    {TextureTileMode::Depth64KB, 77, 40, 40, 1, 3, 1},
    {TextureTileMode::kStandard4KB, 1, 16, 16, 16, 5, 1},
    {TextureTileMode::kStandard4KB, 64, 20, 12, 9, 4, 1},
    {TextureTileMode::kStandard4KB, 14, 64, 16, 8, 7, 1},
    {TextureTileMode::kStandard64KB, 1, 64, 32, 32, 3, 1},
    {TextureTileMode::kStandard64KB, 14, 48, 40, 20, 6, 1},
    {TextureTileMode::kStandard64KB, 56, 32, 32, 32, 1, 1},
    {TextureTileMode::kStandard64KB, 77, 16, 16, 16, 5, 1},
};

struct EncodeSurface {
    std::vector<TileMipLayout> mips;
    std::uint32_t elementBytes;
    std::uint32_t tiledLayers;
    std::uint64_t guestSliceBytes;
    std::uint64_t linearSliceBytes;
    std::uint64_t elements;
    bool thick;
};

EncodeSurface makeSurface(const EncodeCase& test) {
    EncodeSurface surface{};
    surface.thick = test.depth > 1u;
    surface.mips = surface.thick ? ComputeMipLayout(volumeDescriptor(test.tileMode, test.format, test.width, test.height, test.depth, test.mipCount)) : ComputeMipLayout(test.tileMode, test.format, test.width, test.height, test.mipCount);
    Require(surface.thick == (surface.mips.front().blockDepth > 1u), "encode test volumes must use thick tiling");
    surface.elementBytes = BytesPerElement(test.format);
    surface.tiledLayers = surface.thick ? 1u : test.layers;
    surface.guestSliceBytes = ComputeSurfaceSize(surface.mips, surface.thick ? test.depth : test.layers) / surface.tiledLayers;
    for (const auto& mip : surface.mips) {
        surface.linearSliceBytes = std::max(surface.linearSliceBytes, mip.linearOffset + mip.linearSize);
        surface.elements += static_cast<std::uint64_t>(mip.width) * mip.height * mip.depth * surface.tiledLayers;
    }
    return surface;
}

constexpr std::uint64_t kLinearBase = 0x44;
constexpr std::uint64_t kGuestBase = 0x104;
constexpr std::uint64_t kSlack = 0x40;

std::uint64_t linearElementOffset(const EncodeSurface& surface, const TileMipLayout& mip, std::uint32_t layer, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return kLinearBase + layer * surface.linearSliceBytes + mip.linearOffset + (static_cast<std::uint64_t>(z) * mip.height + y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * surface.elementBytes;
}

struct ReferenceTiling {
    TextureTileMode tileMode;
    std::string_view pattern;
    std::uint32_t bits;
    std::uint32_t blockWidth;
    std::uint32_t blockHeight;
    std::uint32_t blockDepth;
};

bool makeReferenceTiling(TextureTileMode tileMode, const EncodeSurface& surface, ReferenceTiling& reference) {
    const auto index = static_cast<std::size_t>(std::countr_zero(surface.elementBytes));
    reference = {tileMode, {}, 16u, 1u, 1u, 1u};
    switch (tileMode) {
        case TextureTileMode::kLinear: return true;
        case TextureTileMode::kStandard256B: reference.bits = 8u; break;
        case TextureTileMode::kStandard4KB: reference.bits = 12u; break;
        case TextureTileMode::kStandard64KB:
        case TextureTileMode::RenderTarget64KB: break;
        case TextureTileMode::Depth64KB: return false;
    }
    reference.pattern = surface.thick ? kAddrlibThickStandardPatterns[index] : tileMode == TextureTileMode::RenderTarget64KB ? kAddrlibRenderTarget64KBSixteenPipePatterns[index] : kAddrlibStandard64KBPatterns[index];
    if (surface.thick) {
        reference.blockWidth = patternExtent(reference.pattern, reference.bits, 'X');
        reference.blockHeight = patternExtent(reference.pattern, reference.bits, 'Y');
        reference.blockDepth = patternExtent(reference.pattern, reference.bits, 'Z');
    } else {
        const auto elementBits = reference.bits - static_cast<std::uint32_t>(index);
        reference.blockWidth = 1u << ((elementBits + 1u) / 2u);
        reference.blockHeight = 1u << (elementBits / 2u);
    }
    Require(reference.blockWidth * reference.blockHeight * reference.blockDepth * surface.elementBytes == (1u << reference.bits), "addrlib reference blocks must cover the whole block size");
    return true;
}

std::uint64_t referenceAddress(const ReferenceTiling& reference, const EncodeSurface& surface, const TileMipLayout& mip, std::uint32_t layer, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    const auto layerBase = kGuestBase + layer * surface.guestSliceBytes;
    if (reference.tileMode == TextureTileMode::kLinear) return layerBase + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * surface.elementBytes;
    const auto blockIndex = mip.tail ? 0u : (y / reference.blockHeight) * mip.blocksPerRow + x / reference.blockWidth;
    const auto swizzleX = mip.tail ? x + mip.tailX : x;
    const auto swizzleY = mip.tail ? y + mip.tailY : y;
    const auto slab = static_cast<std::uint64_t>(z / reference.blockDepth) * mip.sliceStride;
    return layerBase + mip.tiledOffset + slab + (static_cast<std::uint64_t>(blockIndex) << reference.bits) + patternOffset(reference.pattern, reference.bits, swizzleX, swizzleY, surface.thick ? z : layer);
}

std::byte guestPattern(std::uint64_t index) {
    return static_cast<std::byte>((index * 29u + (index >> 9u)) ^ 0xa5u);
}

void fillRandom(std::vector<std::byte>& bytes, std::uint32_t& state) {
    for (auto& byte : bytes) {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        byte = static_cast<std::byte>(state >> 24u);
    }
}

template<typename TVisitor>
void visitElements(const EncodeSurface& surface, TVisitor visitor) {
    for (std::uint32_t layer = 0; layer < surface.tiledLayers; ++layer) {
        for (const auto& mip : surface.mips) {
            for (std::uint32_t z = 0; z < mip.depth; ++z) {
                for (std::uint32_t y = 0; y < mip.height; ++y) {
                    for (std::uint32_t x = 0; x < mip.width; ++x) visitor(mip, layer, x, y, z);
                }
            }
        }
    }
}

void encodeSurface(TextureDetiler& detiler, const TextureDetilerTestAccess& access, const EncodeCase& test, const EncodeSurface& surface, VkBuffer linear, VkBuffer tiled) {
    const auto commands = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1));
    const auto swizzle = expectedSwizzle(test.tileMode, surface.elementBytes, surface.thick);
    for (std::uint32_t layer = 0; layer < surface.tiledLayers; ++layer) {
        for (const auto& mip : surface.mips) {
            detiler.Encode(commands, test.tileMode, surface.elementBytes, linear, kLinearBase + layer * surface.linearSliceBytes + mip.linearOffset, tiled, kGuestBase + layer * surface.guestSliceBytes + mip.tiledOffset, mip, layer);
            const auto capture = access.lastDispatch();
            const auto push = decodePush(capture.pushConstants);
            Require(push.encode == 1 && push.arrayLayer == layer, "texture encoding must push the encode direction and the absolute array layer");
            Require(capture.sourceBuffer == linear && capture.destinationBuffer == tiled, "texture encoding must read the linear buffer and write the tiled buffer");
            emulateDispatch(access, swizzle);
        }
    }
}

void detileSurface(TextureDetiler& detiler, const TextureDetilerTestAccess& access, const EncodeCase& test, const EncodeSurface& surface, VkBuffer tiled, VkBuffer linear) {
    const auto commands = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1));
    const auto swizzle = expectedSwizzle(test.tileMode, surface.elementBytes, surface.thick);
    for (std::uint32_t layer = 0; layer < surface.tiledLayers; ++layer) {
        for (const auto& mip : surface.mips) {
            detiler.Dispatch(commands, test.tileMode, surface.elementBytes, tiled, kGuestBase + layer * surface.guestSliceBytes + mip.tiledOffset, linear, kLinearBase + layer * surface.linearSliceBytes + mip.linearOffset, mip, layer);
            Require(decodePush(access.lastDispatch().pushConstants).encode == 0, "texture detiling must push the detile direction");
            emulateDispatch(access, swizzle);
        }
    }
}

void checkColorTargetLayout(const EncodeCase& test, const EncodeSurface& surface, const std::vector<std::byte>& linear, const std::vector<std::byte>& guest, const std::vector<std::byte>& encoded) {
    const auto& mip = surface.mips.front();
    const ColorTargetLayout layout(mip.width, mip.height, test.tileMode == TextureTileMode::kLinear ? ColorTileMode::Linear : ColorTileMode::RenderTarget, surface.elementBytes);
    Require(layout.Bytes() == mip.tiledSize && layout.LinearBytes() == static_cast<std::size_t>(mip.width) * mip.height * surface.elementBytes, "ColorTargetLayout must describe the same single-mip surface");
    std::vector<std::byte> packed(layout.LinearBytes());
    for (std::uint32_t y = 0; y < mip.height; ++y) std::memcpy(packed.data() + static_cast<std::size_t>(y) * mip.width * surface.elementBytes, linear.data() + linearElementOffset(surface, mip, 0, 0, y, 0), static_cast<std::size_t>(mip.width) * surface.elementBytes);
    auto expected = guest;
    layout.Tile(packed, std::span<std::byte>(expected.data() + kGuestBase, layout.Bytes()));
    Require(encoded == expected, "texture encoding must match the ColorTargetLayout CPU tiler byte for byte, surface padding included");
}

void checkEncodeCase(const Context& context, const TextureDetilerTestAccess& access, const EncodeCase& test, std::uint32_t& seed) {
    const auto surface = makeSurface(test);
    const auto elementBytes = surface.elementBytes;
    const auto linearBytes = kLinearBase + surface.linearSliceBytes * surface.tiledLayers + kSlack;
    const auto guestBytes = kGuestBase + surface.guestSliceBytes * surface.tiledLayers + kSlack;
    TextureDetiler detiler(context);
    const auto linear = access.makeBuffer(linearBytes);
    fillRandom(access.bytes(linear), seed);
    const auto source = access.bytes(linear);
    std::vector<std::byte> guest(guestBytes);
    std::vector<std::byte> complementedGuest(guestBytes);
    for (std::uint64_t index = 0; index < guestBytes; ++index) {
        guest[index] = guestPattern(index);
        complementedGuest[index] = ~guest[index];
    }
    const auto first = access.makeBuffer(guestBytes);
    const auto second = access.makeBuffer(guestBytes);
    access.bytes(first) = guest;
    access.bytes(second) = complementedGuest;

    const auto pipelines = access.pipelineCount();
    encodeSurface(detiler, access, test, surface, linear, first);
    const auto expected = expectedSwizzle(test.tileMode, elementBytes, surface.thick);
    const auto specialization = access.lastSpecialization();
    Require(access.pipelineCount() == pipelines + 1 && specialization[0] == expected.elementBytes && specialization[1] == expected.blockBytes && specialization[2] == expected.family, "texture encoding must use the detiling pipeline of the surface's tile family");
    encodeSurface(detiler, access, test, surface, linear, second);
    Require(access.bytes(linear) == source, "texture encoding must not write its linear source");

    const auto& encoded = access.bytes(first);
    const auto& complement = access.bytes(second);
    std::uint64_t written = 0;
    for (std::uint64_t index = 0; index < guestBytes; ++index) {
        if (encoded[index] == complement[index]) ++written;
    }
    Require(written == surface.elements * elementBytes, "texture encoding must write every element byte exactly once and no other guest byte");

    ReferenceTiling tiling{};
    if (makeReferenceTiling(test.tileMode, surface, tiling)) {
        auto reference = guest;
        visitElements(surface, [&](const TileMipLayout& mip, std::uint32_t layer, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
            const auto address = referenceAddress(tiling, surface, mip, layer, x, y, z);
            Require(address + elementBytes <= guestBytes - kSlack, "an addrlib reference address fell outside the surface");
            std::memcpy(reference.data() + address, source.data() + linearElementOffset(surface, mip, layer, x, y, z), elementBytes);
        });
        Require(encoded == reference, "texture encoding must match the addrlib reference byte for byte and leave every other guest byte untouched");
    }
    const auto singleSurface = surface.mips.size() == 1 && surface.tiledLayers == 1 && !surface.thick;
    if (singleSurface && (test.tileMode == TextureTileMode::kLinear || (test.tileMode == TextureTileMode::RenderTarget64KB && elementBytes == 4))) checkColorTargetLayout(test, surface, source, guest, encoded);

    std::vector<std::byte> unwritten(linearBytes);
    for (std::uint64_t index = 0; index < linearBytes; ++index) unwritten[index] = guestPattern(index ^ 0x5555u);
    const auto restored = access.makeBuffer(linearBytes);
    access.bytes(restored) = unwritten;
    detileSurface(detiler, access, test, surface, first, restored);
    const auto& roundTrip = access.bytes(restored);
    std::vector<bool> elementByte(linearBytes);
    visitElements(surface, [&](const TileMipLayout& mip, std::uint32_t layer, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        const auto offset = linearElementOffset(surface, mip, layer, x, y, z);
        Require(std::memcmp(roundTrip.data() + offset, source.data() + offset, elementBytes) == 0, "detiling an encoded surface must restore every element");
        for (std::uint32_t byte = 0; byte < elementBytes; ++byte) elementByte[offset + byte] = true;
    });
    for (std::uint64_t index = 0; index < linearBytes; ++index) {
        Require(elementByte[index] || roundTrip[index] == unwritten[index], "detiling an encoded surface must not write outside its elements");
    }
}

PFN_vkGetDeviceProcAddr forwardedDeviceProc = nullptr;
std::vector<std::string> resolvedCommands;

PFN_vkVoidFunction VKAPI_CALL recordingDeviceProc(VkDevice device, const char* name) {
    if (std::string_view(name).starts_with("vkCmd")) resolvedCommands.emplace_back(name);
    return forwardedDeviceProc(device, name);
}

void encodeCommandTests(const Context& context, const TextureDetilerTestAccess& access) {
    auto recording = context;
    recording.device = reinterpret_cast<VkDevice>(static_cast<std::uintptr_t>(0xe7c0de));
    recording.deviceProc = recordingDeviceProc;
    forwardedDeviceProc = context.deviceProc;
    resolvedCommands.clear();
    const auto commands = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1));
    const auto linear = access.makeBuffer(4096);
    const auto tiled = access.makeBuffer(4096);
    {
        TextureDetiler detiler(recording);
        detiler.Encode(commands, TextureTileMode::kStandard4KB, 4, linear, 0, tiled, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
        detiler.Encode(commands, TextureTileMode::RenderTarget64KB, 2, linear, 64, tiled, 128, makeLayout(8, 8, 16, 1, 64, 32), 3);
        detiler.Encode(commands, TextureTileMode::kLinear, 1, linear, 3, tiled, 5, makeLayout(8, 8, 32, 1, 32, 32), 0);
    }
    std::sort(resolvedCommands.begin(), resolvedCommands.end());
    resolvedCommands.erase(std::unique(resolvedCommands.begin(), resolvedCommands.end()), resolvedCommands.end());
    const std::vector<std::string> dispatchOnly{"vkCmdBindDescriptorSets", "vkCmdBindPipeline", "vkCmdDispatch", "vkCmdPushConstants"};
    Require(resolvedCommands == dispatchOnly, "texture encoding must record only its dispatches and leave every barrier to the caller");
}

void encodeTests(const Context& context, const TextureDetilerTestAccess& access) {
    encodeCommandTests(context, access);
    auto encodeContext = context;
    encodeContext.limits.minStorageBufferOffsetAlignment = 64;
    encodeContext.limits.maxStorageBufferRange = 1u << 24;
    std::uint32_t seed = 0x9e3779b9u;
    for (const auto& test : kEncodeCases) checkEncodeCase(encodeContext, access, test, seed);
    volumeEncodeTests(context, access);
}

}

void RunTextureDetilerTests(const Context& context, const TextureDetilerTestAccess& access) {
    TextureDetiler detiler(context);
    const auto commands = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1));
    const auto source = access.makeBuffer(4096);
    const auto destination = access.makeBuffer(4096);

    const auto layout = makeLayout(20, 12, 96, 5, 64, 48);
    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 20, destination, 40, layout, 0);
    auto capture = access.lastDispatch();
    Require(capture.groupsX == 3 && capture.groupsY == 2 && capture.groupsZ == 1, "dispatch group counts were computed incorrectly");
    const auto push = decodePush(capture.pushConstants);
    Require(push.srcBase == 4 && push.dstBase == 8, "push constant buffer bases must account for storage buffer offset alignment");
    Require(push.width == 20 && push.height == 12, "push constant dimensions changed");
    Require(push.pitchBytes == 96 && push.blocksPerRow == 5, "push constant row layout changed");
    Require(push.tail == 0 && push.tailX == 0 && push.tailY == 0, "push constant tail fields must reflect a non-tail mip");
    Require(push.elementBytes == 4, "push constant element size changed");
    Require(push.encode == 0, "texture detiling must select the detile direction");
    Require(capture.sourceBuffer == source && capture.destinationBuffer == destination, "dispatch bound the wrong source or destination buffer");
    Require(capture.sourceOffset == 16 && capture.sourceRange == 68, "source descriptor offset or range computed incorrectly");
    Require(capture.destinationOffset == 32 && capture.destinationRange == 56, "destination descriptor offset or range computed incorrectly");

    const auto pipelinesAfterFirst = access.pipelineCount();
    Require(pipelinesAfterFirst == 1, "the first dispatch must create exactly one compute pipeline");

    detiler.Encode(commands, TextureTileMode::kStandard4KB, 4, destination, 20, source, 40, layout, 0);
    capture = access.lastDispatch();
    const auto encodePush = decodePush(capture.pushConstants);
    Require(access.pipelineCount() == pipelinesAfterFirst, "texture encoding must reuse the detiling pipeline of the same tile mode and element size");
    Require(encodePush.encode == 1, "texture encoding must select the encode direction");
    Require(encodePush.srcBase == 4 && encodePush.dstBase == 8, "texture encoding bases must account for storage buffer offset alignment");
    Require(encodePush.width == push.width && encodePush.height == push.height && encodePush.pitchBytes == push.pitchBytes && encodePush.blocksPerRow == push.blocksPerRow && encodePush.elementBytes == push.elementBytes && encodePush.depth == push.depth, "texture encoding must push the same mip geometry as detiling");
    Require(capture.groupsX == 3 && capture.groupsY == 2 && capture.groupsZ == 1, "texture encoding must dispatch the same groups as detiling");
    Require(capture.sourceBuffer == destination && capture.destinationBuffer == source, "texture encoding bound the wrong linear or tiled buffer");
    Require(capture.sourceOffset == 16 && capture.sourceRange == 52, "texture encoding must bind the linear mip as its source range");
    Require(capture.destinationOffset == 32 && capture.destinationRange == 72, "texture encoding must bind the tiled mip as its destination range");

    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterFirst, "dispatching with the same tile mode and element size must reuse the cached pipeline");

    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 8, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterFirst + 1, "a different element size must create a new compute pipeline");

    detiler.Dispatch(commands, TextureTileMode::kLinear, 4, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterFirst + 2, "a different tile mode must create a new compute pipeline");

    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterFirst + 2, "reusing an earlier tile mode and element size must not create another pipeline");

    detiler.Dispatch(commands, TextureTileMode::RenderTarget64KB, 4, source, 0, destination, 0, layout, 13);
    capture = access.lastDispatch();
    Require(decodePush(capture.pushConstants).arrayLayer == 13, "render target detiling must preserve the absolute array layer for XOR addressing");
    Require(access.pipelineCount() == pipelinesAfterFirst + 3, "render target detiling must use a separate pipeline");
    const auto specialization = access.lastSpecialization();
    Require(specialization[0] == 4 && specialization[1] == 65536 && specialization[2] == 2, "render target detiling must select its own swizzle family");
    detiler.Encode(commands, TextureTileMode::RenderTarget64KB, 4, destination, 0, source, 0, layout, 13);
    Require(decodePush(access.lastDispatch().pushConstants).arrayLayer == 13 && access.pipelineCount() == pipelinesAfterFirst + 3, "render target encoding must reuse its pipeline and push the absolute array layer");

    reject([&] { detiler.Dispatch(VK_NULL_HANDLE, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, layout, 0); }, "active command buffer");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, VK_NULL_HANDLE, 0, destination, 0, layout, 0); }, "source and destination buffers");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, VK_NULL_HANDLE, 0, layout, 0); }, "source and destination buffers");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(0, 12, 96, 5, 64, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 0, 96, 5, 64, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 12, 96, 5, 0, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 12, 96, 5, 64, 0), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 3, source, 0, destination, 0, layout, 0); }, "unsupported element size");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 20, destination, 40, makeLayout(20, 12, 96, 5, 1024, 48), 0); }, "buffer range exceeds device limits");
    reject([&] { detiler.Encode(VK_NULL_HANDLE, TextureTileMode::kStandard4KB, 4, destination, 0, source, 0, layout, 0); }, "active command buffer");
    reject([&] { detiler.Encode(commands, TextureTileMode::kStandard4KB, 4, VK_NULL_HANDLE, 0, source, 0, layout, 0); }, "source and destination buffers");
    reject([&] { detiler.Encode(commands, TextureTileMode::kStandard4KB, 4, destination, 0, VK_NULL_HANDLE, 0, layout, 0); }, "source and destination buffers");
    reject([&] { detiler.Encode(commands, TextureTileMode::kStandard4KB, 4, destination, 0, source, 0, makeLayout(20, 12, 96, 5, 0, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Encode(commands, TextureTileMode::kStandard4KB, 3, destination, 0, source, 0, layout, 0); }, "unsupported element size");
    reject([&] { detiler.Encode(commands, TextureTileMode::kStandard4KB, 4, destination, 20, source, 40, makeLayout(20, 12, 96, 5, 1024, 48), 0); }, "buffer range exceeds device limits");
    reject([&] { detiler.Encode(commands, TextureTileMode::kStandard4KB, 4, destination, 0, source, 2, layout, 0); }, "aligned to the element size");
    reject([&] { detiler.Encode(commands, TextureTileMode::kStandard4KB, 4, destination, 6, source, 0, layout, 0); }, "aligned to the element size");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 2, destination, 0, layout, 0); }, "aligned to the element size");
    reject([&] { detiler.Encode(commands, TextureTileMode::kStandard4KB, 2, destination, 0, source, 1, layout, 0); }, "aligned to the element size");
    detiler.Encode(commands, TextureTileMode::kStandard4KB, 2, destination, 2, source, 6, layout, 0);
    const auto halfWordPush = decodePush(access.lastDispatch().pushConstants);
    Require(halfWordPush.srcBase == 2 && halfWordPush.dstBase == 6, "texture encoding must accept offsets aligned to a 2-byte element");
    detiler.Encode(commands, TextureTileMode::kStandard4KB, 8, destination, 4, source, 12, layout, 0);
    const auto wordPush = decodePush(access.lastDispatch().pushConstants);
    Require(wordPush.srcBase == 4 && wordPush.dstBase == 12, "texture encoding must accept word-aligned offsets for 8-byte elements");
    volumeDetileTests(context, access);
    encodeTests(context, access);
}

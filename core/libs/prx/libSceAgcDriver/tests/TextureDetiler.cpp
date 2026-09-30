#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <string>
#include <string_view>

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

void emulateThickDispatch(const DetilerCapture& capture, std::uint32_t blockBytes, const std::vector<std::byte>& source, std::vector<std::byte>& destination) {
    const auto push = decodePush(capture.pushConstants);
    Require(capture.groupsX * 8u >= push.width && capture.groupsY * 8u >= push.height && capture.groupsZ == push.depth, "thick detiling must dispatch every element of every slice");
    const auto blockWidth = 1u << std::popcount(push.swizzleX);
    const auto blockHeight = 1u << std::popcount(push.swizzleY);
    const auto blockDepth = 1u << std::popcount(push.swizzleZ);
    for (std::uint32_t z = 0; z < push.depth; ++z) {
        for (std::uint32_t y = 0; y < push.height; ++y) {
            for (std::uint32_t x = 0; x < push.width; ++x) {
                const auto blockIndex = push.tail != 0 ? 0u : (y / blockHeight) * push.blocksPerRow + x / blockWidth;
                const auto swizzleX = push.tail != 0 ? x + push.tailX : x;
                const auto swizzleY = push.tail != 0 ? y + push.tailY : y;
                const auto read = push.srcBase + (z / blockDepth) * push.sliceStride + blockIndex * blockBytes + (deposit(swizzleX, push.swizzleX) | deposit(swizzleY, push.swizzleY) | deposit(z, push.swizzleZ));
                const auto write = push.dstBase + (z * push.height + y) * push.pitchBytes + x * push.elementBytes;
                Require(read + push.elementBytes <= capture.sourceRange && write + push.elementBytes <= capture.destinationRange, "thick detiling must stay inside the bound buffer ranges");
                std::memcpy(destination.data() + capture.destinationOffset + write, source.data() + capture.sourceOffset + read, push.elementBytes);
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

void volumeDetileTests(const Context& context, const TextureDetilerTestAccess& access) {
    auto volumeContext = context;
    volumeContext.limits.maxStorageBufferRange = 1u << 24;
    TextureDetiler detiler(volumeContext);
    const auto commands = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1));
    for (const auto& reference : kAddrlibVolumes) {
        GuestTextureResource descriptor{};
        descriptor.baseAddress = 0x1409960000ull;
        descriptor.width = reference.width;
        descriptor.height = reference.height;
        descriptor.depthOrLastArray = reference.depth - 1u;
        descriptor.mipCount = reference.mipCount;
        descriptor.lastLevel = reference.mipCount - 1u;
        descriptor.tileMode = reference.tileMode;
        descriptor.dimension = TextureDimension::k3D;
        descriptor.viewDimension = TextureDimension::k3D;
        descriptor.format = reference.format;
        const auto mips = ComputeMipLayout(descriptor);
        const auto elementBytes = BytesPerElement(reference.format);
        const auto blockBytes = reference.tileMode == TextureTileMode::kStandard4KB ? 4096u : 65536u;
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
            emulateThickDispatch(access.lastDispatch(), blockBytes, access.bytes(source), access.bytes(destination));
        }
        const auto& linear = access.bytes(destination);
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto& mip : mips) {
            for (std::uint32_t z = 0; z < mip.depth; ++z) {
                for (std::uint32_t y = 0; y < mip.height; ++y) {
                    for (std::uint32_t x = 0; x < mip.width; ++x) {
                        std::uint32_t address = 0;
                        std::memcpy(&address, linear.data() + mip.linearOffset + (static_cast<std::uint64_t>(z) * mip.height + y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * elementBytes, sizeof(address));
                        for (std::uint32_t byte = 0; byte < 8; ++byte) {
                            hash ^= (static_cast<std::uint64_t>(address) >> (byte * 8u)) & 0xffu;
                            hash *= 1099511628211ull;
                        }
                    }
                }
            }
        }
        Require(hash == reference.addressHash, "a synthetic volume tiled like addrlib must detile back into linear element order");
    }

    GuestTextureResource lut{};
    lut.baseAddress = 0x1409960000ull;
    lut.width = 32;
    lut.height = 32;
    lut.depthOrLastArray = 31;
    lut.mipCount = 1;
    lut.tileMode = TextureTileMode::kStandard64KB;
    lut.dimension = TextureDimension::k3D;
    lut.viewDimension = TextureDimension::k3D;
    lut.format = 56;
    const auto lutMips = ComputeMipLayout(lut);
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
    fresh.Dispatch(commands, TextureTileMode::kStandard64KB, 4, source, 0, destination, 0, makeLayout(32, 32, 128, 1, 65536, 4096), 0);
    Require(access.pipelineCount() == pipelines + 2 && access.lastSpecialization()[2] == 1, "2D standard detiling must keep its own pipeline next to the thick one");
    Require(decodePush(access.lastDispatch().pushConstants).depth == 1 && access.lastDispatch().groupsZ == 1, "2D detiling must stay a single slice dispatch");
    auto slices = makeLayout(32, 32, 128, 1, 65536, 8192);
    slices.depth = 2;
    reject([&] { fresh.Dispatch(commands, TextureTileMode::kStandard64KB, 4, source, 0, destination, 0, slices, 0); }, "several slices only for thick volume layouts");
    const auto createdPipelines = access.pipelineCount();
    reject([&] { fresh.Dispatch(commands, TextureTileMode::RenderTarget64KB, 4, source, 0, destination, 0, lutMips[0], 0); }, "thick volume tiling applies only");
    Require(access.pipelineCount() == createdPipelines, "a rejected thick layout must not create a pipeline");
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
    Require(capture.sourceBuffer == source && capture.destinationBuffer == destination, "dispatch bound the wrong source or destination buffer");
    Require(capture.sourceOffset == 16 && capture.sourceRange == 68, "source descriptor offset or range computed incorrectly");
    Require(capture.destinationOffset == 32 && capture.destinationRange == 56, "destination descriptor offset or range computed incorrectly");

    const auto pipelinesAfterFirst = access.pipelineCount();
    Require(pipelinesAfterFirst == 1, "the first dispatch must create exactly one compute pipeline");

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

    reject([&] { detiler.Dispatch(VK_NULL_HANDLE, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, layout, 0); }, "active command buffer");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, VK_NULL_HANDLE, 0, destination, 0, layout, 0); }, "source and destination buffers");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, VK_NULL_HANDLE, 0, layout, 0); }, "source and destination buffers");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(0, 12, 96, 5, 64, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 0, 96, 5, 64, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 12, 96, 5, 0, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 12, 96, 5, 64, 0), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 3, source, 0, destination, 0, layout, 0); }, "unsupported element size");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 20, destination, 40, makeLayout(20, 12, 96, 5, 1024, 48), 0); }, "buffer range exceeds device limits");
    volumeDetileTests(context, access);
}

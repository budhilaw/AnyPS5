#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdlib>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace AgcDriver::Graphics {

}  // namespace AgcDriver::Graphics

namespace AgcDriver::Graphics {

std::uint32_t FullArrayLayers(const GuestTextureResource& descriptor) {
    switch (descriptor.dimension) {
        case TextureDimension::k1D:
        case TextureDimension::k2D: return 1u;
        case TextureDimension::k2DArray:
        case TextureDimension::kCube:
        case TextureDimension::k3D: return descriptor.depthOrLastArray + 1u;
    }
    throw std::runtime_error("AGC graphics: Texture encountered an unknown guest texture dimension");
}

namespace {

VkImageType ImageTypeFor(TextureDimension dimension) {
    return dimension == TextureDimension::k1D ? VK_IMAGE_TYPE_1D : dimension == TextureDimension::k3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
}

VkImageViewType ViewTypeFor(TextureDimension dimension, std::uint32_t viewLayerCount) {
    switch (dimension) {
        case TextureDimension::k1D: return VK_IMAGE_VIEW_TYPE_1D;
        case TextureDimension::k2D: return VK_IMAGE_VIEW_TYPE_2D;
        case TextureDimension::k2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureDimension::kCube: return viewLayerCount == 6u ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        case TextureDimension::k3D: return VK_IMAGE_VIEW_TYPE_3D;
    }
    throw std::runtime_error("AGC graphics: Texture encountered an unknown guest texture dimension");
}

VkFormat LinearFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_SRGB: return VK_FORMAT_R8_UNORM;
        case VK_FORMAT_R8G8_SRGB: return VK_FORMAT_R8G8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
        default: return format;
    }
}

std::uint64_t SliceLinearBytes(const std::vector<TileMipLayout>& mips) {
    std::uint64_t bytes = 0;
    for (const auto& mip : mips) bytes = std::max(bytes, mip.linearOffset + mip.linearSize);
    return bytes;
}

}

Texture::Texture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, VkComponentMapping components, std::span<const std::byte> snapshot) : context(context) {
    PerformanceTimer timing("Graphics.Texture.Detile");
    try {
        const auto vkFormat = ResolveTextureFormat(descriptor.format);
        if (IsBlockCompressed(descriptor.format)) {
            Require(context.textureCompressionBC, "device does not support BC compressed textures");
        }

        const auto mips = ComputeMipLayout(descriptor);
        const auto arrayLayers = FullArrayLayers(descriptor);
        const auto elementBytes = BytesPerElement(descriptor.format);
        const bool volume = descriptor.dimension == TextureDimension::k3D;
        const auto tiledLayers = mips.front().blockDepth > 1u ? 1u : arrayLayers;
        const auto imageLayers = volume ? 1u : arrayLayers;
        this->imageLayers = imageLayers;
        extent = {descriptor.width, descriptor.height};
        guestAddress = descriptor.baseAddress;
        guestTileMode = descriptor.tileMode;
        guestFormat = vkFormat;
        guestTexelBytes = IsBlockCompressed(descriptor.format) ? 0u : elementBytes;
        guest2D = descriptor.dimension == TextureDimension::k2D;
        guestMipCount = descriptor.mipCount;
        guestDimension = static_cast<std::uint32_t>(descriptor.dimension);
        guestLayers = arrayLayers;
        promotable = (descriptor.dimension == TextureDimension::k2D || descriptor.dimension == TextureDimension::k2DArray) && descriptor.mipCount == 1 && arrayLayers == 1 && !IsBlockCompressed(descriptor.format) && (elementBytes == 1 || (elementBytes == 2 && std::getenv("ANYPS5_PROMOTE_16BIT") != nullptr) || elementBytes == 4 || elementBytes == 8 || elementBytes == 16) && (descriptor.tileMode == TextureTileMode::RenderTarget64KB || descriptor.tileMode == TextureTileMode::kLinear);

        const auto guestBytes = ComputeSurfaceSize(mips, arrayLayers);
        const auto guestSliceBytes = guestBytes / tiledLayers;
        Require(snapshot.size() == guestBytes, "texture snapshot size mismatch");

        const auto sliceLinearBytes = SliceLinearBytes(mips);
        Require(tiledLayers == 0 || sliceLinearBytes <= UINT64_MAX / tiledLayers, "detiled texture buffer size overflows");
        const auto linearBytes = sliceLinearBytes * tiledLayers;

        bool storageCapable = false;
        if (context.storageImages && !IsBlockCompressed(descriptor.format)) {
            VkFormatProperties properties{};
            context.formatProperties(context.physical, LinearFormat(vkFormat), &properties);
            storageCapable = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
        }
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.flags = descriptor.dimension == TextureDimension::kCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u;
        if (storageCapable && LinearFormat(vkFormat) != vkFormat) imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        imageInfo.imageType = ImageTypeFor(descriptor.dimension);
        imageInfo.format = vkFormat;
        imageInfo.extent = {descriptor.width, descriptor.height, volume ? arrayLayers : 1u};
        imageInfo.mipLevels = descriptor.mipCount;
        imageInfo.arrayLayers = imageLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | (storageCapable ? VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u);
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &image), "vkCreateImage");
        timing.Mark("image");
        layout = storageCapable ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocationBytes = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory texture");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory");
        timing.Mark("memory", requirements.size);

        {
            staging = std::make_unique<Buffer>(context, static_cast<std::size_t>(guestBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            std::memcpy(staging->Bytes().data(), snapshot.data(), snapshot.size());
            timing.Mark("staging", snapshot.size());
            linear = std::make_unique<Buffer>(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            timing.Mark("linear", linearBytes);

            std::unique_ptr<CommandBatch> batch;
            VkCommandBuffer commands = VK_NULL_HANDLE;
            if (context.drawQueue != nullptr) commands = context.drawQueue->Begin(context);
            else {
                detiler.BeginBatch();
                batch = std::make_unique<CommandBatch>(context);
                commands = batch->Handle();
            }

            VkBufferMemoryBarrier stagingReadBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            stagingReadBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            stagingReadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            stagingReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            stagingReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            stagingReadBarrier.buffer = staging->Handle();
            stagingReadBarrier.offset = 0;
            stagingReadBarrier.size = VK_WHOLE_SIZE;

            VkBufferMemoryBarrier linearWriteBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            linearWriteBarrier.srcAccessMask = 0;
            linearWriteBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            linearWriteBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearWriteBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearWriteBarrier.buffer = linear->Handle();
            linearWriteBarrier.offset = 0;
            linearWriteBarrier.size = VK_WHOLE_SIZE;

            const VkBufferMemoryBarrier preBarriers[] = {stagingReadBarrier, linearWriteBarrier};
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 2, preBarriers, 0, nullptr);

            for (std::uint32_t layer = 0; layer < tiledLayers; ++layer) {
                const auto guestLayerOffset = static_cast<std::uint64_t>(layer) * guestSliceBytes;
                const auto linearLayerOffset = static_cast<std::uint64_t>(layer) * sliceLinearBytes;
                for (const auto& mip : mips) {
                    detiler.Dispatch(commands, descriptor.tileMode, elementBytes, staging->Handle(), guestLayerOffset + mip.tiledOffset, linear->Handle(), linearLayerOffset + mip.linearOffset, mip, layer);
                }
            }

            VkBufferMemoryBarrier linearReadBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            linearReadBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            linearReadBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            linearReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            linearReadBarrier.buffer = linear->Handle();
            linearReadBarrier.offset = 0;
            linearReadBarrier.size = VK_WHOLE_SIZE;

            VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransferDst.srcAccessMask = 0;
            toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.image = image;
            toTransferDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, imageLayers};
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearReadBarrier, 1, &toTransferDst);

            std::vector<VkBufferImageCopy> regions;
            regions.reserve(static_cast<std::size_t>(tiledLayers) * mips.size());
            for (std::uint32_t layer = 0; layer < tiledLayers; ++layer) {
                const auto linearLayerOffset = static_cast<std::uint64_t>(layer) * sliceLinearBytes;
                for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
                    if (volume && layer >= std::max(arrayLayers >> level, 1u)) break;
                    const auto& mip = mips[level];
                    VkBufferImageCopy region{};
                    region.bufferOffset = linearLayerOffset + mip.linearOffset;
                    region.bufferRowLength = mip.pitchBytes / elementBytes * BlockWidth(descriptor.format);
                    region.bufferImageHeight = 0;
                    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, volume ? 0u : layer, 1};
                    region.imageOffset = {0, 0, volume ? static_cast<std::int32_t>(layer) : 0};
                    region.imageExtent = {std::max(descriptor.width >> level, 1u), std::max(descriptor.height >> level, 1u), mip.depth};
                    regions.push_back(region);
                }
            }
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());

            VkImageMemoryBarrier toShaderRead{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toShaderRead.newLayout = layout;
            toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toShaderRead.image = image;
            toShaderRead.subresourceRange = toTransferDst.subresourceRange;
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShaderRead);

            if (batch != nullptr) {
                batch->SubmitAndWait();
                ReleaseUpload();
            } else {
                detiler.Retire(*context.drawQueue);
            }
            timing.Mark("record");
        }

        createGuestViews(descriptor, components, vkFormat, storageCapable);
        timing.Mark("views");
    } catch (...) {
        release();
        throw;
    }
}

Texture::Texture(const Context& context, const std::shared_ptr<Texture>& shared, const GuestTextureResource& descriptor, VkComponentMapping components) : context(context), sharedImage(shared), ownsImage(false) {
    try {
        Require(shared != nullptr && shared->ownsImage && shared->image != VK_NULL_HANDLE, "shared texture surface is unavailable");
        const auto vkFormat = ResolveTextureFormat(descriptor.format);
        Require(vkFormat == shared->guestFormat && shared->extent.width == descriptor.width && shared->extent.height == descriptor.height && shared->guestMipCount == descriptor.mipCount && shared->guestLayers == FullArrayLayers(descriptor), "shared texture surface does not match the descriptor");
        image = shared->image;
        layout = shared->layout;
        extent = shared->extent;
        imageLayers = shared->imageLayers;
        guestAddress = descriptor.baseAddress;
        guestTileMode = descriptor.tileMode;
        guestFormat = vkFormat;
        guestTexelBytes = shared->guestTexelBytes;
        guest2D = descriptor.dimension == TextureDimension::k2D;
        guestMipCount = descriptor.mipCount;
        guestDimension = static_cast<std::uint32_t>(descriptor.dimension);
        guestLayers = shared->guestLayers;
        promotable = shared->promotable && (descriptor.dimension == TextureDimension::k2D || descriptor.dimension == TextureDimension::k2DArray);
        createGuestViews(descriptor, components, vkFormat, shared->storageFormat != VK_FORMAT_UNDEFINED);
    } catch (...) {
        release();
        throw;
    }
}

void Texture::createGuestViews(const GuestTextureResource& descriptor, VkComponentMapping components, VkFormat vkFormat, bool storageCapable) {
        const auto viewLevelCount = descriptor.lastLevel - descriptor.baseLevel + 1u;
        Require(descriptor.baseArray < imageLayers, "guest texture view starts past its last array slice");
        auto viewLayerCount = imageLayers - descriptor.baseArray;
        if (descriptor.viewDimension == TextureDimension::k1D || descriptor.viewDimension == TextureDimension::k2D) viewLayerCount = 1u;
        if (descriptor.viewDimension == TextureDimension::kCube) {
            Require(viewLayerCount % 6u == 0, "guest cube texture view does not contain a multiple of 6 array slices");
        }

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = ViewTypeFor(descriptor.viewDimension, viewLayerCount);
        viewInfo.format = vkFormat;
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, descriptor.baseLevel, viewLevelCount, descriptor.baseArray, viewLayerCount};
        VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
        minLod.minLod = std::clamp(descriptor.minLod, static_cast<float>(descriptor.baseLevel), static_cast<float>(descriptor.lastLevel));
        if (minLod.minLod > static_cast<float>(descriptor.baseLevel)) {
            if (context.imageViewMinLod) viewInfo.pNext = &minLod;
            else {
                static std::once_flag once;
                std::call_once(once, [&] { APS5_LOG_OUT("the device cannot clamp image views to a minimum LOD; guest textures with a minimum LOD (%.2f) sample their full mip range", descriptor.minLod); });
            }
        }
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView");
        if (storageCapable) {
            storageFormat = LinearFormat(vkFormat);
            storageViewType = viewInfo.viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewInfo.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : viewInfo.viewType;
            storageRange = viewInfo.subresourceRange;
        }
}

Texture::~Texture() {
    release();
}

VkImageView Texture::StorageView() {
    if (storageFormat == VK_FORMAT_UNDEFINED) return VK_NULL_HANDLE;
    if (storageView == VK_NULL_HANDLE) {
        VkImageViewUsageCreateInfo viewUsage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
        viewUsage.usage = VK_IMAGE_USAGE_STORAGE_BIT;
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.pNext = &viewUsage;
        viewInfo.image = image;
        viewInfo.viewType = storageViewType;
        viewInfo.format = storageFormat;
        viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewInfo.subresourceRange = storageRange;
        viewInfo.subresourceRange.levelCount = 1;
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &storageView), "vkCreateImageView storage");
    }
    return storageView;
}

void Texture::ReleaseUpload() {
    staging.reset();
    linear.reset();
}

void Texture::release() noexcept {
    upload.reset();
    staging.reset();
    linear.reset();
    ReleaseImage(context, storageView, VK_NULL_HANDLE, VK_NULL_HANDLE);
    ReleaseImage(context, ownsView ? view : VK_NULL_HANDLE, ownsImage ? image : VK_NULL_HANDLE, ownsImage ? memory : VK_NULL_HANDLE);
    sharedImage.reset();
}

VkImageView Texture::View() const {
    return view;
}

}

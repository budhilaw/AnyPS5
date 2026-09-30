#include <stdexcept>
#include <cstdio>
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libc/include/General.hpp"
#include <mutex>

namespace AgcDriver::Graphics {

namespace {

VkFormat SampledDepthFormat(VkFormat depthFormat, bool stencil) {
    if (stencil) return VK_FORMAT_R8_UINT;
    switch (depthFormat) {
        case VK_FORMAT_D16_UNORM: return VK_FORMAT_R16_UNORM;
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return VK_FORMAT_R32_SFLOAT;
        default: throw std::runtime_error("AGC graphics: depth format cannot be sampled as a texture");
    }
}

}

Texture::Texture(const Context& context, std::span<const std::shared_ptr<DepthImage>> sources, bool stencil, const GuestTextureResource& descriptor, VkComponentMapping components, bool compare) : context(context), depthSources(sources.begin(), sources.end()) {
    try {
        const auto layers = FullArrayLayers(descriptor);
        const bool arrayView = descriptor.viewDimension == TextureDimension::k2DArray;
        const bool supported = descriptor.dimension == TextureDimension::k2D || descriptor.dimension == TextureDimension::k2DArray;
        if (sources.size() != layers || !supported || descriptor.mipCount != 1 || descriptor.baseLevel != 0 || descriptor.baseArray >= layers || (!arrayView && descriptor.viewDimension != TextureDimension::k2D)) {
            char message[176];
            std::snprintf(message, sizeof(message), "AGC graphics: invalid depth texture view (dimension %u view %u last array %u mips %u base level %u base array %u, %zu resident slices)", static_cast<unsigned>(descriptor.dimension), static_cast<unsigned>(descriptor.viewDimension), descriptor.depthOrLastArray, descriptor.mipCount, descriptor.baseLevel, descriptor.baseArray, sources.size());
            throw std::runtime_error(message);
        }
        const auto& target = sources.front()->Description();
        Require(!stencil || target.stencil, "stencil texture view of a depth target without stencil");
        for (const auto& source : sources) {
            const auto& slice = source->Description();
            Require(slice.format == target.format && slice.stencil == target.stencil && slice.extent.width == target.extent.width && slice.extent.height == target.extent.height, "depth texture slices differ in format or size");
        }
        const auto format = compare ? (target.format == VK_FORMAT_D16_UNORM ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT) : SampledDepthFormat(target.format, stencil);
        const VkImageAspectFlags aspect = compare ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        const auto texelBytes = format == VK_FORMAT_R8_UINT ? 1u : (format == VK_FORMAT_R16_UNORM || format == VK_FORMAT_D16_UNORM) ? 2u : 4u;
        if (IsBlockCompressed(descriptor.format) || BytesPerElement(descriptor.format) != texelBytes) {
            char message[200];
            std::snprintf(message, sizeof(message), "AGC graphics: depth texture view 0x%llx format 0x%x (%u bytes) does not match depth target format %d (%u bytes%s)", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.format, BytesPerElement(descriptor.format), static_cast<int>(target.format), texelBytes, stencil ? ", stencil view" : "");
            throw std::runtime_error(message);
        }
        Require(descriptor.width == target.extent.width && descriptor.height == target.extent.height, "depth texture view extent differs from the depth target");
        VkFormatProperties properties{};
        context.formatProperties(context.physical, format, &properties);
        Require((properties.optimalTilingFeatures & (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) == (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT), "depth texture format does not support sampling and copies");
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {descriptor.width, descriptor.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = layers;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage depth texture");
        extent = {descriptor.width, descriptor.height};
        guestAddress = descriptor.baseAddress;
        guestFormat = format;
        guestTexelBytes = texelBytes;
        guestMipCount = 1;
        guestLayers = layers;
        guestDimension = static_cast<std::uint32_t>(descriptor.dimension);
        if (stencil) {
            static std::once_flag once;
            std::call_once(once, [&] { APS5_LOG_OUT("stencil texture view of depth target 0x%llx (%ux%u)", static_cast<unsigned long long>(descriptor.baseAddress), descriptor.width, descriptor.height); });
        }
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocationBytes = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory depth texture");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory depth texture");
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = arrayView ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components = components;
        viewInfo.subresourceRange = {aspect, 0, 1, descriptor.baseArray, arrayView ? layers - descriptor.baseArray : 1u};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView depth texture");
        const std::size_t sliceBytes = static_cast<std::size_t>(descriptor.width) * descriptor.height * texelBytes;
        staging = std::make_unique<Buffer>(context, sliceBytes * layers, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (context.drawQueue) context.drawQueue->Flush();
        upload = std::make_unique<CommandBatch>(context);
        const auto commands = upload->Handle();
        for (std::uint32_t layer = 0; layer < layers; ++layer) {
            const bool readOnly = sources[layer]->ReadOnly();
            sources[layer]->Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkBufferImageCopy toBuffer{};
            toBuffer.bufferOffset = sliceBytes * layer;
            toBuffer.imageSubresource = {stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
            toBuffer.imageExtent = info.extent;
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, sources[layer]->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging->Handle(), 1, &toBuffer);
            if (readOnly) sources[layer]->Transition(commands, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        }
        const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        VkBufferMemoryBarrier bufferBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.buffer = staging->Handle();
        bufferBarrier.size = VK_WHOLE_SIZE;
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {aspect, 0, 1, 0, layers};
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 1, &barrier);
        VkBufferImageCopy toImage{};
        toImage.imageSubresource = {aspect, 0, 0, layers};
        toImage.imageExtent = info.extent;
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, staging->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &toImage);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        layout = barrier.newLayout;
        upload->Submit();
    } catch (...) {
        release();
        throw;
    }
}

Texture::Texture(const Context& context, const std::shared_ptr<DepthImage>& depth, VkImageView sampledView, bool stencil, const GuestTextureResource& descriptor, DirectDepthView) : context(context), depthSources{depth}, ownsImage(false), ownsView(false), directView(true) {
    Require(depth != nullptr && sampledView != VK_NULL_HANDLE, "direct depth texture view has no sampled view");
    const auto& target = depth->Description();
    Require(!stencil || target.stencil, "stencil texture view of a depth target without stencil");
    view = sampledView;
    layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    extent = {descriptor.width, descriptor.height};
    imageLayers = 1;
    guestAddress = descriptor.baseAddress;
    guestTileMode = descriptor.tileMode;
    guestFormat = target.format;
    guestTexelBytes = stencil ? 1u : target.format == VK_FORMAT_D16_UNORM ? 2u : 4u;
    guest2D = descriptor.dimension == TextureDimension::k2D;
    guestMipCount = 1;
    guestLayers = 1;
    guestDimension = static_cast<std::uint32_t>(descriptor.dimension);
}

}

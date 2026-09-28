#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libc/include/General.hpp"
#include <mutex>

namespace AgcDriver::Graphics {

namespace {

// The single-channel format a depth image's depth aspect copies into through a buffer: the
// same texel size, with the sampled value equal to the depth value.
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

Texture::Texture(const Context& context, const std::shared_ptr<DepthImage>& source, bool stencil, const GuestTextureResource& descriptor, VkComponentMapping components) : context(context), depthSource(source) {
    try {
        Require(source != nullptr && descriptor.dimension == TextureDimension::k2D && descriptor.mipCount == 1 && descriptor.baseLevel == 0 && descriptor.baseArray == 0, "invalid depth texture view");
        const auto& target = source->Description();
        Require(!stencil || target.stencil, "stencil texture view of a depth target without stencil");
        const auto format = SampledDepthFormat(target.format, stencil);
        const auto texelBytes = format == VK_FORMAT_R8_UINT ? 1u : format == VK_FORMAT_R16_UNORM ? 2u : 4u;
        Require(!IsBlockCompressed(descriptor.format) && BytesPerElement(descriptor.format) == texelBytes, "depth texture view requires a texel size matching the depth target");
        Require(descriptor.width == target.extent.width && descriptor.height == target.extent.height, "depth texture view extent differs from the depth target");
        VkFormatProperties properties{};
        context.formatProperties(context.physical, format, &properties);
        Require((properties.optimalTilingFeatures & (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) == (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT), "depth texture format does not support sampling and copies");
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {descriptor.width, descriptor.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
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
        guestLayers = 1;
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
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView depth texture");
        // Depth and color formats cannot be copied image to image: the depth aspect goes through
        // a buffer, where its texels are plain 16-bit or 32-bit values.
        const std::size_t bytes = static_cast<std::size_t>(descriptor.width) * descriptor.height * texelBytes;
        staging = std::make_unique<Buffer>(context, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (context.drawQueue) context.drawQueue->Flush();
        upload = std::make_unique<CommandBatch>(context);
        const auto commands = upload->Handle();
        source->Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkBufferImageCopy toBuffer{};
        toBuffer.imageSubresource = {stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
        toBuffer.imageExtent = info.extent;
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, source->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging->Handle(), 1, &toBuffer);
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
        barrier.subresourceRange = viewInfo.subresourceRange;
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 1, &barrier);
        VkBufferImageCopy toImage{};
        toImage.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
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

}

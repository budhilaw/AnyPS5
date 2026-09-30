#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include <set>
#include <mutex>
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"

namespace AgcDriver::Graphics {

Texture::Texture(const Context& context, const std::shared_ptr<ResidentColor>& source, const GuestTextureResource& descriptor, VkComponentMapping components) : context(context), source(source) {
    try {
        const bool singleSliceArray = descriptor.dimension == TextureDimension::k2DArray && descriptor.depthOrLastArray == 0;
        const bool oneRow = descriptor.dimension == TextureDimension::k1D && source != nullptr && source->Description().extent.height == 1 && descriptor.height <= 1;
        Require(source != nullptr && (descriptor.dimension == TextureDimension::k2D || singleSliceArray || oneRow) && descriptor.mipCount == 1 && descriptor.baseLevel == 0 && descriptor.baseArray == 0, "invalid resident texture view");
        const auto format = ResolveTextureFormat(descriptor.format);
        Require(!IsBlockCompressed(descriptor.format) && BytesPerElement(descriptor.format) == source->Description().bytesPerPixel, "resident texture copy requires a texel size matching the render target");
        VkFormatProperties properties{};
        context.formatProperties(context.physical, format, &properties);
        Require((properties.optimalTilingFeatures & (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) == (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT), "resident texture format does not support sampling and copies");
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = oneRow ? VK_IMAGE_TYPE_1D : VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {descriptor.width, oneRow ? 1u : descriptor.height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        const bool storageCapable = context.storageImages && (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
        info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | (storageCapable ? VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u);
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage resident texture");
        extent = {descriptor.width, descriptor.height};
        guestAddress = descriptor.baseAddress;
        guestFormat = format;
        guestTexelBytes = source->Description().bytesPerPixel;
        guestMipCount = 1;
        guestLayers = 1;
        guestDimension = static_cast<std::uint32_t>(descriptor.dimension);
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocationBytes = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory resident texture");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory resident texture");
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = oneRow ? VK_IMAGE_VIEW_TYPE_1D : descriptor.viewDimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView resident texture");
        if (context.drawQueue) context.drawQueue->Flush();
        upload = std::make_unique<CommandBatch>(context);
        const auto commands = upload->Handle();
        source->Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = viewInfo.subresourceRange;
        const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        if (oneRow) {
            const VkDeviceSize rowBytes = static_cast<VkDeviceSize>(descriptor.width) * source->Description().bytesPerPixel;
            staging = std::make_unique<Buffer>(context, static_cast<std::size_t>(rowBytes), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            VkBufferImageCopy row{};
            row.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            row.imageExtent = {descriptor.width, 1, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, source->Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging->Handle(), 1, &row);
            VkMemoryBarrier written{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            written.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            written.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &written, 0, nullptr, 0, nullptr);
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, staging->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &row);
        } else {
            VkImageCopy copy{};
            copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.dstSubresource = copy.srcSubresource;
            copy.extent = info.extent;
            context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source->Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        }
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = storageCapable ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        layout = barrier.newLayout;
        if (storageCapable) {
            storageFormat = format;
            storageViewType = oneRow ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_2D;
            storageRange = viewInfo.subresourceRange;
        }
        upload->Submit();
    } catch (...) {
        release();
        throw;
    }
}

void Texture::MarkStored() {
    stored = true;
    if (directView && source) source->MarkWritten();
}

Texture::Texture(const Context& context, const std::shared_ptr<ResidentColor>& source, const GuestTextureResource& descriptor, VkComponentMapping components, DirectView) : context(context), source(source), ownsImage(false), directView(true) {
    try {
        const bool singleSliceArray = descriptor.dimension == TextureDimension::k2DArray && descriptor.depthOrLastArray == 0;
        Require(source != nullptr && (descriptor.dimension == TextureDimension::k2D || singleSliceArray) && descriptor.mipCount == 1 && descriptor.baseLevel == 0 && descriptor.baseArray == 0, "invalid resident texture view");
        const auto format = ResolveTextureFormat(descriptor.format);
        Require(!IsBlockCompressed(descriptor.format) && BytesPerElement(descriptor.format) == source->Description().bytesPerPixel, "resident texture view requires a texel size matching the render target");
        auto& target = source->Target();
        image = target.Image();
        layout = VK_IMAGE_LAYOUT_GENERAL;
        extent = {descriptor.width, descriptor.height};
        imageLayers = 1;
        guestAddress = descriptor.baseAddress;
        guestTileMode = descriptor.tileMode;
        guestFormat = format;
        guestTexelBytes = source->Description().bytesPerPixel;
        guest2D = descriptor.dimension == TextureDimension::k2D;
        guestMipCount = 1;
        guestLayers = 1;
        guestDimension = static_cast<std::uint32_t>(descriptor.dimension);
        VkImageViewUsageCreateInfo viewUsage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
        viewUsage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.pNext = &viewUsage;
        viewInfo.image = image;
        viewInfo.viewType = descriptor.viewDimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components = components;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView resident view");
        VkFormatProperties storageProperties{};
        context.formatProperties(context.physical, StorageFormat(format), &storageProperties);
        if (target.StorageCapable() && (storageProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0) {
            storageFormat = StorageFormat(format);
            storageViewType = viewInfo.viewType;
            storageRange = viewInfo.subresourceRange;
        }
        {
            static std::mutex reportMutex;
            static std::set<std::uint64_t> reported;
            std::lock_guard lock(reportMutex);
            if (reported.insert((static_cast<std::uint64_t>(source->Description().format) << 32) | format).second) APS5_LOG_OUT("direct view of render target format %u (%u bpp%s, storage %s) through descriptor format 0x%x = vk%u", static_cast<unsigned>(source->Description().format), source->Description().bytesPerPixel, source->Description().gpuOnly ? ", GPU-only" : "", target.StorageCapable() ? "yes" : "no", descriptor.format, static_cast<unsigned>(format));
        }
    } catch (...) {
        release();
        throw;
    }
}

void Texture::FlushStores() {
    if (!stored) return;
    stored = false;
    if (directView) return;
    if (!source) {
        if (!promotable || context.renderCache == nullptr) {
            static int reportedKept = 0;
            if (reportedKept++ < 60) APS5_LOG_OUT("shader stores into texture 0x%llx (%ux%u, %u bpp, tile %u, %u mips, dimension %u, %u layers) stay on the device", static_cast<unsigned long long>(guestAddress), extent.width, extent.height, guestTexelBytes, static_cast<unsigned>(guestTileMode), guestMipCount, guestDimension, guestLayers);
            return;
        }
        const ColorTileMode mode = guestTileMode == TextureTileMode::RenderTarget64KB ? ColorTileMode::RenderTarget : ColorTileMode::Linear;
        const ColorTargetLayout layoutInfo(extent.width, extent.height, mode, guestTexelBytes);
        ColorTarget target{guestAddress, extent, guestFormat, layoutInfo.Bytes(), 0xe4u, mode, guestTexelBytes, guestTexelBytes != 4};
        if (target.gpuOnly) {
            try {
                GuestMemory::CheckGpuRange(reinterpret_cast<const void*>(target.address), target.bytes, layoutInfo.Alignment(), true);
            } catch (const std::exception&) {
                const auto mapped = GuestMemory::MappedGpuBytes(reinterpret_cast<const void*>(target.address), target.bytes, true);
                Require(mapped != 0, "stored texture starts in unmapped guest memory");
                target.bytes = mapped;
            }
        }
        auto resident = context.renderCache->Get(target, false);
        static int reported = 0;
        if (reported++ < 32) APS5_LOG_OUT("shader stores into texture 0x%llx (%ux%u, %u bpp) promoted to a resident render target", static_cast<unsigned long long>(guestAddress), extent.width, extent.height, guestTexelBytes);
        if (context.drawQueue) context.drawQueue->Flush();
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        VkImageMemoryBarrier toSource{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toSource.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toSource.oldLayout = layout;
        toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSource.image = image;
        toSource.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        pipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
        resident->Adopt(commands, image);
        toSource.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toSource.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        toSource.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSource.newLayout = layout;
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
        batch.SubmitAndWait();
        return;
    }
    if (context.drawQueue) context.drawQueue->Flush();
    CommandBatch batch(context);
    const auto commands = batch.Handle();
    const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    VkImageMemoryBarrier toSource{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toSource.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSource.oldLayout = layout;
    toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.image = image;
    toSource.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    pipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
    source->Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageCopy copy{};
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.dstSubresource = copy.srcSubresource;
    copy.extent = {extent.width, extent.height, 1};
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, source->Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    toSource.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSource.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    toSource.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSource.newLayout = layout;
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
    batch.SubmitAndWait();
    source->MarkWritten();
}

}

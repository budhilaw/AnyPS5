#include "prx/libSceAgcDriver/Graphics/include/DispatchRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CaptureObjects.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <set>
#include <stdexcept>

namespace AgcDriver::Graphics {

namespace {

struct StoredRegion {
    std::weak_ptr<const void> owner;
    const std::byte* data = nullptr;
    std::size_t size = 0;
    std::filesystem::path root;
    std::string blob;
};

constexpr std::size_t StoredRegionLimit = 256;

std::mutex& storedMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<StoredRegion>& storedRegions() {
    static std::vector<StoredRegion> regions;
    return regions;
}

void addSubresources(std::vector<std::array<std::uint32_t, 3>>& target, const CapturedView& view, const CapturedImage& image) {
    const auto levels = view.range.levelCount == VK_REMAINING_MIP_LEVELS ? image.levels - std::min(view.range.baseMipLevel, image.levels) : view.range.levelCount;
    const auto layers = image.type == VK_IMAGE_TYPE_3D ? 1u : view.range.layerCount == VK_REMAINING_ARRAY_LAYERS ? image.layers - std::min(view.range.baseArrayLayer, image.layers) : view.range.layerCount;
    const auto baseLayer = image.type == VK_IMAGE_TYPE_3D ? 0u : view.range.baseArrayLayer;
    for (const VkImageAspectFlags aspect : {VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_STENCIL_BIT}) {
        if ((view.range.aspectMask & aspect) == 0) continue;
        for (std::uint32_t level = view.range.baseMipLevel; level < view.range.baseMipLevel + levels && level < image.levels; ++level) {
            for (std::uint32_t layer = baseLayer; layer < baseLayer + layers && layer < image.layers; ++layer) {
                const std::array<std::uint32_t, 3> subresource{aspect, level, layer};
                if (std::find(target.begin(), target.end(), subresource) == target.end()) target.push_back(subresource);
            }
        }
    }
    std::sort(target.begin(), target.end());
}

std::string hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "%llx", static_cast<unsigned long long>(value));
    return text;
}

void resolveGuest(const DispatchBindings::BoundRegion& region, std::uint64_t begin, std::uint64_t end) {
    if (region.bytes.data() != reinterpret_cast<const std::byte*>(region.begin - region.padding) || end <= begin) return;
    GuestMemory::CheckRange(reinterpret_cast<const void*>(begin), static_cast<std::size_t>(end - begin), 1, false);
}

}

DispatchRecorder::DispatchRecorder(const Context& context, const CaptureTarget& target, const ShaderRecompiler::RecompileResult& shader, std::array<std::uint32_t, 3> groups, DispatchBindings bindings) : context(context), target(target), shader(shader), groups(groups), bindings(std::move(bindings)) {
    std::filesystem::create_directories(target.root);
    const auto base = hex(target.program) + "-" + std::to_string(groups[0]) + "x" + std::to_string(groups[1]) + "x" + std::to_string(groups[2]) + "-";
    for (auto index = std::max(target.index, 1u);; ++index) {
        directory = target.root / (base + std::to_string(index));
        std::error_code error;
        if (!std::filesystem::exists(directory, error) && std::filesystem::create_directory(directory)) break;
    }
    manifest.program = target.program;
    manifest.codeHash = target.codeHash;
    manifest.groups = groups;
    manifest.lanes = shader.lanesPerInvocation;
    manifest.spirv = CaptureSpirvName;
    manifest.request = target.request.empty() ? std::string() : std::string(CaptureRequestName);
    if (!shader.pushConstants.empty()) {
        const std::array<CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
        const auto block = AssemblePushConstants(shaders);
        manifest.push.assign(block.begin(), block.end());
    }
    for (const auto& layout : this->bindings.layout) manifest.layout.push_back({layout.binding, static_cast<std::uint32_t>(layout.descriptorType), layout.descriptorCount});
    for (const auto& binding : shader.bindings) {
        CaptureManifest::Descriptor descriptor;
        descriptor.binding = binding.binding;
        descriptor.role = static_cast<std::uint32_t>(binding.role);
        descriptor.kind = static_cast<std::uint32_t>(binding.kind);
        descriptor.count = binding.count;
        descriptor.imageDepthCompare = binding.imageDepthCompare ? 1u : 0u;
        for (const bool compare : binding.samplerDepthCompare) descriptor.samplerDepthCompare.push_back(compare ? 1u : 0u);
        descriptor.words = binding.guestDescriptor;
        manifest.descriptors.push_back(std::move(descriptor));
    }
}

std::string DispatchRecorder::store(std::span<const std::byte> bytes) {
    storedBytes += bytes.size();
    return WriteCaptureBlob(target.root, bytes);
}

std::string DispatchRecorder::storeRegion(const DispatchBindings::BoundRegion& region) {
    if (region.immutable && region.owner) {
        std::lock_guard lock(storedMutex());
        for (const auto& stored : storedRegions()) {
            const auto owner = stored.owner.lock();
            if (owner && owner.get() == region.owner.get() && stored.data == region.bytes.data() && stored.size == region.bytes.size() && stored.root == target.root) return stored.blob;
        }
    }
    auto blob = store(region.bytes);
    if (region.immutable && region.owner) {
        std::lock_guard lock(storedMutex());
        auto& stored = storedRegions();
        if (stored.size() >= StoredRegionLimit) stored.erase(stored.begin());
        stored.push_back({region.owner, region.bytes.data(), region.bytes.size(), target.root, blob});
    }
    return blob;
}

const DispatchBindings::BoundRegion* DispatchRecorder::regionOf(std::uint64_t begin, std::uint64_t end) const {
    for (const auto& region : bindings.regions) {
        if (begin >= region.begin && end <= region.end) return &region;
    }
    return nullptr;
}

void DispatchRecorder::planImages() {
    plans.clear();
    for (const auto& bound : bindings.images) {
        const auto view = CaptureObjects::View(bound.view);
        const auto image = view ? CaptureObjects::Image(view->image) : std::nullopt;
        auto plan = view && image ? std::find_if(plans.begin(), plans.end(), [&](const ImagePlan& candidate) { return candidate.known && candidate.image == view->image; }) : plans.end();
        if (plan == plans.end()) {
            plans.push_back({view ? view->image : VK_NULL_HANDLE, bound.layout, {}, {}, view && image});
            plan = plans.end() - 1;
        }
        if (!plan->known) continue;
        addSubresources(plan->before, *view, *image);
        if (bound.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) addSubresources(plan->after, *view, *image);
    }
}

std::vector<std::byte> DispatchRecorder::copyImage(const ImagePlan& plan, const CaptureManifest::Image& image, std::span<const std::array<std::uint32_t, 3>> subresources, std::vector<CaptureManifest::Subresource>& layout) {
    const auto format = static_cast<VkFormat>(image.format);
    std::uint64_t total = 0;
    layout = CaptureLayoutSubresources(format, static_cast<VkImageType>(image.type), {image.width, image.height, image.depth}, subresources, total);
    if (total == 0) return {};
    std::unique_ptr<Buffer> staging;
    try {
        staging = std::make_unique<Buffer>(context, static_cast<std::size_t>(total), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    } catch (const std::exception&) {
        staging = std::make_unique<Buffer>(context, static_cast<std::size_t>(total), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }
    const bool general = plan.layout == VK_IMAGE_LAYOUT_GENERAL;
    const auto copyLayout = general ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    std::vector<VkImageMemoryBarrier> toSource;
    std::vector<VkImageMemoryBarrier> toOriginal;
    std::vector<VkBufferImageCopy> copies;
    std::set<std::pair<std::uint32_t, std::uint32_t>> transitioned;
    for (const auto& subresource : layout) {
        if (transitioned.insert({subresource.level, subresource.layer}).second) {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.oldLayout = plan.layout;
            barrier.newLayout = copyLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = plan.image;
            barrier.subresourceRange = {CaptureFormatAspects(format), subresource.level, 1, subresource.layer, 1};
            toSource.push_back(barrier);
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            barrier.oldLayout = copyLayout;
            barrier.newLayout = plan.layout;
            toOriginal.push_back(barrier);
        }
        VkBufferImageCopy copy{};
        copy.bufferOffset = subresource.offset;
        copy.imageSubresource = {subresource.aspect, subresource.level, subresource.layer, 1};
        copy.imageExtent = {subresource.width, subresource.height, subresource.depth};
        copies.push_back(copy);
    }
    const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    CommandBatch batch(context);
    const auto commands = batch.Handle();
    pipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(toSource.size()), toSource.data());
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, plan.image, copyLayout, staging->Handle(), static_cast<std::uint32_t>(copies.size()), copies.data());
    const VkMemoryBarrier toHost{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &toHost, 0, nullptr, static_cast<std::uint32_t>(toOriginal.size()), toOriginal.data());
    batch.SubmitAndWait();
    const auto bytes = staging->Bytes();
    return {bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(total)};
}

void DispatchRecorder::Before() {
    Require(!before, "dispatch capture inputs were already recorded");
    for (const auto& region : bindings.regions) {
        Require(region.bytes.size() == region.padding + (region.end - region.begin), "dispatch capture region bytes do not cover the region");
        resolveGuest(region, region.begin - region.padding, region.end);
    }
    for (const auto& region : bindings.regions) manifest.regions.push_back({region.begin, region.end, region.padding, region.writable, region.imported, region.memory, storeRegion(region)});
    for (const auto& bound : bindings.buffers) {
        CaptureManifest::Buffer buffer;
        buffer.binding = bound.binding;
        buffer.element = bound.element;
        buffer.source = bound.source;
        buffer.address = bound.address;
        buffer.size = bound.size;
        buffer.memory = bound.memory;
        if (bound.source == CaptureBufferSource::Data || bound.source == CaptureBufferSource::Gds || bound.source == CaptureBufferSource::Table) buffer.blob = store(bound.bytes);
        if (bound.source == CaptureBufferSource::Guest && regionOf(bound.address, bound.address + bound.size) == nullptr) manifest.notes.push_back("guest buffer 0x" + hex(bound.address) + "+0x" + hex(bound.size) + " of binding " + std::to_string(bound.binding) + " lies outside every captured region");
        manifest.buffers.push_back(std::move(buffer));
    }
    planImages();
    for (const auto& plan : plans) {
        CaptureManifest::Image image;
        image.layout = static_cast<std::uint32_t>(plan.layout);
        const auto info = plan.known ? CaptureObjects::Image(plan.image) : std::nullopt;
        if (info) {
            image.type = static_cast<std::uint32_t>(info->type);
            image.format = static_cast<std::uint32_t>(info->format);
            image.width = info->extent.width;
            image.height = info->extent.height;
            image.depth = info->extent.depth;
            image.levels = info->levels;
            image.layers = info->layers;
            image.flags = info->flags;
            image.usage = info->usage;
            if (!info->copyable) manifest.notes.push_back("image " + std::to_string(manifest.images.size()) + " cannot be copied; its contents were not captured");
            else if (plan.layout == VK_IMAGE_LAYOUT_UNDEFINED) manifest.notes.push_back("image " + std::to_string(manifest.images.size()) + " is bound in an undefined layout; its contents were not captured");
            else {
                try {
                    const auto bytes = copyImage(plan, image, plan.before, image.contents);
                    if (!bytes.empty()) image.blob = store(bytes);
                } catch (const std::exception& error) {
                    image.contents.clear();
                    manifest.notes.push_back("image " + std::to_string(manifest.images.size()) + " contents were not captured: " + error.what());
                }
            }
        } else {
            manifest.notes.push_back("image " + std::to_string(manifest.images.size()) + " was not created through the capture registry; its parameters are unknown");
        }
        manifest.images.push_back(std::move(image));
    }
    for (const auto& bound : bindings.images) {
        CaptureManifest::View record;
        record.binding = bound.binding;
        record.element = bound.element;
        record.type = static_cast<std::uint32_t>(bound.type);
        record.layout = static_cast<std::uint32_t>(bound.layout);
        const auto view = CaptureObjects::View(bound.view);
        const auto plan = std::find_if(plans.begin(), plans.end(), [&](const ImagePlan& candidate) { return view ? candidate.image == view->image : candidate.image == VK_NULL_HANDLE; });
        record.image = static_cast<std::uint32_t>(plan - plans.begin());
        if (view) {
            const auto info = CaptureObjects::Image(view->image);
            record.viewType = static_cast<std::uint32_t>(view->type);
            record.format = static_cast<std::uint32_t>(view->format);
            record.components = {static_cast<std::uint32_t>(view->components.r), static_cast<std::uint32_t>(view->components.g), static_cast<std::uint32_t>(view->components.b), static_cast<std::uint32_t>(view->components.a)};
            record.aspect = view->range.aspectMask;
            record.baseLevel = view->range.baseMipLevel;
            record.levels = view->range.levelCount == VK_REMAINING_MIP_LEVELS && info ? info->levels - view->range.baseMipLevel : view->range.levelCount;
            record.baseLayer = view->range.baseArrayLayer;
            record.layers = view->range.layerCount == VK_REMAINING_ARRAY_LAYERS && info ? info->layers - view->range.baseArrayLayer : view->range.layerCount;
            record.minLod = view->minLod;
            record.usage = view->usage;
        } else {
            manifest.notes.push_back("the view of binding " + std::to_string(bound.binding) + " element " + std::to_string(bound.element) + " was not created through the capture registry");
        }
        manifest.views.push_back(record);
    }
    for (const auto& bound : bindings.samplers) {
        const auto& info = bound.info;
        manifest.samplers.push_back({bound.binding, bound.element, static_cast<std::uint32_t>(info.magFilter), static_cast<std::uint32_t>(info.minFilter), static_cast<std::uint32_t>(info.mipmapMode), static_cast<std::uint32_t>(info.addressModeU), static_cast<std::uint32_t>(info.addressModeV), static_cast<std::uint32_t>(info.addressModeW), info.mipLodBias, info.anisotropyEnable, info.maxAnisotropy, info.compareEnable, static_cast<std::uint32_t>(info.compareOp), info.minLod, info.maxLod, static_cast<std::uint32_t>(info.borderColor), info.unnormalizedCoordinates});
    }
    before = true;
}

void DispatchRecorder::After() {
    Require(before && !after, "dispatch capture outputs need recorded inputs and are recorded once");
    auto writes = bindings.writes;
    std::sort(writes.begin(), writes.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    for (const auto& range : writes) {
        if (range.first >= range.second) continue;
        if (!merged.empty() && range.first <= merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
        else merged.push_back(range);
    }
    for (const auto& [begin, end] : merged) {
        const auto* region = regionOf(begin, end);
        if (region == nullptr) {
            manifest.notes.push_back("written range 0x" + hex(begin) + "-0x" + hex(end) + " lies outside every captured region");
            continue;
        }
        const auto offset = static_cast<std::size_t>(region->padding + (begin - region->begin));
        resolveGuest(*region, begin, end);
        manifest.writes.push_back({begin, end, store(region->bytes.subspan(offset, static_cast<std::size_t>(end - begin)))});
    }
    for (std::size_t index = 0; index < bindings.buffers.size(); ++index) {
        const auto& bound = bindings.buffers[index];
        if (bound.source == CaptureBufferSource::Gds || bound.source == CaptureBufferSource::Fault) manifest.buffers[index].after = store(bound.bytes);
    }
    for (std::size_t index = 0; index < plans.size(); ++index) {
        const auto& plan = plans[index];
        auto& image = manifest.images[index];
        if (!plan.known || plan.after.empty() || (image.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) continue;
        try {
            const auto bytes = copyImage(plan, image, plan.after, image.written);
            if (!bytes.empty()) image.after = store(bytes);
        } catch (const std::exception& error) {
            image.written.clear();
            manifest.notes.push_back("image " + std::to_string(index) + " results were not captured: " + error.what());
        }
    }
    after = true;
}

std::filesystem::path DispatchRecorder::Finish() {
    Require(before, "dispatch capture has no recorded inputs");
    if (!after) manifest.notes.push_back("the dispatch results were not captured");
    {
        std::ofstream file(directory / CaptureSpirvName, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(shader.spirv.data()), static_cast<std::streamsize>(shader.spirv.size() * sizeof(std::uint32_t)));
        if (!file) throw std::runtime_error("cannot write the captured SPIR-V into " + directory.string());
    }
    if (!target.request.empty()) {
        std::ofstream file(directory / CaptureRequestName, std::ios::binary | std::ios::trunc);
        file << CaptureRequestHeader << '\n' << target.request << '\n';
        if (!file) throw std::runtime_error("cannot write the captured request into " + directory.string());
    }
    WriteCaptureManifest(directory, manifest);
    return directory;
}

std::string DispatchRecorder::Summary() const {
    std::uint64_t regionBytes = 0;
    for (const auto& region : bindings.regions) regionBytes += region.bytes.size();
    char text[256];
    std::snprintf(text, sizeof(text), "%zu regions (%.1f MiB), %zu written ranges, %zu buffers, %zu images, %zu samplers, %.1f MiB stored, %zu notes", manifest.regions.size(), regionBytes / 1048576.0, manifest.writes.size(), manifest.buffers.size(), manifest.images.size(), manifest.samplers.size(), storedBytes / 1048576.0, manifest.notes.size());
    return text;
}

}

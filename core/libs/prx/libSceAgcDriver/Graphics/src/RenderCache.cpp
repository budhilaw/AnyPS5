#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <string>
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/CallerSymbol.hpp"
#include "prx/libc/include/SlowOperation.hpp"
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <string>

namespace AgcDriver::Graphics {

void ResidentColor::Transition(VkCommandBuffer commands, VkImageLayout next) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0u : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.oldLayout = layout;
    barrier.newLayout = next;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = Target().Image();
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    layout = next;
}

void ResidentColor::Begin(VkCommandBuffer commands) {
    PerformanceTimer timing("Graphics.ResidentColor.Begin");
    Require(generation != std::numeric_limits<std::uint64_t>::max(), "resident color generation overflow");
    ++generation;
    if (color.gpuOnly) {
        Transition(commands, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        valid = true;
        timing.Mark("gpu_only");
        return;
    }
    if (memoryWatch == nullptr) {
        char message[160];
        std::snprintf(message, sizeof(message), "render target 0x%llx (%ux%u) memory ownership was released before its use", static_cast<unsigned long long>(color.address), color.extent.width, color.extent.height);
        throw std::runtime_error(message);
    }
    if (!valid) {
        const GuestMemory::MemoryAccessScope suspended(nullptr, nullptr);
        if (transfer.UploadView(color.address, color.extent.width, color.extent.height, color.tileMode)) {
            adoptedThrough = context.drawQueue != nullptr ? context.drawQueue->NextSequence() : 0;
        } else {
            memoryWatch->Protect(GuestMemoryTracking::Protection::Read);
            transfer.Upload(color.address, color.extent.width, color.extent.height, color.tileMode);
        }
        transfer.Detile(commands);
        retainUploadSource();
        Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {color.extent.width, color.extent.height, 1};
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, transfer.LinearBuffer(), Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        timing.Mark("upload", color.bytes);
    } else {
        timing.Mark("reuse");
    }
    Transition(commands, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    valid = true;
    dirty = true;
    memoryWatch->Protect(GuestMemoryTracking::Protection::None);
}

void ResidentColor::retainUploadSource() {
    if (context.drawQueue != nullptr && transfer.UploadSource()) context.drawQueue->EnqueueCompletion([source = transfer.UploadSource()] {});
}

bool ResidentColor::Refresh(VkCommandBuffer commands) {
    if (color.gpuOnly || valid || memoryWatch == nullptr) return false;
    PerformanceTimer timing("Graphics.ResidentColor.Refresh");
    {
        const GuestMemory::MemoryAccessScope suspended(nullptr, nullptr);
        if (!transfer.UploadView(color.address, color.extent.width, color.extent.height, color.tileMode)) return false;
    }
    Require(generation != std::numeric_limits<std::uint64_t>::max(), "resident color generation overflow");
    ++generation;
    adoptedThrough = context.drawQueue != nullptr ? context.drawQueue->NextSequence() : 0;
    transfer.Detile(commands);
    retainUploadSource();
    Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {color.extent.width, color.extent.height, 1};
    context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, transfer.LinearBuffer(), Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    valid = true;
    dirty = false;
    memoryWatch->Protect(GuestMemoryTracking::Protection::Read);
    timing.Mark("record", color.bytes);
    return true;
}

void ResidentColor::Adopt(VkCommandBuffer commands, VkImage image) {
    Require(generation != std::numeric_limits<std::uint64_t>::max(), "resident color generation overflow");
    ++generation;
    if (!color.gpuOnly) {
        Require(memoryWatch != nullptr, "render target memory ownership was released");
        memoryWatch->Protect(GuestMemoryTracking::Protection::None);
        transfer.Prepare(color.extent.width, color.extent.height, color.tileMode);
    }
    Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageCopy copy{};
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.dstSubresource = copy.srcSubresource;
    copy.extent = {color.extent.width, color.extent.height, 1};
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    valid = true;
    dirty = true;
}

void ResidentColor::DebugClear(VkCommandBuffer commands, float r, float g, float b) {
    Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkClearColorValue value{};
    value.float32[0] = r; value.float32[1] = g; value.float32[2] = b; value.float32[3] = 1.0f;
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &value, 1, &range);
    valid = true;
    dirty = true;
    ++generation;
}

void ResidentColor::Download(VkCommandBuffer commands) {
    if (!dirty || color.gpuOnly) return;
    Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkMemoryBarrier reuse{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    reuse.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    reuse.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &reuse, 0, nullptr, 0, nullptr);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {color.extent.width, color.extent.height, 1};
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transfer.LinearBuffer(), 1, &copy);
    transfer.Tile(commands);
}

void ResidentColor::Commit() {
    if (!dirty || color.gpuOnly) return;
    const GuestMemory::MemoryAccessScope suspended(nullptr, nullptr);
    transfer.WriteBackTracked(color.address);
    dirty = false;
    memoryWatch->Protect(GuestMemoryTracking::Protection::Read);
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> RenderCache::UnadoptedRanges(std::uint64_t begin, std::uint64_t end, std::uint64_t sequence) const {
    return excluding(begin, end, [sequence](const ResidentColor& entry) { return entry.Adopted(sequence); });
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> RenderCache::UnwatchedRanges(std::uint64_t begin, std::uint64_t end) const {
    return excluding(begin, end, [](const ResidentColor& entry) { return entry.Watched(); });
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> RenderCache::WatchedRanges(std::uint64_t begin, std::uint64_t end, std::uint64_t sequence) const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
    for (auto it = entries.lower_bound(reachStart(begin)); it != entries.end(); ++it) {
        const auto& [address, entry] = *it;
        if (address >= end + pageSize) break;
        if (!entry->Watched() || entry->Adopted(sequence)) continue;
        const auto& color = entry->Description();
        const auto first = std::max(begin, color.address - color.address % pageSize);
        const auto last = std::min(end, (color.address + color.bytes + pageSize - 1) / pageSize * pageSize);
        if (first < last) result.emplace_back(first, last);
    }
    return result;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> RenderCache::excluding(std::uint64_t begin, std::uint64_t end, const std::function<bool(const ResidentColor&)>& excluded) const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> result{{begin, end}};
    for (auto it = entries.lower_bound(reachStart(begin)); it != entries.end(); ++it) {
        const auto& [address, entry] = *it;
        if (address >= end + pageSize) break;
        if (!excluded(*entry)) continue;
        const auto& color = entry->Description();
        const auto first = color.address - color.address % pageSize;
        const auto last = (color.address + color.bytes + pageSize - 1) / pageSize * pageSize;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> remaining;
        for (const auto& [from, to] : result) {
            if (to <= first || last <= from) {
                remaining.emplace_back(from, to);
                continue;
            }
            if (from < first) remaining.emplace_back(from, first);
            if (last < to) remaining.emplace_back(last, to);
        }
        result = std::move(remaining);
        if (result.empty()) break;
    }
    return result;
}

void RenderCache::retire(std::shared_ptr<ResidentColor> entry) {
    if (context.drawQueue) context.drawQueue->EnqueueCompletion([entry = std::move(entry)] {});
}

RenderCache::Entries::iterator RenderCache::release(Entries::iterator entry) {
    const auto bytes = entry->second->Description().bytes;
    entry->second->ReleaseMemory();
    retire(entry->second);
    const auto next = entries.erase(entry);
    if (bytes >= largestTarget) {
        largestTarget = 0;
        for (const auto& [address, remaining] : entries) largestTarget = std::max<std::uint64_t>(largestTarget, remaining->Description().bytes);
    }
    epoch.fetch_add(1, std::memory_order_release);
    return next;
}

std::shared_ptr<ResidentColor> RenderCache::Get(const ColorTarget& color, bool blending) {
    PerformanceTimer timing("Graphics.RenderCache.Get");
    Require(color.address != 0 && color.bytes != 0 && color.bytes <= std::numeric_limits<std::uint64_t>::max() - color.address, "invalid resident color range");
    if (!color.gpuOnly && context.drawQueue && context.drawQueue->WritesPending(color.address, color.bytes)) {
        const bool ordered = context.guestBufferCache != nullptr && context.guestBufferCache->HostImportable(color.address, color.bytes);
        context.drawQueue->Resolve(color.address, color.bytes, ordered);
        timing.Mark("pending_writes");
    }
    if (blending) {
        VkFormatProperties properties{};
        context.formatProperties(context.physical, color.format, &properties);
        Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) != 0, "render target format does not support blending");
    }
    const auto lastPage = (color.address + color.bytes - 1) / pageSize;
    for (auto it = entries.lower_bound(reachStart(color.address)); it != entries.end() && it->first / pageSize <= lastPage;) {
        const auto& previous = it->second->Description();
        if (!it->second->SharesPages(color, pageSize)) {
            ++it;
            continue;
        }
        if (previous.address == color.address && previous.bytes == color.bytes && previous.extent.width == color.extent.width && previous.extent.height == color.extent.height && previous.format == color.format && previous.tileMode == color.tileMode) {
            it->second->lastUse = ++useCounter;
            timing.Mark("hit");
            return it->second;
        }
        static const bool traceTargets = std::getenv("ANYPS5_TRACE_TARGETS") != nullptr;
        if (traceTargets) APS5_LOG_OUT("resident target 0x%llx (%ux%u, %zu bytes) released for 0x%llx (%ux%u, %zu bytes) sharing its pages", static_cast<unsigned long long>(previous.address), previous.extent.width, previous.extent.height, previous.bytes, static_cast<unsigned long long>(color.address), color.extent.width, color.extent.height, color.bytes);
        Resolve(previous.address, previous.bytes, true);
        it = release(it);
        timing.Mark("shared_pages");
    }
    if (entries.size() >= 160) {
        SlowOperationTimer full("render target cache full");
        Flush();
        std::vector<std::map<std::uint64_t, std::shared_ptr<ResidentColor>>::iterator> candidates;
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (it->second.use_count() == 1 && !it->second->Dirty()) candidates.push_back(it);
        }
        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a->second->lastUse < b->second->lastUse; });
        static const bool traceEviction = std::getenv("ANYPS5_TRACE_TARGETS") != nullptr;
        for (const auto& it : candidates) {
            if (entries.size() <= 120) break;
            if (traceEviction) APS5_LOG_OUT("resident target 0x%llx (%ux%u) evicted (least recently used, %zu entries)", static_cast<unsigned long long>(it->first), it->second->Description().extent.width, it->second->Description().extent.height, entries.size());
            release(it);
            SlowOperationRecord_nid_no_patch("render target evict", 0);
        }
        timing.Mark("evict");
    }
    auto entry = std::make_shared<ResidentColor>(context, color);
    entry->lastUse = ++useCounter;
    entries.emplace(color.address, entry);
    largestTarget = std::max<std::uint64_t>(largestTarget, color.bytes);
    epoch.fetch_add(1, std::memory_order_release);
    timing.Mark("create");
    return entry;
}

std::shared_ptr<DepthImage> RenderCache::GetDepth(const DepthTarget& depth) {
    Require(depth.address != 0 && depth.extent.width != 0 && depth.extent.height != 0, "invalid depth target");
    const auto it = depthEntries.find(depth.address);
    if (it != depthEntries.end()) {
        const auto& previous = it->second.target;
        const bool sameExtent = previous.extent.width == depth.extent.width && previous.extent.height == depth.extent.height;
        if (sameExtent && (previous.format == depth.format || (previous.stencil && !depth.stencil))) return it->second.image;
        if (static int reported = 0; reported++ < 40) APS5_LOG_OUT("depth target 0x%llx recreated: %ux%u format %u stencil 0x%llx -> %ux%u format %u stencil 0x%llx", static_cast<unsigned long long>(depth.address), previous.extent.width, previous.extent.height, static_cast<unsigned>(previous.format), static_cast<unsigned long long>(previous.stencilAddress), depth.extent.width, depth.extent.height, static_cast<unsigned>(depth.format), static_cast<unsigned long long>(depth.stencilAddress));
        if (context.drawQueue) context.drawQueue->Wait();
        depthEntries.erase(it);
    }
    if (depthEntries.size() >= 32) {
        if (static int reported = 0; reported++ < 10) APS5_LOG_CHARS_OUT("depth target cache is full: dropping every depth target");
        if (context.drawQueue) context.drawQueue->Wait();
        depthEntries.clear();
    }
    auto image = std::make_shared<DepthImage>(context, depth);
    depthEntries.emplace(depth.address, DepthEntry{depth, image});
    return image;
}

std::shared_ptr<DepthImage> RenderCache::FindDepth(std::uint64_t address, bool* stencil) const {
    if (stencil) *stencil = false;
    const auto it = depthEntries.find(address);
    if (it != depthEntries.end()) return it->second.image;
    for (const auto& [zAddress, entry] : depthEntries) {
        if (entry.target.stencil && entry.target.stencilAddress == address) {
            if (stencil) *stencil = true;
            return entry.image;
        }
    }
    return nullptr;
}

std::string RenderCache::DescribeColorTargets() const {
    std::string result;
    for (const auto& [address, entry] : entries) {
        const auto& color = entry->Description();
        char text[128];
        std::snprintf(text, sizeof(text), " 0x%llx(%ux%u fmt %u tile %u bpp %u%s%s)", static_cast<unsigned long long>(address), color.extent.width, color.extent.height, static_cast<unsigned>(color.format), static_cast<unsigned>(color.tileMode), color.bytesPerPixel, color.gpuOnly ? " gpuonly" : "", entry->Valid() ? "" : " invalid");
        result += text;
    }
    return result.empty() ? " none" : result;
}

namespace {

float HalfToFloat(std::uint16_t half) {
    const auto sign = (half >> 15u) & 1u;
    const auto exponent = (half >> 10u) & 0x1fu;
    const auto mantissa = half & 0x3ffu;
    float value;
    if (exponent == 0) value = std::ldexp(static_cast<float>(mantissa), -24);
    else if (exponent == 31) value = mantissa ? 0.0f : 1e30f;
    else value = std::ldexp(static_cast<float>(mantissa | 0x400u), static_cast<int>(exponent) - 25);
    return sign ? -value : value;
}

unsigned char ToByte(float value) {
    value = value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
    return static_cast<unsigned char>(value * 255.0f + 0.5f);
}

}

void RenderCache::DumpTargets(const std::string& prefix, std::uint64_t only) {
    if (context.drawQueue) context.drawQueue->Wait();
    for (const auto& [address, entry] : entries) {
        if (!entry->Valid() || (only != 0 && address != only)) continue;
        const auto& color = entry->Description();
        const auto texel = color.bytesPerPixel;
        if (texel != 1 && texel != 4 && texel != 8) continue;
        const std::size_t bytes = static_cast<std::size_t>(color.extent.width) * color.extent.height * texel;
        Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        {
            CommandBatch batch(context, "readback");
            const auto commands = batch.Handle();
            const auto previous = entry->Layout();
            entry->Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {color.extent.width, color.extent.height, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, entry->Target().Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.Handle(), 1, &copy);
            if (previous != VK_IMAGE_LAYOUT_UNDEFINED) entry->Transition(commands, previous);
            batch.SubmitAndWait();
        }
        readback.Invalidate();
        const auto* data = reinterpret_cast<const unsigned char*>(readback.Bytes().data());
        char name[64];
        std::snprintf(name, sizeof(name), "%llx_%ux%u_f%u.bmp", static_cast<unsigned long long>(address), color.extent.width, color.extent.height, static_cast<unsigned>(color.format));
        auto* file = std::fopen((prefix + name).c_str(), "wb");
        if (file == nullptr) continue;
        const std::uint32_t width = color.extent.width, height = color.extent.height;
        const std::uint32_t rowBytes = width * 4, imageBytes = rowBytes * height;
        unsigned char header[54] = {'B', 'M'};
        const auto put32 = [&](int at, std::uint32_t v) { header[at] = v & 0xff; header[at + 1] = (v >> 8) & 0xff; header[at + 2] = (v >> 16) & 0xff; header[at + 3] = (v >> 24) & 0xff; };
        put32(2, 54 + imageBytes); put32(10, 54); put32(14, 40); put32(18, width); put32(22, static_cast<std::uint32_t>(-static_cast<std::int32_t>(height))); header[26] = 1; header[28] = 32; put32(34, imageBytes);
        std::fwrite(header, 1, 54, file);
        std::vector<unsigned char> row(rowBytes);
        const bool bgra = color.format == VK_FORMAT_B8G8R8A8_UNORM || color.format == VK_FORMAT_B8G8R8A8_SRGB;
        const bool hdr = texel == 8 || color.format == VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        const auto hdrTexel = [&](const unsigned char* texelData, float rgb[3]) {
            if (texel == 8) {
                std::uint16_t h[4];
                std::memcpy(h, texelData, 8);
                for (int c = 0; c < 3; ++c) rgb[c] = HalfToFloat(h[c]);
            } else {
                std::uint32_t v;
                std::memcpy(&v, texelData, 4);
                rgb[0] = HalfToFloat(static_cast<std::uint16_t>((v & 0x7ff) << 4));
                rgb[1] = HalfToFloat(static_cast<std::uint16_t>(((v >> 11) & 0x7ff) << 4));
                rgb[2] = HalfToFloat(static_cast<std::uint16_t>(((v >> 22) & 0x3ff) << 5));
            }
        };
        float hdrScale = 1.0f;
        if (hdr) {
            double sums[3] = {0, 0, 0}, peak = 0;
            std::size_t count = 0;
            for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; i += 7) {
                float rgb[3];
                hdrTexel(data + i * texel, rgb);
                if (!std::isfinite(rgb[0]) || !std::isfinite(rgb[1]) || !std::isfinite(rgb[2])) continue;
                for (int c = 0; c < 3; ++c) sums[c] += rgb[c];
                peak += std::max({rgb[0], rgb[1], rgb[2]});
                ++count;
            }
            if (count != 0 && peak > 0) hdrScale = static_cast<float>(0.3 * count / peak);
            APS5_LOG_OUT("hdr target 0x%llx %ux%u format %u mean %g %g %g, dumped at x%g", static_cast<unsigned long long>(address), width, height, static_cast<unsigned>(color.format), count ? sums[0] / count : 0.0, count ? sums[1] / count : 0.0, count ? sums[2] / count : 0.0, hdrScale);
        }
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                const auto* texelData = data + (static_cast<std::size_t>(y) * width + x) * texel;
                unsigned char r, g, b;
                if (hdr) {
                    float rgb[3];
                    hdrTexel(texelData, rgb);
                    r = ToByte(rgb[0] * hdrScale); g = ToByte(rgb[1] * hdrScale); b = ToByte(rgb[2] * hdrScale);
                } else if (texel == 1) {
                    r = g = b = texelData[0];
                } else if (texel == 8) {
                    std::uint16_t h[4];
                    std::memcpy(h, texelData, 8);
                    r = ToByte(HalfToFloat(h[0])); g = ToByte(HalfToFloat(h[1])); b = ToByte(HalfToFloat(h[2]));
                } else if (color.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || color.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32) {
                    std::uint32_t v;
                    std::memcpy(&v, texelData, 4);
                    r = static_cast<unsigned char>((v & 0x3ff) >> 2); g = static_cast<unsigned char>(((v >> 10) & 0x3ff) >> 2); b = static_cast<unsigned char>(((v >> 20) & 0x3ff) >> 2);
                    if (color.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32) std::swap(r, b);
                } else if (color.format == VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
                    std::uint32_t v;
                    std::memcpy(&v, texelData, 4);
                    r = ToByte(HalfToFloat(static_cast<std::uint16_t>((v & 0x7ff) << 4))); g = ToByte(HalfToFloat(static_cast<std::uint16_t>(((v >> 11) & 0x7ff) << 4))); b = ToByte(HalfToFloat(static_cast<std::uint16_t>(((v >> 22) & 0x3ff) << 5)));
                } else {
                    r = texelData[bgra ? 2 : 0]; g = texelData[1]; b = texelData[bgra ? 0 : 2];
                }
                row[x * 4] = b; row[x * 4 + 1] = g; row[x * 4 + 2] = r; row[x * 4 + 3] = 255;
            }
            std::fwrite(row.data(), 1, rowBytes, file);
        }
        std::fclose(file);
        if (texel == 1) {
            std::array<std::size_t, 256> histogram{};
            for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i) ++histogram[data[i]];
            std::string summary;
            for (std::uint32_t value = 0; value < 256; ++value) {
                if (histogram[value] == 0) continue;
                char item[32];
                std::snprintf(item, sizeof(item), " %02x:%zu", value, histogram[value]);
                summary += item;
            }
            APS5_LOG_OUT("8-bit target 0x%llx %ux%u values:%s", static_cast<unsigned long long>(address), width, height, summary.c_str());
        }
    }
}

std::string RenderCache::DescribeStencil(DepthImage& image) {
    const auto& target = image.Description();
    if (target.format != VK_FORMAT_D32_SFLOAT_S8_UINT) return " (no stencil)";
    if (context.drawQueue) context.drawQueue->Wait();
    const std::size_t bytes = static_cast<std::size_t>(target.extent.width) * target.extent.height;
    Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    {
        CommandBatch batch(context, "readback");
        const auto commands = batch.Handle();
        const bool attached = image.Attached();
        image.Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1};
        copy.imageExtent = {target.extent.width, target.extent.height, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image.Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.Handle(), 1, &copy);
        if (attached) image.Transition(commands, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        batch.SubmitAndWait();
    }
    readback.Invalidate();
    const auto* stencil = reinterpret_cast<const unsigned char*>(readback.Bytes().data());
    std::array<std::size_t, 256> histogram{};
    for (std::size_t i = 0; i < bytes; ++i) ++histogram[stencil[i]];
    std::string summary;
    for (std::uint32_t value = 0; value < 256; ++value) {
        if (histogram[value] == 0) continue;
        char item[32];
        std::snprintf(item, sizeof(item), " %02x:%zu", value, histogram[value]);
        summary += item;
    }
    return summary;
}

void RenderCache::DumpDepthTargets(const std::string& prefix) {
    if (context.drawQueue) context.drawQueue->Wait();
    for (const auto& [address, entry] : depthEntries) {
        const auto& target = entry.target;
        if (target.format != VK_FORMAT_D32_SFLOAT && target.format != VK_FORMAT_D32_SFLOAT_S8_UINT) continue;
        const std::size_t bytes = static_cast<std::size_t>(target.extent.width) * target.extent.height * 4;
        Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        {
            CommandBatch batch(context, "readback");
            const auto commands = batch.Handle();
            entry.image->Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
            copy.imageExtent = {target.extent.width, target.extent.height, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, entry.image->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.Handle(), 1, &copy);
            entry.image->Transition(commands, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            batch.SubmitAndWait();
        }
        readback.Invalidate();
        const auto* values = reinterpret_cast<const float*>(readback.Bytes().data());
        float minimum = 1e30f, maximum = -1e30f;
        std::size_t count = static_cast<std::size_t>(target.extent.width) * target.extent.height, sampled = 0, nan = 0, zero = 0;
        for (std::size_t i = 0; i < count; i += 97) {
            ++sampled;
            if (std::isnan(values[i])) { ++nan; continue; }
            if (values[i] == 0.0f) ++zero;
            minimum = std::min(minimum, values[i]); maximum = std::max(maximum, values[i]);
        }
        std::fprintf(stderr, "depth target 0x%llx %ux%u: values %g .. %g (%zu sampled, %zu NaN, %zu zero)\n", static_cast<unsigned long long>(address), target.extent.width, target.extent.height, minimum, maximum, sampled, nan, zero);
        if (target.format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
            const std::size_t stencilBytes = static_cast<std::size_t>(target.extent.width) * target.extent.height;
            Buffer stencilReadback(context, stencilBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
            {
                CommandBatch batch(context, "readback");
                const auto commands = batch.Handle();
                entry.image->Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                VkBufferImageCopy copy{};
                copy.imageSubresource = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1};
                copy.imageExtent = {target.extent.width, target.extent.height, 1};
                context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, entry.image->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stencilReadback.Handle(), 1, &copy);
                entry.image->Transition(commands, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                batch.SubmitAndWait();
            }
            stencilReadback.Invalidate();
            const auto* stencil = reinterpret_cast<const unsigned char*>(stencilReadback.Bytes().data());
            std::array<std::size_t, 256> histogram{};
            for (std::size_t i = 0; i < stencilBytes; ++i) ++histogram[stencil[i]];
            std::string summary;
            for (std::uint32_t value = 0; value < 256; ++value) {
                if (histogram[value] == 0) continue;
                char item[32];
                std::snprintf(item, sizeof(item), " %02x:%zu", value, histogram[value]);
                summary += item;
            }
            std::fprintf(stderr, "depth target 0x%llx (stencil plane 0x%llx) stencil values:%s\n", static_cast<unsigned long long>(address), static_cast<unsigned long long>(target.stencilAddress), summary.c_str());
        }
        std::fflush(stderr);
        char name[64];
        std::snprintf(name, sizeof(name), "depth_%llx_%ux%u.bmp", static_cast<unsigned long long>(address), target.extent.width, target.extent.height);
        auto* file = std::fopen((prefix + name).c_str(), "wb");
        if (file == nullptr) continue;
        const std::uint32_t width = target.extent.width, height = target.extent.height, rowBytes = width * 4, imageBytes = rowBytes * height;
        unsigned char header[54] = {'B', 'M'};
        const auto put32 = [&](int at, std::uint32_t v) { header[at] = v & 0xff; header[at + 1] = (v >> 8) & 0xff; header[at + 2] = (v >> 16) & 0xff; header[at + 3] = (v >> 24) & 0xff; };
        put32(2, 54 + imageBytes); put32(10, 54); put32(14, 40); put32(18, width); put32(22, static_cast<std::uint32_t>(-static_cast<std::int32_t>(height))); header[26] = 1; header[28] = 32; put32(34, imageBytes);
        std::fwrite(header, 1, 54, file);
        std::vector<unsigned char> row(rowBytes);
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                const auto v = ToByte(values[static_cast<std::size_t>(y) * width + x]);
                row[x * 4] = v; row[x * 4 + 1] = v; row[x * 4 + 2] = v; row[x * 4 + 3] = 255;
            }
            std::fwrite(row.data(), 1, rowBytes, file);
        }
        std::fclose(file);
    }
}

std::string RenderCache::DescribeDepthTargets() const {
    std::string result;
    for (const auto& [address, entry] : depthEntries) {
        char text[96];
        std::snprintf(text, sizeof(text), " 0x%llx(%ux%u fmt %u stencil 0x%llx)", static_cast<unsigned long long>(address), entry.target.extent.width, entry.target.extent.height, static_cast<unsigned>(entry.target.format), static_cast<unsigned long long>(entry.target.stencilAddress));
        result += text;
    }
    return result.empty() ? " none" : result;
}

std::shared_ptr<ResidentColor> RenderCache::Find(std::uint64_t address) const {
    const auto it = entries.find(address);
    if (it == entries.end()) return nullptr;
    const auto& entry = it->second;
    const auto bytes = entry->Description().bytes;
    if (!entry->Description().gpuOnly && context.drawQueue && context.drawQueue->WritesPending(address, bytes, entry->AdoptedThrough())) {
        const bool reloaded = !entry->Valid() && !context.drawQueue->WritesPending(address, bytes, context.drawQueue->NextSequence()) && entry->Refresh(context.drawQueue->BeginBarrier(context));
        if (!reloaded) {
            static const bool trace = std::getenv("ANYPS5_TRACE_WAITS") != nullptr;
            static int reported = 0;
            if (trace && reported++ < 20) std::fprintf(stderr, "[find-wait] 0x%llx valid %d dirty %d gpuOnly %d tile %u adopted %llu next %llu\n", static_cast<unsigned long long>(address), entry->Valid() ? 1 : 0, entry->Dirty() ? 1 : 0, entry->Description().gpuOnly ? 1 : 0, static_cast<unsigned>(entry->Description().tileMode), static_cast<unsigned long long>(entry->AdoptedThrough()), static_cast<unsigned long long>(context.drawQueue->NextSequence()));
            if (trace) std::fflush(stderr);
            context.drawQueue->Resolve(address, bytes);
        }
    }
    return entry->Valid() ? entry : nullptr;
}

void RenderCache::Resolve(std::uint64_t address, std::size_t bytes, bool writable) {
    std::vector<std::shared_ptr<ResidentColor>> affected;
    const auto end = address + bytes;
    for (auto it = entries.lower_bound(reachStart(address)); it != entries.end() && it->first < end; ++it) {
        const auto& [base, entry] = *it;
        const auto& color = entry->Description();
        if (address >= base + color.bytes) continue;
        if (color.gpuOnly) {
            if (writable) entry->Invalidate();
            continue;
        }
        if (entry->Dirty()) affected.push_back(entry);
        else if (writable) entry->Invalidate();
    }
    if (affected.empty()) return;
    {
        static const char* traceValue = std::getenv("ANYPS5_TRACE_RESOLVE");
        static const auto traceStart = std::chrono::steady_clock::now();
        static int reported = 0;
        if (traceValue != nullptr && reported < 60 && std::chrono::duration<double>(std::chrono::steady_clock::now() - traceStart).count() >= std::atof(traceValue)) {
            ++reported;
            void* frames[8];
            const int count = CaptureCallers(frames, 8);
            std::string chain;
            for (int i = 1; i < count; ++i) chain += " <- " + DescribeCaller(frames[i]);
            APS5_LOG_OUT("resolve 0x%llx+0x%zx %s: %zu dirty target(s), first 0x%llx%s", static_cast<unsigned long long>(address), bytes, writable ? "write" : "read", affected.size(), static_cast<unsigned long long>(affected.front()->Description().address), chain.c_str());
        }
    }
    PerformanceTimer timing("Graphics.RenderCache.Resolve");
    if (context.drawQueue) context.drawQueue->Wait();
    CommandBatch batch(context, "resolve");
    for (const auto& entry : affected) entry->Download(batch.Handle());
    batch.SubmitAndWait();
    timing.Mark("download_wait");
    for (const auto& entry : affected) {
        entry->Commit();
        if (writable) entry->Invalidate();
    }
    timing.Mark("guest_writeback");
}

void RenderCache::DiscardCovered(std::uint64_t address, std::size_t bytes) {
    const auto end = address + bytes;
    for (auto it = entries.lower_bound(address); it != entries.end() && it->first < end; ++it) {
        const auto& [base, entry] = *it;
        const auto& color = entry->Description();
        if (base + color.bytes <= end && base + color.bytes > base) entry->Discard();
    }
}

void RenderCache::Flush() {
    Resolve(0, std::numeric_limits<std::size_t>::max(), true);
}

}

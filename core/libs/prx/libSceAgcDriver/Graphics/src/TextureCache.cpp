#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <mutex>

namespace AgcDriver::Graphics {

TextureCache::TextureCache(const Context& context) : context(context), budget(4096ull << 20) {
    Require(context.detiler != nullptr, "texture cache requires a device detiler");
    if (const char* value = std::getenv("ANYPS5_TEXTURE_CACHE_MB")) {
        const auto megabytes = std::strtoull(value, nullptr, 10);
        if (megabytes != 0) budget = megabytes << 20;
    }
}

void TextureCache::trim() {
    for (auto it = entries.begin(); it != entries.end() && (retainedBytes > budget || entries.size() > maxEntries);) {
        if (it->texture.use_count() != 1) {
            ++it;
            continue;
        }
        it = eraseEntry(it);
    }
}

std::list<TextureCache::Entry>::iterator TextureCache::eraseEntry(std::list<Entry>::iterator it) {
    retainedBytes -= it->retained;
    auto [first, last] = index.equal_range(descriptorHash(it->descriptor));
    for (; first != last; ++first) {
        if (first->second == it) {
            index.erase(first);
            break;
        }
    }
    return entries.erase(it);
}

std::uint64_t TextureCache::descriptorHash(const std::array<std::uint32_t, 8>& descriptor) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const auto word : descriptor) hash = (hash ^ word) * 1099511628211ull;
    return hash;
}

void TextureCache::addEntry(Entry entry) {
    const auto hash = descriptorHash(entry.descriptor);
    entries.push_back(std::move(entry));
    index.emplace(hash, std::prev(entries.end()));
}

std::list<TextureCache::Entry>::iterator TextureCache::findEntry(const std::array<std::uint32_t, 8>& descriptor, TextureDimension viewDimension, bool compare, bool depthView) {
    if (++lookupsSinceSweep >= 1024) {
        lookupsSinceSweep = 0;
        for (auto it = entries.begin(); it != entries.end();) it = (it->surface && it->surface->stale) || !depthResident(*it) ? eraseEntry(it) : std::next(it);
    }
    auto [first, last] = index.equal_range(descriptorHash(descriptor));
    while (first != last) {
        const auto it = first->second;
        ++first;
        if (it->surface && it->surface->stale) {
            eraseEntry(it);
            continue;
        }
        if (it->descriptor == descriptor && it->viewDimension == viewDimension && it->compare == compare && it->depthView == depthView) return it;
    }
    return entries.end();
}

bool TextureCache::depthResident(const Entry& entry) const {
    if (context.renderCache == nullptr) return true;
    return std::all_of(entry.depthSources.begin(), entry.depthSources.end(), [&](const std::weak_ptr<DepthImage>& source) {
        const auto image = source.lock();
        return image != nullptr && context.renderCache->FindDepth(image->Description().address) == image;
    });
}

std::shared_ptr<Texture> TextureCache::directDepth(const std::array<std::uint32_t, 8>& key, const std::shared_ptr<DepthImage>& image, bool stencil, const GuestTextureResource& resource, VkComponentMapping components, bool compare, PerformanceTimer& timing) {
    const auto& target = image->Description();
    const bool knownFormat = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D32_SFLOAT || target.format == VK_FORMAT_D32_SFLOAT_S8_UINT;
    const auto aspectBytes = stencil ? 1u : target.format == VK_FORMAT_D16_UNORM ? 2u : 4u;
    const bool singleSlice = resource.dimension == TextureDimension::k2D || (resource.dimension == TextureDimension::k2DArray && resource.depthOrLastArray == 0);
    const bool planarView = resource.viewDimension == TextureDimension::k2D || resource.viewDimension == TextureDimension::k2DArray;
    if (context.drawQueue == nullptr || !image->Sampled() || (!stencil && !image->Filterable()) || (stencil && !target.stencil) || !knownFormat) return nullptr;
    if (!singleSlice || !planarView || resource.mipCount != 1 || resource.baseLevel != 0 || resource.baseArray != 0 || resource.width != target.extent.width || resource.height != target.extent.height) return nullptr;
    if (IsBlockCompressed(resource.format) || BytesPerElement(resource.format) != aspectBytes) return nullptr;
    const auto view = image->SampledView(stencil, resource.viewDimension == TextureDimension::k2DArray, components);
    if (view == VK_NULL_HANDLE) return nullptr;
    if (!image->ReadOnly()) image->Transition(context.drawQueue->BeginBarrier(context), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    const bool depthCompare = compare && !stencil;
    if (const auto it = findEntry(key, resource.viewDimension, depthCompare, true); it != entries.end()) {
        if (it->depthSources.size() == 1 && it->depthSources.front().lock() == image) {
            auto result = it->texture;
            entries.splice(entries.end(), entries, it);
            timing.Mark("depth_view_hit");
            return result;
        }
        eraseEntry(it);
    }
    auto texture = std::make_shared<Texture>(context, image, view, stencil, resource, Texture::DirectDepthView{});
    addEntry({key, resource.viewDimension, texture, nullptr, {}, {image}, 0, 0, depthCompare, true});
    trim();
    timing.Mark("depth_view");
    return texture;
}

bool TextureCache::hostReadable(std::uint64_t address, std::uint64_t bytes) {
    if (TextureMemcmp() || context.guestBufferCache == nullptr || context.drawQueue == nullptr) return false;
    if (!context.drawQueue->CopiedWritesPending(address, static_cast<std::size_t>(bytes))) return true;
    context.drawQueue->Resolve(address, static_cast<std::size_t>(bytes), true);
    return false;
}

bool TextureCache::refresh(Surface& surface, std::uint64_t address, bool track) {
    const auto owner = surface.texture.lock();
    if (owner == nullptr || !owner->Refreshable() || !hostReadable(address, surface.bytes)) return false;
    auto view = context.guestBufferCache->HostRange(address, surface.bytes);
    if (!view.buffer) return false;
    if (context.renderCache != nullptr) context.renderCache->Resolve(address, static_cast<std::size_t>(surface.bytes), false);
    if (track) surface.stamp = context.guestBufferCache->Track(address, address + surface.bytes);
    auto linear = owner->Refresh(context.drawQueue->BeginBarrier(context), view.buffer->Handle(), view.offset);
    context.drawQueue->AddRead(address, address + surface.bytes);
    context.drawQueue->EnqueueUpload([source = std::move(view.buffer), linear = std::move(linear)] {}, static_cast<std::size_t>(surface.bytes));
    if (!surface.snapshot.empty()) std::vector<std::byte>().swap(surface.snapshot);
    return true;
}

std::shared_ptr<Texture> TextureCache::hostTexture(const GuestTextureResource& resource, VkComponentMapping components, std::uint64_t bytes, std::uint64_t stamp) {
    const auto address = resource.baseAddress;
    if (stamp == 0 || context.guestBufferCache == nullptr || context.guestBufferCache->Check(address, address + bytes, stamp) == GuestBufferCache::RangeState::Untracked || !hostReadable(address, bytes)) return nullptr;
    auto view = context.guestBufferCache->HostRange(address, bytes);
    if (!view.buffer) return nullptr;
    if (context.renderCache != nullptr) context.renderCache->Resolve(address, static_cast<std::size_t>(bytes), false);
    auto texture = std::make_shared<Texture>(context, *context.detiler, resource, components, Texture::HostSource{view.buffer->Handle(), view.offset});
    context.drawQueue->AddRead(address, address + bytes);
    context.drawQueue->EnqueueUpload([texture, source = std::move(view.buffer)] { texture->ReleaseUpload(); }, static_cast<std::size_t>(bytes));
    return texture;
}

bool TextureCache::SameSurface(const GuestTextureResource& a, const GuestTextureResource& b) {
    return a.baseAddress == b.baseAddress && a.width == b.width && a.height == b.height && a.mipCount == b.mipCount && a.tileMode == b.tileMode && FullArrayLayers(a) == FullArrayLayers(b) && (a.dimension == b.dimension || (a.dimension != TextureDimension::k3D && b.dimension != TextureDimension::k3D && a.dimension != TextureDimension::kCube && b.dimension != TextureDimension::kCube && a.dimension != TextureDimension::k1D && b.dimension != TextureDimension::k1D)) && ResolveTextureFormat(a.format) == ResolveTextureFormat(b.format);
}

std::shared_ptr<Texture> TextureCache::Get(std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components, bool storage, bool compare, const DepthImage* attachedDepth, bool stores) {
    Require(words.size() == 8, "texture cache descriptor must contain eight DWORDs");
    PerformanceTimer timing("Graphics.TextureCache");
    trim();
    std::array<std::uint32_t, 8> key;
    std::copy(words.begin(), words.end(), key.begin());
    std::vector<std::shared_ptr<DepthImage>> depthSources;
    bool stencil = false;
    if (resource.tileMode == TextureTileMode::Depth64KB && context.renderCache && !storage) {
        if (auto first = context.renderCache->FindDepth(resource.baseAddress, &stencil)) {
            if (first.get() == attachedDepth) timing.Mark("depth_feedback_copy");
            else if (auto direct = directDepth(key, first, stencil, resource, components, compare, timing)) return direct;
            const auto layers = FullArrayLayers(resource);
            const auto sliceBytes = DepthSliceBytes(BytesPerElement(resource.format), resource.width, resource.height);
            depthSources.push_back(std::move(first));
            for (std::uint32_t layer = 1; layer < layers; ++layer) {
                bool sliceStencil = false;
                auto slice = context.renderCache->FindDepth(resource.baseAddress + layer * sliceBytes, &sliceStencil);
                if (slice == nullptr || sliceStencil != stencil) break;
                depthSources.push_back(std::move(slice));
            }
            if (depthSources.size() != layers) {
                static std::once_flag once;
                std::call_once(once, [&] { APS5_LOG_OUT("depth texture 0x%llx has %zu of %u slices resident: sampled from guest memory", static_cast<unsigned long long>(resource.baseAddress), depthSources.size(), layers); });
                depthSources.clear();
            }
        }
    }
    if (!depthSources.empty()) {
        std::uint64_t generation = 0;
        for (const auto& slice : depthSources) generation += slice->Generation();
        if (const auto it = findEntry(key, resource.viewDimension, compare && !stencil); it != entries.end()) {
            const bool sameSources = it->depthSources.size() == depthSources.size() && std::equal(depthSources.begin(), depthSources.end(), it->depthSources.begin(), [](const auto& slice, const auto& cached) { return cached.lock() == slice; });
            if (sameSources && it->generation == generation) {
                auto result = it->texture;
                entries.splice(entries.end(), entries, it);
                timing.Mark("depth_hit");
                return result;
            }
            eraseEntry(it);
        }
        auto texture = std::make_shared<Texture>(context, std::span<const std::shared_ptr<DepthImage>>(depthSources), stencil, resource, components, compare && !stencil);
        addEntry({key, resource.viewDimension, texture, nullptr, {}, {depthSources.begin(), depthSources.end()}, generation, texture->AllocationBytes(), compare && !stencil});
        retainedBytes += texture->AllocationBytes();
        trim();
        timing.Mark("depth_copy");
        return texture;
    }
    auto source = context.renderCache ? context.renderCache->Find(resource.baseAddress) : nullptr;
    bool rowCopy = false;
    if (source) {
        const auto& color = source->Description();
        if (source->Layout() != VK_IMAGE_LAYOUT_GENERAL && context.drawQueue != nullptr) source->Transition(context.drawQueue->BeginBarrier(context), VK_IMAGE_LAYOUT_GENERAL);
        const auto compatibleTiling = (color.tileMode == ColorTileMode::RenderTarget && resource.tileMode == TextureTileMode::RenderTarget64KB) || (color.tileMode == ColorTileMode::Linear && resource.tileMode == TextureTileMode::kLinear) || (color.tileMode == ColorTileMode::ZOrder64KB && resource.tileMode == TextureTileMode::Depth64KB);
        const bool singleSlice = resource.dimension == TextureDimension::k2D || (resource.dimension == TextureDimension::k2DArray && resource.depthOrLastArray == 0);
        rowCopy = resource.dimension == TextureDimension::k1D && color.extent.height == 1 && resource.height <= 1 && resource.width == color.extent.width && compatibleTiling && resource.mipCount == 1 && resource.baseLevel == 0 && resource.baseArray == 0 && !IsBlockCompressed(resource.format) && BytesPerElement(resource.format) == color.bytesPerPixel;
        if (!rowCopy && (!compatibleTiling || resource.width != color.extent.width || resource.height != color.extent.height || !singleSlice || resource.mipCount != 1 || resource.baseLevel != 0 || resource.baseArray != 0 || IsBlockCompressed(resource.format) || BytesPerElement(resource.format) != color.bytesPerPixel)) {
            static int reported = 0;
            if (reported++ < 400) APS5_LOG_OUT("texture 0x%llx %ux%u format 0x%x tile %u mips %u dim %u (base level %u, base array %u, %u bpp) does not match the resident render target there (%ux%u tile %u bpp %u%s): sampled from guest memory", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, static_cast<unsigned>(resource.tileMode), resource.mipCount, static_cast<unsigned>(resource.dimension), resource.baseLevel, resource.baseArray, BytesPerElement(resource.format), color.extent.width, color.extent.height, static_cast<unsigned>(color.tileMode), color.bytesPerPixel, color.gpuOnly ? ", GPU-only" : "");
            source.reset();
        }
    }
    static const bool traceTextures = std::getenv("ANYPS5_TRACE_TEXTURES") != nullptr;
    bool snapshotMiss = stores;
    if (auto it = findEntry(key, resource.viewDimension); it != entries.end()) do {
        if (source || it->generation != 0 || !it->depthSources.empty()) {
            if (source && it->source.lock() == source && it->texture->IsDirectView() && it->texture->Image() == source->Target().Image()) {
                auto result = it->texture;
                entries.splice(entries.end(), entries, it);
                timing.Mark("resident_hit");
                return result;
            }
            if (source && rowCopy && it->source.lock() == source && !it->texture->IsDirectView() && it->generation == source->Generation()) {
                auto result = it->texture;
                entries.splice(entries.end(), entries, it);
                timing.Mark("row_copy_hit");
                return result;
            }
            eraseEntry(it);
            break;
        }
        Require(it->surface != nullptr, "texture cache entry has no surface");
        auto& surface = *it->surface;
        const auto end = resource.baseAddress + surface.bytes;
        const bool hostBacked = surface.snapshot.empty();
        if (stores && hostBacked) {
            surface.stale = true;
            eraseEntry(it);
            timing.Mark("store_snapshot");
            break;
        }
        surface.stored = surface.stored || stores;
        if (hostBacked && surface.volatileUses != 0) {
            const auto batch = context.drawQueue != nullptr ? context.drawQueue->RecordingBatch() : 0;
            if (batch != 0 && surface.refreshedBatch == batch && !context.drawQueue->WritesPending(resource.baseAddress, static_cast<std::size_t>(surface.bytes))) {
                auto result = it->texture;
                entries.splice(entries.end(), entries, it);
                timing.Mark("volatile_batch_hit");
                return result;
            }
            if (refresh(surface, resource.baseAddress, false)) {
                surface.refreshedBatch = batch;
                --surface.volatileUses;
                auto result = it->texture;
                entries.splice(entries.end(), entries, it);
                timing.Mark("volatile_refresh");
                return result;
            }
            surface.volatileUses = 0;
        }
        const auto state = context.guestBufferCache != nullptr && surface.stamp != 0 ? context.guestBufferCache->Check(resource.baseAddress, end, surface.stamp) : GuestBufferCache::RangeState::Untracked;
        if (state == GuestBufferCache::RangeState::Current) {
            surface.changedUses = 0;
            auto result = it->texture;
            entries.splice(entries.end(), entries, it);
            timing.Mark("stamp_hit");
            return result;
        }
        if (!surface.stored && state != GuestBufferCache::RangeState::Untracked && refresh(surface, resource.baseAddress, true)) {
            surface.changedUses = state == GuestBufferCache::RangeState::Written ? surface.changedUses + 1 : 0;
            if (surface.changedUses >= VolatileUses) {
                surface.changedUses = 0;
                surface.volatileUses = VolatileProbe;
            }
            auto result = it->texture;
            entries.splice(entries.end(), entries, it);
            timing.Mark("refresh");
            return result;
        }
        if (traceTextures) {
            static int reported = 0;
            if (reported++ < 4000) APS5_LOG_OUT("texture 0x%llx (%llu bytes, format 0x%x %ux%u) compared by bytes; stamp %llu:%s", static_cast<unsigned long long>(resource.baseAddress), static_cast<unsigned long long>(surface.bytes), resource.format, resource.width, resource.height, static_cast<unsigned long long>(surface.stamp), context.guestBufferCache ? context.guestBufferCache->Describe(resource.baseAddress, end, surface.stamp).c_str() : " (no cache)");
        }
        if (hostBacked) {
            surface.stale = true;
            eraseEntry(it);
            snapshotMiss = true;
            timing.Mark("host_fallback");
            break;
        }
        GuestMemory::CheckRange(reinterpret_cast<const void*>(resource.baseAddress), surface.snapshot.size(), 1);
        if (std::memcmp(reinterpret_cast<const void*>(resource.baseAddress), surface.snapshot.data(), surface.snapshot.size()) == 0) {
            if (context.guestBufferCache != nullptr) surface.stamp = context.guestBufferCache->Track(resource.baseAddress, end);
            auto result = it->texture;
            entries.splice(entries.end(), entries, it);
            timing.Mark("memcmp_hit");
            return result;
        }
        surface.stale = true;
        eraseEntry(it);
        break;
    } while (false);
    if (source && rowCopy) {
        auto texture = std::make_shared<Texture>(context, source, resource, components);
        addEntry({key, resource.viewDimension, texture, nullptr, source, {}, source->Generation(), texture->AllocationBytes()});
        retainedBytes += texture->AllocationBytes();
        trim();
        timing.Mark("row_copy");
        return texture;
    }
    if (source) {
        auto texture = std::make_shared<Texture>(context, source, resource, components, Texture::DirectView{});
        addEntry({key, resource.viewDimension, texture, nullptr, source, {}, source->Generation(), 0});
        trim();
        timing.Mark("resident_view");
        return texture;
    }
    for (auto it = entries.begin(); it != entries.end();) {
        if (it->surface && it->surface->stale) {
            it = eraseEntry(it);
            continue;
        }
        if (!it->surface || !SameSurface(it->surface->identity, resource)) {
            ++it;
            continue;
        }
        auto owner = it->surface->texture.lock();
        if (!owner || (stores && it->surface->snapshot.empty())) {
            it->surface->stale = true;
            it = eraseEntry(it);
            continue;
        }
        auto surface = it->surface;
        surface->stored = surface->stored || stores;
        auto texture = std::make_shared<Texture>(context, owner, resource, components);
        addEntry({key, resource.viewDimension, texture, surface, {}, {}, 0, 0});
        if (traceTextures) {
            static int reported = 0;
            if (reported++ < 2000) APS5_LOG_OUT("texture 0x%llx (format 0x%x %ux%u mips %u-%u of %u, dim %u) views the surface another descriptor detiled", static_cast<unsigned long long>(resource.baseAddress), resource.format, resource.width, resource.height, resource.baseLevel, resource.lastLevel, resource.mipCount, static_cast<unsigned>(resource.viewDimension));
        }
        timing.Mark("surface_view");
        return texture;
    }
    const auto mips = ComputeMipLayout(resource);
    const auto layers = FullArrayLayers(resource);
    const auto bytes = ComputeSurfaceSize(mips, layers);
    Require(bytes != 0 && bytes <= std::numeric_limits<std::size_t>::max(), "texture cache surface size overflow");
    if (traceTextures) {
        static int reported = 0;
        if (bytes >= (8u << 20) && context.renderCache != nullptr) {
            static int reportedLarge = 0;
            if (reportedLarge++ < 200) APS5_LOG_OUT("large texture 0x%llx (%ux%u format 0x%x tile %u) has no resident render target: %s", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, static_cast<unsigned>(resource.tileMode), context.renderCache->DescribeColorTargets().c_str());
        }
        if (reported++ < 6000) APS5_LOG_OUT("texture 0x%llx (%llu bytes, format 0x%x %ux%u mips %u layers %u) detiled", static_cast<unsigned long long>(resource.baseAddress), static_cast<unsigned long long>(bytes), resource.format, resource.width, resource.height, resource.mipCount, layers);
    }
    const auto stamp = context.guestBufferCache != nullptr ? context.guestBufferCache->Track(resource.baseAddress, resource.baseAddress + bytes) : 0;
    timing.Mark("miss_track");
    std::vector<std::byte> snapshot;
    std::shared_ptr<Texture> texture = snapshotMiss ? nullptr : hostTexture(resource, components, bytes, stamp);
    if (texture) {
        timing.Mark("miss_host_detile", bytes);
    } else {
        snapshot.resize(static_cast<std::size_t>(bytes));
        GuestMemory::Read(resource.baseAddress, snapshot, 1);
        timing.Mark("miss_read", snapshot.size());
        texture = std::make_shared<Texture>(context, *context.detiler, resource, components, snapshot);
        timing.Mark("miss_detile");
        if (context.drawQueue != nullptr) context.drawQueue->EnqueueUpload([texture] { texture->ReleaseUpload(); }, snapshot.size());
    }
    const auto retained = snapshot.size() + texture->AllocationBytes();
    auto surface = std::make_shared<Surface>();
    surface->identity = resource;
    surface->snapshot = std::move(snapshot);
    surface->bytes = bytes;
    surface->stamp = stamp;
    surface->stored = stores;
    surface->texture = texture;
    addEntry({key, resource.viewDimension, texture, surface, {}, {}, 0, retained});
    retainedBytes += retained;
    trim();
    timing.Mark("miss_retain");
    return texture;
}

namespace {
std::vector<unsigned char> ReadbackTexture(const Context& context, const Texture& texture) {
    const auto texel = texture.GuestTexelBytes();
    const auto extent = texture.Extent();
    const std::size_t bytes = static_cast<std::size_t>(extent.width) * extent.height * texel;
    Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    {
        CommandBatch batch(context, "readback");
        const auto commands = batch.Handle();
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout = texture.Layout();
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.Image();
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        pipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {extent.width, extent.height, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, texture.Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.Handle(), 1, &copy);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = texture.Layout();
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        batch.SubmitAndWait();
    }
    readback.Invalidate();
    const auto* data = reinterpret_cast<const unsigned char*>(readback.Bytes().data());
    return std::vector<unsigned char>(data, data + bytes);
}
}

std::vector<unsigned char> TextureCache::Contents(const Texture& texture) {
    if (context.drawQueue) context.drawQueue->Wait();
    return ReadbackTexture(context, texture);
}

std::string TextureCache::DescribeContents(const Texture& texture) {
    const auto texel = texture.GuestTexelBytes();
    if (texture.Image() == VK_NULL_HANDLE || texel == 0 || texel > 16) return " (not readable)";
    if (context.drawQueue) context.drawQueue->Wait();
    const auto data = ReadbackTexture(context, texture);
    std::size_t nonzero = 0;
    const std::size_t count = data.size() / texel;
    for (std::size_t at = 0; at + texel <= data.size(); at += texel) {
        bool any = false;
        for (std::uint32_t b = 0; b < texel; ++b) any = any || data[at + b] != 0;
        nonzero += any;
    }
    std::string samples;
    const auto extent = texture.Extent();
    for (const auto [fx, fy] : {std::pair{0.0, 0.0}, std::pair{0.25, 0.25}, std::pair{0.5, 0.5}, std::pair{0.33, 0.66}, std::pair{0.75, 0.75}}) {
        const std::size_t x = static_cast<std::size_t>(fx * (extent.width - 1)), y = static_cast<std::size_t>(fy * (extent.height - 1));
        const std::size_t at = (y * extent.width + x) * texel;
        if (at + texel > data.size()) break;
        samples += " ";
        for (std::uint32_t b = texel; b-- > 0;) { char h[4]; std::snprintf(h, sizeof(h), "%02x", data[at + b]); samples += h; }
        char where[40]; std::snprintf(where, sizeof(where), "@%zu,%zu", x, y); samples += where;
    }
    std::string stats;
    const auto format = texture.GuestFormat();
    if (format == VK_FORMAT_B10G11R11_UFLOAT_PACK32 || format == VK_FORMAT_R16G16B16A16_SFLOAT || format == VK_FORMAT_R16_SFLOAT) {
        const auto miniFloat = [](std::uint32_t bits, std::uint32_t mantissaBits) -> double {
            const std::uint32_t exponent = bits >> mantissaBits, mantissa = bits & ((1u << mantissaBits) - 1u);
            if (exponent == 31) return mantissa ? std::nan("") : INFINITY;
            if (exponent == 0) return std::ldexp(static_cast<double>(mantissa), -14 - static_cast<int>(mantissaBits));
            return std::ldexp(1.0 + static_cast<double>(mantissa) / (1u << mantissaBits), static_cast<int>(exponent) - 15);
        };
        const auto half = [&](std::uint32_t bits) -> double { const double magnitude = miniFloat(bits & 0x7fffu, 10); return (bits & 0x8000u) ? -magnitude : magnitude; };
        double maxima[4] = {0, 0, 0, 0};
        std::size_t nonFinite = 0;
        for (std::size_t at = 0; at + texel <= data.size(); at += texel) {
            double values[4] = {0, 0, 0, 0};
            if (format == VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
                std::uint32_t word; std::memcpy(&word, data.data() + at, 4);
                values[0] = miniFloat(word & 0x7ffu, 6); values[1] = miniFloat((word >> 11) & 0x7ffu, 6); values[2] = miniFloat(word >> 22, 5);
            } else {
                for (std::uint32_t c = 0; c < texel / 2; ++c) { std::uint16_t h; std::memcpy(&h, data.data() + at + c * 2, 2); values[c] = half(h); }
            }
            for (int c = 0; c < 4; ++c) {
                if (!std::isfinite(values[c])) { ++nonFinite; continue; }
                maxima[c] = std::max(maxima[c], std::fabs(values[c]));
            }
        }
        char item[128];
        std::snprintf(item, sizeof(item), " max(%.4g %.4g %.4g %.4g) nonfinite %zu;", maxima[0], maxima[1], maxima[2], maxima[3], nonFinite);
        stats = item;
    }
    std::string rows;
    if (format == VK_FORMAT_R16G16B16A16_SFLOAT && extent.width <= 64 && extent.height <= 64) {
        const auto miniFloat = [](std::uint32_t bits, std::uint32_t mantissaBits) -> double {
            const std::uint32_t exponent = bits >> mantissaBits, mantissa = bits & ((1u << mantissaBits) - 1u);
            if (exponent == 31) return mantissa ? std::nan("") : INFINITY;
            if (exponent == 0) return std::ldexp(static_cast<double>(mantissa), -14 - static_cast<int>(mantissaBits));
            return std::ldexp(1.0 + static_cast<double>(mantissa) / (1u << mantissaBits), static_cast<int>(exponent) - 15);
        };
        for (std::uint32_t y = 0; y < std::min(extent.height, 3u); ++y) {
            char line[48]; std::snprintf(line, sizeof(line), " row%u:", y); rows += line;
            for (std::uint32_t x = 0; x < std::min(extent.width, 4u); ++x) {
                const std::size_t at = (y * extent.width + x) * texel;
                if (at + 8 > data.size()) break;
                double v[4];
                for (int c = 0; c < 4; ++c) { std::uint16_t h; std::memcpy(&h, data.data() + at + c * 2, 2); const double m = miniFloat(h & 0x7fffu, 10); v[c] = (h & 0x8000u) ? -m : m; }
                std::snprintf(line, sizeof(line), " (%.3g %.3g %.3g %.3g)", v[0], v[1], v[2], v[3]); rows += line;
            }
        }
    }
    char text[96];
    std::snprintf(text, sizeof(text), " %zu of %zu texels nonzero;", nonzero, count);
    return text + stats + rows + samples;
}

void TextureCache::DumpTextures(const std::string& prefix) {
    if (context.drawQueue) context.drawQueue->Wait();
    int index = 0;
    std::size_t compressed = 0, other = 0, dumped = 0;
    for (const auto& entry : entries) {
        const auto& texture = *entry.texture;
        const auto texel = texture.GuestTexelBytes();
        if (texture.Image() == VK_NULL_HANDLE) continue;
        if (texel == 0) {
            ++compressed;
            const auto blockExtent = texture.Extent();
            const std::size_t blockBytes = static_cast<std::size_t>((blockExtent.width + 3) / 4) * ((blockExtent.height + 3) / 4) * 16;
            const std::vector<int> blocks(blockBytes);
            const std::size_t imageNonzero = 0;
            const auto address = ((static_cast<std::uint64_t>(entry.descriptor[1] & 0xffu) << 32) | entry.descriptor[0]) << 8;
            std::vector<std::byte> guest(std::min<std::size_t>(blocks.size(), 1u << 20));
            std::size_t guestNonzero = 0;
            try {
                GuestMemory::Read(address, guest);
                for (const auto byte : guest) guestNonzero += byte != std::byte{0};
            } catch (...) {}
            const auto extent = texture.Extent();
            APS5_LOG_OUT("compressed texture 0x%llx %ux%u format %u: (%zu, %zu) guest %zu of first %zu bytes nonzero",
                static_cast<unsigned long long>(address), extent.width, extent.height, static_cast<unsigned>(texture.GuestFormat()), imageNonzero, blocks.size(), guestNonzero, guest.size());
            continue;
        }
        if (texture.GuestLayers() != 1 || (texel != 1 && texel != 2 && texel != 4 && texel != 8)) { ++other; continue; }
        ++dumped;
        const auto extent = texture.Extent();
        const auto data0 = ReadbackTexture(context, texture);
        const auto* data = data0.data();
        char name[96];
        std::snprintf(name, sizeof(name), "tex%03d_%08x_%ux%u_f%u.bmp", index++, entry.descriptor[0], extent.width, extent.height, static_cast<unsigned>(texture.GuestFormat()));
        auto* file = std::fopen((prefix + name).c_str(), "wb");
        if (file == nullptr) continue;
        const std::uint32_t width = extent.width, height = extent.height, rowBytes = width * 4, imageBytes = rowBytes * height;
        unsigned char header[54] = {'B', 'M'};
        const auto put32 = [&](int at, std::uint32_t v) { header[at] = v & 0xff; header[at + 1] = (v >> 8) & 0xff; header[at + 2] = (v >> 16) & 0xff; header[at + 3] = (v >> 24) & 0xff; };
        put32(2, 54 + imageBytes); put32(10, 54); put32(14, 40); put32(18, width); put32(22, static_cast<std::uint32_t>(-static_cast<std::int32_t>(height))); header[26] = 1; header[28] = 32; put32(34, imageBytes);
        std::fwrite(header, 1, 54, file);
        const bool bgra = texture.GuestFormat() == VK_FORMAT_B8G8R8A8_UNORM || texture.GuestFormat() == VK_FORMAT_B8G8R8A8_SRGB;
        std::vector<unsigned char> row(rowBytes);
        for (std::uint32_t y = 0; y < height; ++y) {
            const auto* line = data + static_cast<std::size_t>(y) * width * texel;
            for (std::uint32_t x = 0; x < width; ++x) {
                if (texel == 8) {
                    row[x * 4] = line[x * 8 + 5]; row[x * 4 + 1] = line[x * 8 + 3]; row[x * 4 + 2] = line[x * 8 + 1]; row[x * 4 + 3] = 255;
                } else if (texel == 2) {
                    row[x * 4] = row[x * 4 + 1] = row[x * 4 + 2] = line[x * 2 + 1]; row[x * 4 + 3] = 255;
                } else if (texel == 1) {
                    row[x * 4] = row[x * 4 + 1] = row[x * 4 + 2] = line[x]; row[x * 4 + 3] = 255;
                } else {
                    row[x * 4] = line[x * 4 + (bgra ? 0 : 2)]; row[x * 4 + 1] = line[x * 4 + 1]; row[x * 4 + 2] = line[x * 4 + (bgra ? 2 : 0)]; row[x * 4 + 3] = 255;
                }
            }
            std::fwrite(row.data(), 1, rowBytes, file);
        }
        std::fclose(file);
    }
    APS5_LOG_OUT("texture cache dump: %zu entries, %zu dumped, %zu compressed, %zu other shapes", entries.size(), dumped, compressed, other);
}

std::shared_ptr<Texture> TextureCache::Null(TextureDimension dimension, TextureNumericClass numericClass) {
    auto& texture = nulls[{dimension, numericClass}];
    if (!texture) texture = std::make_shared<Texture>(context, dimension, numericClass);
    return texture;
}

std::shared_ptr<Texture> TextureCache::NullStorage(TextureDimension dimension, TextureNumericClass numericClass) {
    auto& texture = storageNulls[{dimension, numericClass}];
    if (!texture) texture = std::make_shared<Texture>(context, dimension, numericClass);
    return texture;
}

}

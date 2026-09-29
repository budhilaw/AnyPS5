#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RENDERCACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RENDERCACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <functional>
#include <map>

namespace AgcDriver::Graphics {

class ResidentColor {
public:
    ResidentColor(const Context& context, const ColorTarget& color);
    ~ResidentColor();
    void Begin(VkCommandBuffer commands);
    // Whether a draw in the open render pass can render into it as is: Begin would record only
    // a barrier the shared pass makes redundant. Continue then accounts for the draw.
    bool Attached() const { return layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && valid && (dirty || color.gpuOnly); }
    void Continue() { ++generation; }
    // Whether the contents came from guest memory on the GPU after the queued work numbered
    // `sequence`: what that work wrote in place is part of them.
    bool Adopted(std::uint64_t sequence) const { return valid && adoptedThrough > sequence; }
    std::uint64_t AdoptedThrough() const { return valid ? adoptedThrough : 0; }
    // Reloads invalid contents from guest memory on the GPU, after the queued work (what it
    // writes in place is included), leaving the image in the transfer destination layout. False,
    // recording nothing, when the memory cannot be read on the GPU.
    bool Refresh(VkCommandBuffer commands);
    void Download(VkCommandBuffer commands);
    void Commit();
    void Transition(VkCommandBuffer commands, VkImageLayout layout);
    VkImageLayout Layout() const { return layout; }
    RenderTarget& Target() { return transfer.Target(color, false); }
    const ColorTarget& Description() const { return color; }
    bool Valid() const { return valid; }
    bool Dirty() const { return dirty; }
    bool Watched() const { return memoryWatch != nullptr; }
    std::uint64_t lastUse = 0; // RenderCache use counter at the last lookup, for LRU eviction
    std::uint64_t Generation() const { return generation; }
    void Invalidate();
    // Drops the GPU contents without saving them (the guest memory is about to be overwritten).
    void Discard() { dirty = false; Invalidate(); }
    void ReleaseMemory();
    // The image received GPU writes outside a draw (a shader store through a texture copy).
    void MarkWritten() { dirty = true; ++generation; }
    // Takes the contents of `image` (same format and extent, in the transfer source layout) as
    // the target's current contents: the GPU owns the memory from now on.
    void Adopt(VkCommandBuffer commands, VkImage image);
    // Diagnostics: fills the target with a color.
    void DebugClear(VkCommandBuffer commands, float r, float g, float b);
    bool SharesPages(const ColorTarget& other) const;

private:
    void resolveCpuAccess(GuestMemoryTracking::Access access);
    Context context;
    ColorTarget color;
    GpuColorTransfer transfer;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool valid = false;
    bool dirty = false;
    std::uint64_t generation = 0;
    std::uint64_t adoptedThrough = 0; // queue sequence the last GPU-side upload follows
    std::unique_ptr<GuestMemoryTracking::Watch> memoryWatch;
};

class RenderCache {
public:
    explicit RenderCache(const Context& context) : context(context) {}
    ~RenderCache();
    std::shared_ptr<ResidentColor> Get(const ColorTarget& color, bool blending);
    // The guest ranges of every resident color target.
    void AppendColorRanges(std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) const {
        for (const auto& [address, entry] : entries) ranges.emplace_back(address, address + entry->Description().bytes);
    }
    // The parts of [begin, end) whose watchers need to hear of an in-place GPU write by the
    // queued work numbered `sequence` (resident targets that adopted it are left out).
    std::vector<std::pair<std::uint64_t, std::uint64_t>> UnadoptedRanges(std::uint64_t begin, std::uint64_t end, std::uint64_t sequence) const;
    // The parts of [begin, end) outside the pages of every target that watches its memory.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> UnwatchedRanges(std::uint64_t begin, std::uint64_t end) const;
    // Depth targets are keyed by their Z base address; a changed extent or format replaces the image.
    std::shared_ptr<DepthImage> GetDepth(const DepthTarget& depth);
    std::shared_ptr<ResidentColor> Find(std::uint64_t address) const;
    // The resident depth image whose Z base is `address`, if any (titles sample their depth
    // buffers through texture descriptors with the depth swizzle).
    // `stencil` receives whether the address names the stencil surface rather than Z.
    std::shared_ptr<DepthImage> FindDepth(std::uint64_t address, bool* stencil = nullptr) const;
    std::string DescribeDepthTargets() const;
    std::string DescribeColorTargets() const;
    // Diagnostics: reads every valid resident target back and writes it as <prefix><address>.bmp.
    void DumpTargets(const std::string& prefix);
    void DumpDepthTargets(const std::string& prefix);
    void Resolve(std::uint64_t address, std::size_t bytes, bool writable);
    // Drops the GPU contents of targets lying wholly inside [address, address + bytes) without a
    // download: the caller overwrites that memory completely.
    void DiscardCovered(std::uint64_t address, std::size_t bytes);
    void Flush();

private:
    std::vector<std::pair<std::uint64_t, std::uint64_t>> excluding(std::uint64_t begin, std::uint64_t end, const std::function<bool(const ResidentColor&)>& excluded) const;
    Context context;
    std::map<std::uint64_t, std::shared_ptr<ResidentColor>> entries;
    std::uint64_t useCounter = 0;
    struct DepthEntry {
        DepthTarget target;
        std::shared_ptr<DepthImage> image;
    };
    std::map<std::uint64_t, DepthEntry> depthEntries;
};

}

#endif

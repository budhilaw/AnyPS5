#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RENDERCACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RENDERCACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <atomic>
#include <functional>
#include <map>

namespace AgcDriver::Graphics {

class ResidentColor {
public:
    ResidentColor(const Context& context, const ColorTarget& color);
    ~ResidentColor();
    void Begin(VkCommandBuffer commands);
    bool Attached() const { return layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && valid && (dirty || color.gpuOnly); }
    void Continue() { ++generation; }
    bool Adopted(std::uint64_t sequence) const { return valid && adoptedThrough > sequence; }
    std::uint64_t AdoptedThrough() const { return valid ? adoptedThrough : 0; }
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
    std::uint64_t lastUse = 0;
    std::uint64_t Generation() const { return generation; }
    void Invalidate();
    void Discard() { dirty = false; Invalidate(); }
    void ReleaseMemory();
    void MarkWritten() { dirty = true; ++generation; }
    void Adopt(VkCommandBuffer commands, VkImage image);
    void DebugClear(VkCommandBuffer commands, float r, float g, float b);
    bool SharesPages(const ColorTarget& other, std::uint64_t pageSize) const;

private:
    void resolveCpuAccess(GuestMemoryTracking::Access access);
    void retainUploadSource();
    Context context;
    ColorTarget color;
    GpuColorTransfer transfer;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool valid = false;
    bool dirty = false;
    std::uint64_t generation = 0;
    std::uint64_t adoptedThrough = 0;
    std::unique_ptr<GuestMemoryTracking::Watch> memoryWatch;
};

class RenderCache {
public:
    explicit RenderCache(const Context& context) : context(context), pageSize(GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix()) {}
    ~RenderCache();
    std::shared_ptr<ResidentColor> Get(const ColorTarget& color, bool blending);
    std::uint64_t Epoch() const { return epoch.load(std::memory_order_acquire); }
    void AppendColorRanges(std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) const {
        for (const auto& [address, entry] : entries) ranges.emplace_back(address, address + entry->Description().bytes);
    }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> UnadoptedRanges(std::uint64_t begin, std::uint64_t end, std::uint64_t sequence) const;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> UnwatchedRanges(std::uint64_t begin, std::uint64_t end) const;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> WatchedRanges(std::uint64_t begin, std::uint64_t end, std::uint64_t sequence) const;
    std::shared_ptr<DepthImage> GetDepth(const DepthTarget& depth);
    std::shared_ptr<ResidentColor> Find(std::uint64_t address) const;
    std::shared_ptr<DepthImage> FindDepth(std::uint64_t address, bool* stencil = nullptr) const;
    std::string DescribeDepthTargets() const;
    std::string DescribeColorTargets() const;
    void DumpTargets(const std::string& prefix, std::uint64_t only = 0);
    std::string DescribeStencil(DepthImage& image);
    void DumpDepthTargets(const std::string& prefix);
    void Resolve(std::uint64_t address, std::size_t bytes, bool writable);
    void DiscardCovered(std::uint64_t address, std::size_t bytes);
    void Flush();

private:
    using Entries = std::map<std::uint64_t, std::shared_ptr<ResidentColor>>;
    void retire(std::shared_ptr<ResidentColor> entry);
    Entries::iterator release(Entries::iterator entry);
    std::uint64_t reachStart(std::uint64_t address) const { return address > largestTarget + pageSize ? address - largestTarget - pageSize : 0; }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> excluding(std::uint64_t begin, std::uint64_t end, const std::function<bool(const ResidentColor&)>& excluded) const;
    Context context;
    std::uint64_t pageSize;
    Entries entries;
    std::uint64_t largestTarget = 0;
    std::atomic<std::uint64_t> epoch{0};
    std::uint64_t useCounter = 0;
    struct DepthEntry {
        DepthTarget target;
        std::shared_ptr<DepthImage> image;
    };
    std::map<std::uint64_t, DepthEntry> depthEntries;
};

}

#endif

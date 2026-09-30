#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAWQUEUE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAWQUEUE_HPP

#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <vector>

namespace AgcDriver::Graphics {

struct RenderPassKey {
    std::array<VkImageView, 10> views{};
    std::uint32_t viewCount = 0;
    VkExtent2D extent{};
    bool operator==(const RenderPassKey& other) const {
        return viewCount == other.viewCount && extent.width == other.extent.width && extent.height == other.extent.height && std::equal(views.begin(), views.begin() + viewCount, other.views.begin());
    }
};

class DrawQueue {
public:
    ~DrawQueue();
    VkCommandBuffer Begin(const Context& context);
    VkCommandBuffer BeginBarrier(const Context& context) { const auto commands = Begin(context); recording.hasBarrier = true; return commands; }
    VkCommandBuffer ContinuePass(const RenderPassKey& key);
    void OpenPass(const Context& context, const RenderPassKey& key);
    void EndPass();
    VkPipeline BoundPipeline() const { return boundPipeline; }
    void SetBoundPipeline(VkPipeline pipeline) { boundPipeline = pipeline; }
    void Enqueue(std::shared_ptr<ShaderResources> resources, std::shared_ptr<void> storage);
    void EnqueueCompletion(std::function<void()> action);
    void EnqueueUpload(std::function<void()> release, std::size_t bytes);
    bool HasPending() const { return recording.commands != nullptr || !pending.empty(); }
    void Flush();
    void Resolve(std::uint64_t address, std::size_t bytes, bool ordered = false);
    bool WritesPending(std::uint64_t address, std::size_t bytes, std::uint64_t adoptedBefore = 0) const;
    std::uint64_t NextSequence() const { return nextSequence; }
    void AppendWriteRanges(std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) const {
        for (const auto& writer : writers) ranges.insert(ranges.end(), writer.resources->WriteRanges().begin(), writer.resources->WriteRanges().end());
    }
    void Wait();
    void WaitGpu();
    void Collect();
    void RecordMemoryBarrier(const Context& context);

private:
    struct Entry {
        std::shared_ptr<void> storage;
        std::shared_ptr<ShaderResources> resources;
        std::uint64_t sequence = 0;
    };
    struct Batch {
        std::vector<Entry> entries;
        std::unique_ptr<CommandBatch> commands;
        bool hasBarrier = false;
        std::size_t uploadBytes = 0;
        std::vector<std::function<void()>> completions;
    };
    void retire(Batch batch);
    void throttle();
    Batch recording;
    std::vector<Batch> pending;
    std::vector<std::unique_ptr<CommandBatch>> available;
    std::size_t drawCount = 0;
    std::uint64_t nextSequence = 1;
    struct Writer {
        std::uint64_t sequence;
        const ShaderResources* resources;
    };
    std::vector<Writer> writers;
    std::chrono::nanoseconds lastCompletion{};
    bool passOpen = false;
    RenderPassKey pass;
    VkPipeline boundPipeline = VK_NULL_HANDLE;
    std::shared_ptr<ReleaseQueue> releases;
    PFN_vkCmdEndRenderPass endRenderPass = nullptr;
    PFN_vkCmdPipelineBarrier pipelineBarrier = nullptr;
};

}

#endif

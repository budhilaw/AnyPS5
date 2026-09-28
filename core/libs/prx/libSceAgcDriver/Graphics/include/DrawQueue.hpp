#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAWQUEUE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAWQUEUE_HPP

#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <vector>

namespace AgcDriver::Graphics {

// The attachments and render area of a render pass instance: draws with equal keys share one.
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
    // The recording command buffer, outside any render pass (an open one ends first).
    VkCommandBuffer Begin(const Context& context);
    // Begin, for a barrier or transition recorded outside a draw or dispatch.
    VkCommandBuffer BeginBarrier(const Context& context) { const auto commands = Begin(context); recording.hasBarrier = true; return commands; }
    // The recording command buffer inside the open render pass when its key is `key`, else null.
    VkCommandBuffer ContinuePass(const RenderPassKey& key);
    // Marks the render pass the caller began on the recording command buffer as open.
    void OpenPass(const Context& context, const RenderPassKey& key);
    // Ends the open render pass (recording its results' barrier), if any.
    void EndPass();
    // The pipeline bound last in the open render pass (binding it again is redundant).
    VkPipeline BoundPipeline() const { return boundPipeline; }
    void SetBoundPipeline(VkPipeline pipeline) { boundPipeline = pipeline; }
    void Enqueue(std::shared_ptr<ShaderResources> resources, std::shared_ptr<void> storage);
    // Runs `action` once the GPU work recorded so far has completed (the console's end-of-pipe
    // label writes). With nothing recorded or pending it runs at once.
    void EnqueueCompletion(std::function<void()> action);
    bool HasPending() const { return recording.commands != nullptr || !pending.empty(); }
    void Flush();
    // Waits for queued work that writes the range. With `ordered`, work that writes guest memory
    // in place does not count: the caller reads the range on the GPU in queue order.
    void Resolve(std::uint64_t address, std::size_t bytes, bool ordered = false);
    // Whether queued work writes the range; with `adoptedBefore`, in-place writes by work
    // numbered below it do not count (the caller's GPU copy follows them).
    bool WritesPending(std::uint64_t address, std::size_t bytes, std::uint64_t adoptedBefore = 0) const;
    // The sequence number the next queued draw or dispatch receives: GPU work recorded now
    // follows every queued one with a lower number.
    std::uint64_t NextSequence() const { return nextSequence; }
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
        std::vector<std::function<void()>> completions;
    };
    void retire(Batch batch);
    void throttle();
    Batch recording;
    std::vector<Batch> pending;
    std::vector<std::unique_ptr<CommandBatch>> available;
    std::size_t drawCount = 0;
    std::uint64_t nextSequence = 1;
    // Queued work that writes guest memory, oldest first (most draws write none): the range
    // checks every guest read makes scan only these.
    struct Writer {
        std::uint64_t sequence;
        const ShaderResources* resources;
    };
    std::vector<Writer> writers;
    std::chrono::nanoseconds lastCompletion{};
    bool passOpen = false;
    RenderPassKey pass;
    VkPipeline boundPipeline = VK_NULL_HANDLE;
    PFN_vkCmdEndRenderPass endRenderPass = nullptr;
    PFN_vkCmdPipelineBarrier pipelineBarrier = nullptr;
};

}

#endif

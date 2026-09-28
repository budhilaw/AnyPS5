#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace AgcDriver::Graphics {

namespace {

// Draws per submitted command buffer, and the command buffers the GPU may have in flight before
// recording waits for the oldest (which bounds the memory in-flight draws hold). Earlier
// batches of 8 draws with a full drain every 64 kept the GPU and the recorder taking turns.
constexpr std::size_t BatchDraws = 64;
constexpr std::size_t MaxPendingBatches = 4;

}

DrawQueue::~DrawQueue() {
    recording.commands.reset();
    for (auto& batch : pending) batch.commands.reset();
}

VkCommandBuffer DrawQueue::Begin(const Context& context) {
    EndPass();
    Collect();
    throttle();
    if (!recording.commands) {
        if (available.empty()) recording.commands = std::make_unique<CommandBatch>(context);
        else {
            recording.commands = std::move(available.back());
            available.pop_back();
            recording.commands->Reset();
        }
    }
    return recording.commands->Handle();
}

void DrawQueue::throttle() {
    while (pending.size() >= MaxPendingBatches) {
        PerformanceTimer timing("Graphics.DrawQueue.Throttle");
        pending.front().commands->Wait();
        timing.Mark("fence_wait");
        auto batch = std::move(pending.front());
        pending.erase(pending.begin());
        retire(std::move(batch));
        timing.Mark("retire");
    }
}

VkCommandBuffer DrawQueue::ContinuePass(const RenderPassKey& key) {
    if (!passOpen || !(pass == key)) return VK_NULL_HANDLE;
    return recording.commands->Handle();
}

void DrawQueue::OpenPass(const Context& context, const RenderPassKey& key) {
    Require(recording.commands != nullptr && !passOpen, "render pass opened outside a recording batch or inside another");
    if (endRenderPass == nullptr) {
        endRenderPass = context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass");
        pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    }
    passOpen = true;
    pass = key;
    boundPipeline = VK_NULL_HANDLE;
}

void DrawQueue::EndPass() {
    if (!passOpen) return;
    passOpen = false;
    boundPipeline = VK_NULL_HANDLE;
    const auto commands = recording.commands->Handle();
    endRenderPass(commands);
    // What the pass's draws wrote is visible to later transfers, shaders and the host.
    VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    download.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    download.dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
}

void DrawQueue::Enqueue(std::shared_ptr<ShaderResources> resources, std::shared_ptr<void> storage) {
    Require(recording.commands != nullptr && resources != nullptr && storage != nullptr, "draw batch is incomplete");
    if (resources->HasGuestWrites()) writers.push_back({nextSequence, resources.get()});
    recording.entries.push_back({std::move(storage), std::move(resources), nextSequence++});
    ++drawCount;
    if (recording.entries.size() >= BatchDraws) Flush();
}

void DrawQueue::EnqueueCompletion(std::function<void()> action) {
    if (recording.commands) {
        recording.completions.push_back(std::move(action));
        return;
    }
    if (!pending.empty()) {
        pending.back().completions.push_back(std::move(action));
        return;
    }
    action();
}

void DrawQueue::EnqueueUpload(std::function<void()> release, std::size_t bytes) {
    constexpr std::size_t BatchUploadBytes = 64u << 20;
    Require(recording.commands != nullptr, "upload recorded outside a batch");
    recording.completions.push_back(std::move(release));
    recording.uploadBytes += bytes;
    if (recording.uploadBytes >= BatchUploadBytes) Flush();
}

void DrawQueue::Flush() {
    if (!recording.commands) return;
    EndPass();
    // A batch without draws still carries barriers and layout transitions (or a draw failed
    // after it began): submitting it keeps the command buffer reusable and the queue in order.
    pending.push_back(std::move(recording));
    recording = Batch{};
    pending.back().commands->Submit();
}

bool DrawQueue::WritesPending(std::uint64_t address, std::size_t bytes, std::uint64_t adoptedBefore) const {
    return std::any_of(writers.begin(), writers.end(), [&](const Writer& writer) { return writer.sequence < adoptedBefore ? writer.resources->WritesOverlapCopied(address, bytes) : writer.resources->WritesOverlap(address, bytes); });
}

void DrawQueue::Resolve(std::uint64_t address, std::size_t bytes, bool ordered) {
    const auto overlaps = [&](const Writer& writer) {
        const auto& resources = *writer.resources;
        if (ordered ? !resources.WritesOverlapCopied(address, bytes) : !resources.WritesOverlap(address, bytes)) return false;
        // ANYPS5_TRACE_WAITS: the in-flight write ranges that make a target lookup wait (diagnostics).
        static const char* traceValue = std::getenv("ANYPS5_TRACE_WAITS");
        static const auto traceStart = std::chrono::steady_clock::now();
        static int reported = 0;
        if (traceValue != nullptr && reported < 200 && std::chrono::duration<double>(std::chrono::steady_clock::now() - traceStart).count() >= std::atof(traceValue)) {
            ++reported;
            std::string ranges;
            for (const auto& [begin, end] : resources.WriteRanges()) {
                char item[64];
                std::snprintf(item, sizeof(item), " 0x%llx+0x%llx", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin));
                ranges += item;
            }
            std::fprintf(stderr, "[resolve-wait] 0x%llx+0x%zx overlaps writes%s (ordered %d, copied %d)\n", static_cast<unsigned long long>(address), bytes, ranges.c_str(), ordered ? 1 : 0, resources.WritesOverlapCopied(address, bytes) ? 1 : 0);
        }
        return true;
    };
    if (std::any_of(writers.begin(), writers.end(), overlaps)) Wait();
}


}

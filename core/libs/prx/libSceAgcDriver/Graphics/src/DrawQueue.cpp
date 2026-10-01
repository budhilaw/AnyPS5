#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReleaseQueue.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace AgcDriver::Graphics {

namespace {

constexpr std::size_t BatchDraws = 64;
constexpr std::size_t MaxPendingBatches = 4;

}

void WriteIntervals::Clear() {
    intervals.clear();
    reach.clear();
}

void WriteIntervals::Add(std::uint64_t begin, std::uint64_t end, std::uint64_t sequence, bool copied) {
    intervals.push_back({begin, end, sequence, copied});
}

void WriteIntervals::Build() {
    std::sort(intervals.begin(), intervals.end(), [](const Interval& left, const Interval& right) {
        if (left.begin != right.begin) return left.begin < right.begin;
        if (left.end != right.end) return left.end < right.end;
        return left.copied < right.copied;
    });
    std::size_t kept = 0;
    for (const auto& interval : intervals) {
        if (kept != 0) {
            auto& previous = intervals[kept - 1];
            if (previous.begin == interval.begin && previous.end == interval.end && previous.copied == interval.copied) {
                previous.sequence = std::max(previous.sequence, interval.sequence);
                continue;
            }
        }
        intervals[kept++] = interval;
    }
    intervals.resize(kept);
    reach.resize(kept);
    std::uint64_t furthest = 0;
    for (std::size_t index = 0; index < kept; ++index) {
        furthest = std::max(furthest, intervals[index].end);
        reach[index] = furthest;
    }
}

std::size_t WriteIntervals::first(std::uint64_t address) const {
    return static_cast<std::size_t>(std::partition_point(reach.begin(), reach.end(), [address](std::uint64_t furthest) { return furthest <= address; }) - reach.begin());
}

std::uint64_t WriteIntervals::Latest(std::uint64_t address, std::size_t bytes, bool copiedOnly) const {
    const auto end = address + bytes;
    std::uint64_t latest = 0;
    for (auto index = first(address); index < intervals.size() && intervals[index].begin < end; ++index) {
        const auto& interval = intervals[index];
        if (address < interval.end && (interval.copied || !copiedOnly)) latest = std::max(latest, interval.sequence);
    }
    return latest;
}

bool WriteIntervals::Pending(std::uint64_t address, std::size_t bytes, std::uint64_t adoptedBefore) const {
    const auto end = address + bytes;
    for (auto index = first(address); index < intervals.size() && intervals[index].begin < end; ++index) {
        const auto& interval = intervals[index];
        if (address < interval.end && (interval.copied || interval.sequence >= adoptedBefore)) return true;
    }
    return false;
}

DrawQueue::~DrawQueue() {
    recording.commands.reset();
    for (auto& batch : pending) batch.commands.reset();
}

VkCommandBuffer DrawQueue::Begin(const Context& context) {
    workSinceBarrier = true;
    return begin(context);
}

VkCommandBuffer DrawQueue::begin(const Context& context) {
    releases = context.releaseQueue;
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

void DrawQueue::OpenPass(const Context& context, const RenderPassKey& key, std::uint64_t colorAddress, std::uint64_t depthAddress) {
    Require(recording.commands != nullptr && !passOpen, "render pass opened outside a recording batch or inside another");
    if (endRenderPass == nullptr) {
        endRenderPass = context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass");
        pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    }
    passOpen = true;
    pass = key;
    boundPipeline = VK_NULL_HANDLE;
    const bool depthOnly = colorAddress == 0 && depthAddress != 0;
    recording.commands->BeginRegion(depthOnly ? GpuWork::DepthPass : GpuWork::ColorPass, depthOnly ? depthAddress : colorAddress, {key.extent.width, key.extent.height, 1});
}

void DrawQueue::EndPass() {
    if (!passOpen) return;
    passOpen = false;
    boundPipeline = VK_NULL_HANDLE;
    const auto commands = recording.commands->Handle();
    recording.commands->EndRegion();
    endRenderPass(commands);
    VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    download.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    download.dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
}

void DrawQueue::Enqueue(std::shared_ptr<ShaderResources> resources, std::shared_ptr<void> storage) {
    Require(recording.commands != nullptr && resources != nullptr && storage != nullptr, "draw batch is incomplete");
    if (resources->HasGuestWrites()) {
        writers.push_back({nextSequence, resources.get()});
        writerEpoch.fetch_add(1, std::memory_order_release);
    }
    if (resources->UsesGds()) lastGdsSequence = nextSequence;
    recording.entries.push_back({std::move(storage), std::move(resources), nextSequence++});
    ++drawCount;
    workSinceBarrier = true;
    if (recording.entries.size() >= BatchDraws) Flush();
}

void DrawQueue::MarkGds() {
    Require(recording.commands != nullptr, "GDS work recorded outside a batch");
    lastGdsSequence = nextSequence;
    recording.entries.push_back({nullptr, nullptr, nextSequence++});
    ++drawCount;
    workSinceBarrier = true;
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
    workSinceBarrier = true;
    if (recording.uploadBytes >= BatchUploadBytes) Flush();
}

void DrawQueue::Flush() {
    if (!recording.commands) return;
    EndPass();
    pending.push_back(std::move(recording));
    recording = Batch{};
    pending.back().commands->Submit();
    if (releases) releases->Collect();
}

const WriteIntervals& DrawQueue::writeIntervals() const {
    const auto epoch = writerEpoch.load(std::memory_order_relaxed);
    if (intervalsEpoch == epoch) return intervals;
    intervals.Clear();
    for (const auto& writer : writers) {
        for (const auto& [begin, end] : writer.resources->WriteRanges()) intervals.Add(begin, end, writer.sequence, writer.resources->WriteCopied(begin));
    }
    intervals.Build();
    intervalsEpoch = epoch;
    return intervals;
}

bool DrawQueue::WritesPending(std::uint64_t address, std::size_t bytes, std::uint64_t adoptedBefore) const {
    return writeIntervals().Pending(address, bytes, adoptedBefore);
}

void DrawQueue::traceResolve(std::uint64_t address, std::size_t bytes, bool ordered, const char* traceValue) const {
    static const auto traceStart = std::chrono::steady_clock::now();
    static int reported = 0;
    for (const auto& writer : writers) {
        const auto& resources = *writer.resources;
        if (ordered ? !resources.WritesOverlapCopied(address, bytes) : !resources.WritesOverlap(address, bytes)) continue;
        if (reported >= 200 || std::chrono::duration<double>(std::chrono::steady_clock::now() - traceStart).count() < std::atof(traceValue)) continue;
        ++reported;
        std::string ranges;
        for (const auto& [begin, end] : resources.WriteRanges()) {
            char item[64];
            std::snprintf(item, sizeof(item), " 0x%llx+0x%llx", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin));
            ranges += item;
        }
        std::fprintf(stderr, "[resolve-wait] 0x%llx+0x%zx overlaps writes%s (ordered %d, copied %d)\n", static_cast<unsigned long long>(address), bytes, ranges.c_str(), ordered ? 1 : 0, resources.WritesOverlapCopied(address, bytes) ? 1 : 0);
    }
}

void DrawQueue::Resolve(std::uint64_t address, std::size_t bytes, bool ordered) {
    static const char* traceValue = std::getenv("ANYPS5_TRACE_WAITS");
    if (traceValue != nullptr) traceResolve(address, bytes, ordered, traceValue);
    const auto last = writeIntervals().Latest(address, bytes, ordered);
    if (last != 0) waitThrough(last);
}

void DrawQueue::waitThrough(std::uint64_t sequence) {
    if (sequence <= retiredThrough) return;
    PerformanceTimer timing("Graphics.DrawQueue.WaitThrough");
    if (!recording.entries.empty() && recording.entries.front().sequence <= sequence) Flush();
    timing.Mark("submit");
    while (!pending.empty()) {
        const bool covers = !pending.front().entries.empty() && pending.front().entries.back().sequence >= sequence;
        pending.front().commands->Wait();
        timing.Mark("fence_wait");
        auto batch = std::move(pending.front());
        pending.erase(pending.begin());
        retire(std::move(batch));
        timing.Mark("retire");
        if (covers) break;
    }
}


}

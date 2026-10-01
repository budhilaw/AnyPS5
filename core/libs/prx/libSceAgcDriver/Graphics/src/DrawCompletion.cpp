#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/CallerSymbol.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace AgcDriver::Graphics {

void DrawQueue::retire(Batch batch) {
    if (auto* frame = PerformanceContext::Current(); frame != nullptr && batch.commands) {
        const auto begin = std::max(batch.commands->submittedAt, lastCompletion);
        if (batch.commands->completedAt > begin) frame->Add(frame->Get("Graphics.GpuEstimate", "busy"), std::chrono::duration_cast<FrameTiming::Clock::duration>(batch.commands->completedAt - begin));
        if (batch.commands->gpuTime.count() > 0) frame->Add(frame->Get("Graphics.GpuTime", "execute"), std::chrono::duration_cast<FrameTiming::Clock::duration>(batch.commands->gpuTime));
        lastCompletion = std::max(lastCompletion, batch.commands->completedAt);
    }
    if (batch.commands) ReportGpuRegions(batch.commands->Regions(), batch.commands->DroppedRegions(), batch.commands->gpuTime);
    PerformanceTimer timing("Graphics.DrawQueue.Retire");
    const GuestMemory::MemoryAccessScope suspended(nullptr, nullptr);
    if (!batch.reads.empty()) readRanges.fetch_sub(batch.reads.size(), std::memory_order_release);
    batch.reads.clear();
    Require(drawCount >= batch.entries.size(), "draw queue completion count underflow");
    drawCount -= batch.entries.size();
    if (!batch.entries.empty()) {
        const auto last = batch.entries.back().sequence;
        const auto retired = std::find_if(writers.begin(), writers.end(), [&](const Writer& writer) { return writer.sequence > last; });
        if (retired != writers.begin()) {
            writers.erase(writers.begin(), retired);
            writerEpoch.fetch_add(1, std::memory_order_release);
        }
        retiredThrough = std::max(retiredThrough, last);
    }
    for (auto& entry : batch.entries) {
        if (entry.resources) entry.resources->WriteBack(entry.sequence);
    }
    timing.Mark("resources_writeback");
    batch.entries.clear();
    for (auto& completion : batch.completions) completion();
    batch.completions.clear();
    timing.Mark("completions");
    available.push_back(std::move(batch.commands));
    if (batch.uploads) {
        batch.uploads->Wait();
        availableUploads.push_back(std::move(batch.uploads));
    }
    timing.Mark("resources_release");
}

void DrawQueue::Collect() {
    while (!pending.empty() && pending.front().commands->IsComplete()) {
        auto batch = std::move(pending.front());
        pending.erase(pending.begin());
        retire(std::move(batch));
    }
}

void DrawQueue::WaitGpu() {
    Flush();
    for (auto& batch : pending) batch.commands->Wait();
}

void DrawQueue::Wait() {
    if (pending.empty() && !recording.commands) return;
    PerformanceTimer timing("Graphics.DrawQueue.Wait");
    if (static const char* traceWaits = std::getenv("ANYPS5_TRACE_WAITS"); traceWaits != nullptr) {
        static const auto traceStart = std::chrono::steady_clock::now();
        static int reported = 0;
        if (reported < 80 && std::chrono::duration<double>(std::chrono::steady_clock::now() - traceStart).count() >= std::atof(traceWaits)) {
            ++reported;
            void* frames[9];
            const int count = CaptureCallers(frames, 9);
            std::string chain;
            for (int i = 1; i < count; ++i) chain += " <- " + DescribeCaller(frames[i]);
            std::fprintf(stderr, "[wait]%s\n", chain.c_str());
            std::fflush(stderr);
        }
    }
    Flush();
    timing.Mark("submit");
    while (!pending.empty()) {
        pending.front().commands->Wait();
        timing.Mark("fence_wait");
        auto batch = std::move(pending.front());
        pending.erase(pending.begin());
        retire(std::move(batch));
        timing.Mark("retire");
    }
}

void DrawQueue::RecordMemoryBarrier(const Context& context) {
    if (!workSinceBarrier) return;
    const auto commands = begin(context);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    recording.hasBarrier = true;
    workSinceBarrier = false;
}

}

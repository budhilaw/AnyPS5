#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <execinfo.h>
#include <string>

namespace AgcDriver::Graphics {

void DrawQueue::retire(Batch batch) {
    // GPU time estimate: a batch runs from its submission (or the previous batch's completion)
    // until the host saw its fence signalled; polling makes this an upper bound.
    if (auto* frame = PerformanceContext::Current(); frame != nullptr && batch.commands) {
        const auto begin = std::max(batch.commands->submittedAt, lastCompletion);
        if (batch.commands->completedAt > begin) frame->Add(frame->Get("Graphics.GpuEstimate", "busy"), std::chrono::duration_cast<FrameTiming::Clock::duration>(batch.commands->completedAt - begin));
        lastCompletion = std::max(lastCompletion, batch.commands->completedAt);
    }
    PerformanceTimer timing("Graphics.DrawQueue.Retire");
    const GuestMemory::MemoryAccessScope suspended(nullptr, nullptr);
    Require(drawCount >= batch.entries.size(), "draw queue completion count underflow");
    drawCount -= batch.entries.size();
    if (!batch.entries.empty()) {
        const auto last = batch.entries.back().sequence;
        writers.erase(writers.begin(), std::find_if(writers.begin(), writers.end(), [&](const Writer& writer) { return writer.sequence > last; }));
    }
    for (auto& entry : batch.entries) entry.resources->WriteBack(entry.sequence);
    timing.Mark("resources_writeback");
    batch.entries.clear();
    for (auto& completion : batch.completions) completion();
    batch.completions.clear();
    timing.Mark("completions");
    available.push_back(std::move(batch.commands));
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
    // ANYPS5_TRACE_WAITS=<seconds>: from that time on, the callers of the first waits (diagnostics).
    if (static const char* traceWaits = std::getenv("ANYPS5_TRACE_WAITS"); traceWaits != nullptr) {
        static const auto traceStart = std::chrono::steady_clock::now();
        static int reported = 0;
        if (reported < 80 && std::chrono::duration<double>(std::chrono::steady_clock::now() - traceStart).count() >= std::atof(traceWaits)) {
            ++reported;
            void* frames[9];
            const int count = ::backtrace(frames, 9);
            std::string chain;
            for (int i = 1; i < count; ++i) {
                Dl_info info{};
                dladdr(frames[i], &info);
                char item[160];
                std::snprintf(item, sizeof(item), " <- %s+0x%lx", info.dli_sname ? info.dli_sname : "?", info.dli_saddr ? static_cast<unsigned long>(static_cast<const char*>(frames[i]) - static_cast<const char*>(info.dli_saddr)) : 0ul);
                chain += item;
            }
            std::fprintf(stderr, "[wait]%s\n", chain.c_str());
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
    const auto commands = Begin(context);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    recording.hasBarrier = true;
}

}

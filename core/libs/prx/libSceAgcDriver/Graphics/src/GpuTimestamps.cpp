#include "prx/libSceAgcDriver/Graphics/include/GpuTimestamps.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <tuple>
#include <unordered_map>

namespace AgcDriver::Graphics {

namespace {

constexpr auto ReportInterval = std::chrono::seconds(5);
constexpr std::size_t ReportedEntries = 20;

struct Recorder {
    std::mutex mutex;
    GpuTimeReport report;
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    std::unordered_map<std::uint64_t, std::string> passNames;
    std::unordered_map<std::uint64_t, std::string> programNames;
};

Recorder& recorder() {
    static auto* value = new Recorder;
    return *value;
}

const char* passName(Recorder& state, const std::array<std::uint32_t, 3>& size) {
    auto& name = state.passNames[(static_cast<std::uint64_t>(size[0]) << 32u) | size[1]];
    if (name.empty()) name = std::to_string(size[0]) + "x" + std::to_string(size[1]);
    return name.c_str();
}

const char* programName(Recorder& state, std::uint64_t address) {
    auto& name = state.programNames[address];
    if (name.empty()) {
        char text[24];
        std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(address));
        name = text;
    }
    return name.c_str();
}

void reportIfDue(Recorder& state) {
    const auto now = std::chrono::steady_clock::now();
    if (now - state.started < ReportInterval) return;
    const auto text = state.report.Format(std::chrono::duration_cast<std::chrono::nanoseconds>(now - state.started), ReportedEntries);
    std::fwrite(text.data(), 1, text.size(), stderr);
    std::fflush(stderr);
    state.report.Clear();
    state.started = now;
}

double milliseconds(std::chrono::nanoseconds time) {
    return std::chrono::duration<double, std::milli>(time).count();
}

}

bool TraceGpuPasses() {
    static const bool enabled = std::getenv("ANYPS5_TRACE_GPU_PASSES") != nullptr;
    return enabled;
}

std::uint32_t BatchTimestamps(const VkPhysicalDeviceLimits& limits) {
    if (!limits.timestampComputeAndGraphics) return 0;
    if (TraceGpuPasses()) return PassTimestamps;
    return FrameTiming::Enabled() ? 2u : 0u;
}

void ReportGpuBatch(const char* name, std::chrono::nanoseconds time) {
    if (auto* frame = PerformanceContext::Current()) frame->Add(frame->Get("Graphics.GpuBatch", name), std::chrono::duration_cast<FrameTiming::Clock::duration>(time));
    if (!TraceGpuPasses()) return;
    auto& state = recorder();
    std::lock_guard lock(state.mutex);
    state.report.AddBatch(name, time);
    reportIfDue(state);
}

void ReportGpuRegions(std::span<const GpuRegion> regions, std::uint32_t dropped, std::chrono::nanoseconds batch) {
    if (!TraceGpuPasses() || (regions.empty() && dropped == 0 && batch.count() == 0)) return;
    auto& state = recorder();
    std::lock_guard lock(state.mutex);
    if (auto* frame = PerformanceContext::Current()) {
        for (const auto& region : regions) {
            const auto time = std::chrono::duration_cast<FrameTiming::Clock::duration>(region.time);
            if (region.work == GpuWork::Dispatch) frame->Add(frame->Get("Graphics.GpuProgram", programName(state, region.address)), time);
            else frame->Add(frame->Get("Graphics.GpuPass", passName(state, region.size)), time);
        }
    }
    state.report.AddRegions(regions, dropped, batch);
    reportIfDue(state);
}

GpuTimestamps::GpuTimestamps(const Context& context, std::uint32_t queries) : device(context.device), period(context.limits.timestampPeriod), capacity(queries < 2 ? 0 : queries) {
    if (capacity == 0) return;
    resetQueries = context.Function<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool");
    writeTimestamp = context.Function<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp");
    queryResults = context.Function<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults");
    destroyPool = context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool");
    regions.resize((capacity - 2) / 2);
    ticks.resize(capacity);
    VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = capacity;
    Check(context.Function<PFN_vkCreateQueryPool>("vkCreateQueryPool")(device, &info, nullptr, &pool), "vkCreateQueryPool");
}

GpuTimestamps::~GpuTimestamps() {
    if (pool != VK_NULL_HANDLE) destroyPool(device, pool, nullptr);
}

void GpuTimestamps::Begin(VkCommandBuffer commands) {
    used = 0;
    open = 0;
    dropped = 0;
    regionCount = 0;
    if (pool == VK_NULL_HANDLE) return;
    resetQueries(commands, pool, 0, capacity);
    writeTimestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, 0);
    used = 2;
}

void GpuTimestamps::End(VkCommandBuffer commands) {
    if (used == 0) return;
    EndRegion(commands);
    writeTimestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, 1);
}

void GpuTimestamps::beginRegion(VkCommandBuffer commands, GpuWork work, std::uint64_t address, std::array<std::uint32_t, 3> size) {
    EndRegion(commands);
    if (used + 2 > capacity) {
        ++dropped;
        return;
    }
    regions[regionCount++] = {work, address, size, used, {}};
    writeTimestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, used);
    open = used;
    used += 2;
}

void GpuTimestamps::endRegion(VkCommandBuffer commands) {
    writeTimestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, open + 1);
    open = 0;
}

std::chrono::nanoseconds GpuTimestamps::elapsed(std::uint32_t first) const {
    const auto begin = ticks[first];
    const auto end = ticks[first + 1];
    if (end < begin) return {};
    return std::chrono::nanoseconds(static_cast<std::int64_t>(static_cast<double>(end - begin) * period));
}

std::chrono::nanoseconds GpuTimestamps::Read() {
    if (used == 0) return {};
    if (queryResults(device, pool, 0, used, used * sizeof(std::uint64_t), ticks.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT) != VK_SUCCESS) {
        regionCount = 0;
        dropped = 0;
        return {};
    }
    for (std::size_t index = 0; index < regionCount; ++index) regions[index].time = elapsed(regions[index].query);
    return elapsed(0);
}

void GpuTimeReport::Total::Add(std::chrono::nanoseconds time) {
    ++count;
    sum += time;
    maximum = std::max(maximum, time);
}

bool GpuTimeReport::Key::operator<(const Key& other) const {
    return std::tie(work, address, size) < std::tie(other.work, other.address, other.size);
}

void GpuTimeReport::AddBatch(std::string_view name, std::chrono::nanoseconds time) {
    batches[name].Add(time);
}

void GpuTimeReport::AddRegions(std::span<const GpuRegion> recorded, std::uint32_t lost, std::chrono::nanoseconds batch) {
    queued.Add(batch);
    std::chrono::nanoseconds timed{};
    for (const auto& region : recorded) {
        regions[{region.work, region.address, region.size}].Add(region.time);
        (region.work == GpuWork::Dispatch ? dispatches : passes).Add(region.time);
        timed += region.time;
    }
    if (batch > timed) untimed += batch - timed;
    dropped += lost;
}

void GpuTimeReport::Clear() {
    regions.clear();
    batches.clear();
    queued = {};
    passes = {};
    dispatches = {};
    untimed = {};
    dropped = 0;
}

std::string GpuTimeReport::formatRanked(bool dispatchesOnly, std::size_t top) const {
    std::vector<std::pair<const Key*, const Total*>> ranked;
    for (const auto& [key, total] : regions) {
        if ((key.work == GpuWork::Dispatch) == dispatchesOnly) ranked.emplace_back(&key, &total);
    }
    const auto shown = std::min(top, ranked.size());
    std::partial_sort(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(shown), ranked.end(), [](const auto& left, const auto& right) { return left.second->sum > right.second->sum; });
    std::string text;
    for (std::size_t index = 0; index < shown; ++index) {
        const auto& key = *ranked[index].first;
        const auto& total = *ranked[index].second;
        const auto average = total.count == 0 ? 0.0 : milliseconds(total.sum) / static_cast<double>(total.count);
        char line[192];
        if (dispatchesOnly) std::snprintf(line, sizeof(line), "[gpu-time] program 0x%llx groups %ux%ux%u: n=%llu total=%.3f ms avg=%.3f ms max=%.3f ms\n", static_cast<unsigned long long>(key.address), key.size[0], key.size[1], key.size[2], static_cast<unsigned long long>(total.count), milliseconds(total.sum), average, milliseconds(total.maximum));
        else std::snprintf(line, sizeof(line), "[gpu-time] %s 0x%llx %ux%u: n=%llu total=%.3f ms avg=%.3f ms max=%.3f ms\n", key.work == GpuWork::DepthPass ? "depth pass" : "pass", static_cast<unsigned long long>(key.address), key.size[0], key.size[1], static_cast<unsigned long long>(total.count), milliseconds(total.sum), average, milliseconds(total.maximum));
        text += line;
    }
    return text;
}

std::string GpuTimeReport::Format(std::chrono::nanoseconds window, std::size_t top) const {
    char line[320];
    std::snprintf(line, sizeof(line), "[gpu-time] %.1f s: batches n=%llu total=%.3f ms, passes n=%llu total=%.3f ms, dispatches n=%llu total=%.3f ms, untimed %.3f ms, dropped %llu\n", std::chrono::duration<double>(window).count(), static_cast<unsigned long long>(queued.count), milliseconds(queued.sum), static_cast<unsigned long long>(passes.count), milliseconds(passes.sum), static_cast<unsigned long long>(dispatches.count), milliseconds(dispatches.sum), milliseconds(untimed), static_cast<unsigned long long>(dropped));
    std::string text = line;
    for (const auto& [name, total] : batches) {
        std::snprintf(line, sizeof(line), "[gpu-time] one-off %.*s: n=%llu total=%.3f ms max=%.3f ms\n", static_cast<int>(name.size()), name.data(), static_cast<unsigned long long>(total.count), milliseconds(total.sum), milliseconds(total.maximum));
        text += line;
    }
    text += formatRanked(false, top);
    text += formatRanked(true, top);
    return text;
}

}

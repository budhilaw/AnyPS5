#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GPUTIMESTAMPS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GPUTIMESTAMPS_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AgcDriver::Graphics {

constexpr std::uint32_t PassTimestamps = 512;

enum class GpuWork : std::uint8_t {
    ColorPass,
    DepthPass,
    Dispatch
};

struct GpuRegion {
    GpuWork work = GpuWork::ColorPass;
    std::uint64_t address = 0;
    std::array<std::uint32_t, 3> size{};
    std::uint32_t query = 0;
    std::chrono::nanoseconds time{};
};

bool TraceGpuPasses();
std::uint32_t BatchTimestamps(const VkPhysicalDeviceLimits& limits);
void ReportGpuBatch(const char* name, std::chrono::nanoseconds time);
void ReportGpuRegions(std::span<const GpuRegion> regions, std::uint32_t dropped, std::chrono::nanoseconds batch);

class GpuTimestamps {
public:
    GpuTimestamps(const Context& context, std::uint32_t queries);
    ~GpuTimestamps();
    GpuTimestamps(const GpuTimestamps&) = delete;
    GpuTimestamps& operator=(const GpuTimestamps&) = delete;
    bool Enabled() const { return pool != VK_NULL_HANDLE; }
    void Begin(VkCommandBuffer commands);
    void End(VkCommandBuffer commands);
    void BeginRegion(VkCommandBuffer commands, GpuWork work, std::uint64_t address, std::array<std::uint32_t, 3> size) {
        if (used != 0 && !regions.empty()) beginRegion(commands, work, address, size);
    }
    void EndRegion(VkCommandBuffer commands) {
        if (open != 0) endRegion(commands);
    }
    std::chrono::nanoseconds Read();
    std::span<const GpuRegion> Regions() const { return {regions.data(), regionCount}; }
    std::uint32_t Dropped() const { return dropped; }

private:
    void beginRegion(VkCommandBuffer commands, GpuWork work, std::uint64_t address, std::array<std::uint32_t, 3> size);
    void endRegion(VkCommandBuffer commands);
    std::chrono::nanoseconds elapsed(std::uint32_t first) const;
    VkDevice device = VK_NULL_HANDLE;
    VkQueryPool pool = VK_NULL_HANDLE;
    float period = 1.0f;
    std::uint32_t capacity = 0;
    std::uint32_t used = 0;
    std::uint32_t open = 0;
    std::uint32_t dropped = 0;
    std::size_t regionCount = 0;
    std::vector<GpuRegion> regions;
    std::vector<std::uint64_t> ticks;
    PFN_vkCmdResetQueryPool resetQueries = nullptr;
    PFN_vkCmdWriteTimestamp writeTimestamp = nullptr;
    PFN_vkGetQueryPoolResults queryResults = nullptr;
    PFN_vkDestroyQueryPool destroyPool = nullptr;
};

class GpuTimeReport {
public:
    void AddBatch(std::string_view name, std::chrono::nanoseconds time);
    void AddRegions(std::span<const GpuRegion> recorded, std::uint32_t lost, std::chrono::nanoseconds batch);
    std::string Format(std::chrono::nanoseconds window, std::size_t top) const;
    void Clear();

private:
    struct Total {
        std::uint64_t count = 0;
        std::chrono::nanoseconds sum{};
        std::chrono::nanoseconds maximum{};
        void Add(std::chrono::nanoseconds time);
    };
    struct Key {
        GpuWork work;
        std::uint64_t address;
        std::array<std::uint32_t, 3> size;
        bool operator<(const Key& other) const;
    };
    std::string formatRanked(bool dispatches, std::size_t top) const;
    std::map<Key, Total> regions;
    std::map<std::string_view, Total> batches;
    Total queued;
    Total passes;
    Total dispatches;
    std::chrono::nanoseconds untimed{};
    std::uint64_t dropped = 0;
};

}

#endif

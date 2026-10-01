#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_FRAMEPACING_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_FRAMEPACING_HPP

#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

enum class DisplayProfile { Hz60, Hz120, Vrr };

inline constexpr double MaximumTestedFrameRate = 120.0;
inline constexpr std::uint32_t AssumedRefreshRate = 60;
inline constexpr double FifoRefreshFraction = 0.97;
inline constexpr std::array<VkPresentModeKHR, 4> RequestedPresentModes{VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR};

struct FramePacingSettings {
    DisplayProfile display = DisplayProfile::Hz60;
    bool uncapped = false;
    double fpsLimit = -1.0;
    std::optional<AgcDriver::PresentModeRequest> presentMode;
};

struct FlipGateInput {
    int flipMode = 0;
    int flipRate = 0;
    bool uncapped = false;
    bool vrr = false;
    bool pegged = false;
    std::uint64_t lastLatchVblank = 0;
    std::chrono::steady_clock::time_point lastLatchTime{};
    std::chrono::steady_clock::time_point lastLimitSlot{};
    std::chrono::steady_clock::time_point now{};
    std::chrono::steady_clock::duration vblankPeriod{};
    std::chrono::steady_clock::duration limitInterval{};
};

struct FlipGate {
    bool byVblank = false;
    std::uint64_t targetVblank = 0;
    std::chrono::steady_clock::time_point notBefore{};
};

FramePacingSettings ParseFramePacing(const char* display, const char* uncapped, const char* fpsLimit, const char* presentMode);
const FramePacingSettings& GetFramePacing();
std::chrono::steady_clock::duration VblankPeriod(std::uint64_t outputMode);
AgcDriver::PresentModeRequest ResolvePresentMode(const FramePacingSettings& settings, bool uncapped, bool immediateFlips, std::uint32_t refreshRate);
std::chrono::steady_clock::duration UncappedLimitInterval(double fpsLimit, std::uint32_t refreshRate, std::chrono::steady_clock::duration vblankPeriod);
std::chrono::steady_clock::duration RefreshHoldInterval(std::uint32_t refreshRate, VkPresentModeKHR hostPresentMode);
FlipGate ComputeFlipGate(const FlipGateInput& input);
std::chrono::steady_clock::time_point LimitSlot(const FlipGate& gate, std::chrono::steady_clock::time_point latch);

#endif

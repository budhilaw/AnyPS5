#include "prx/libSceVideoOut/include/FramePacing.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

bool given(const char* value) {
    return value != nullptr && *value != '\0';
}

[[noreturn]] void reject(const char* variable, const char* value, const char* accepted) {
    throw std::runtime_error(std::string("VideoOut: ") + variable + "=" + value + " is not supported; use " + accepted);
}

const char* displayName(DisplayProfile display) {
    switch (display) {
        case DisplayProfile::Hz120: return "120hz";
        case DisplayProfile::Vrr: return "vrr";
        default: return "60hz";
    }
}

std::chrono::steady_clock::duration framePeriod(double rate) {
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0 / rate));
}

FramePacingSettings readFramePacing() {
    const auto settings = ParseFramePacing(std::getenv("ANYPS5_DISPLAY"), std::getenv("ANYPS5_UNCAPPED"), std::getenv("ANYPS5_FPS_LIMIT"), std::getenv("ANYPS5_PRESENT_MODE"));
    char limit[32] = "monitor refresh up to 120 fps";
    if (settings.fpsLimit == 0.0) std::snprintf(limit, sizeof(limit), "none");
    else if (settings.fpsLimit > 0.0) std::snprintf(limit, sizeof(limit), "%g fps", settings.fpsLimit);
    APS5_LOG_OUT("frame pacing: display %s%s, %s, limit %s, present %s", displayName(settings.display), settings.display == DisplayProfile::Vrr ? " (VRR capable and active for the whole run)" : "", settings.uncapped ? "uncapped" : "PS5 pacing", limit, settings.presentMode ? AgcDriver::PresentModeRequestName(*settings.presentMode) : "auto");
    return settings;
}

}

FramePacingSettings ParseFramePacing(const char* display, const char* uncapped, const char* fpsLimit, const char* presentMode) {
    FramePacingSettings settings;
    if (given(display)) {
        const std::string_view text(display);
        if (text == "60hz") settings.display = DisplayProfile::Hz60;
        else if (text == "120hz") settings.display = DisplayProfile::Hz120;
        else if (text == "vrr") settings.display = DisplayProfile::Vrr;
        else reject("ANYPS5_DISPLAY", display, "60hz, 120hz or vrr");
    }
    if (given(uncapped)) {
        const std::string_view text(uncapped);
        if (text != "0" && text != "1") reject("ANYPS5_UNCAPPED", uncapped, "0 or 1");
        settings.uncapped = text == "1";
    }
    if (given(fpsLimit)) {
        char* end = nullptr;
        const double limit = std::strtod(fpsLimit, &end);
        if (end == fpsLimit || *end != '\0' || !std::isfinite(limit) || (limit != 0.0 && limit < 1.0)) reject("ANYPS5_FPS_LIMIT", fpsLimit, "0 for no limit or a frame rate of at least 1");
        settings.fpsLimit = limit;
    }
    if (given(presentMode)) {
        const std::string_view text(presentMode);
        if (text == "fifo") settings.presentMode = AgcDriver::PresentModeRequest::Fifo;
        else if (text == "mailbox") settings.presentMode = AgcDriver::PresentModeRequest::Mailbox;
        else if (text == "immediate") settings.presentMode = AgcDriver::PresentModeRequest::Immediate;
        else if (text == "relaxed") settings.presentMode = AgcDriver::PresentModeRequest::Relaxed;
        else if (text != "auto") reject("ANYPS5_PRESENT_MODE", presentMode, "auto, fifo, mailbox, immediate or relaxed");
    }
    return settings;
}

const FramePacingSettings& GetFramePacing() {
    static const FramePacingSettings settings = readFramePacing();
    return settings;
}

std::chrono::steady_clock::duration VblankPeriod(std::uint64_t outputMode) {
    const auto ticks = outputMode == VIDEO_OUT_OUTPUT_MODE_119_88HZ ? VblankTicksAt119_88Hz : VblankTicksAt59_94Hz;
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(VblankTick(static_cast<std::int64_t>(ticks)));
}

AgcDriver::PresentModeRequest ResolvePresentMode(const FramePacingSettings& settings, bool uncapped, bool immediateFlips, std::uint32_t refreshRate) {
    if (settings.presentMode) return *settings.presentMode;
    if (uncapped || immediateFlips) return AgcDriver::PresentModeRequest::Mailbox;
    const std::uint32_t topRate = settings.display == DisplayProfile::Hz60 ? 60 : 120;
    const std::uint32_t refresh = refreshRate != 0 ? refreshRate : AssumedRefreshRate;
    return refresh + 1 >= 2 * topRate ? AgcDriver::PresentModeRequest::Fifo : AgcDriver::PresentModeRequest::Mailbox;
}

std::chrono::steady_clock::duration UncappedLimitInterval(double fpsLimit, std::uint32_t refreshRate, std::chrono::steady_clock::duration vblankPeriod) {
    if (fpsLimit == 0.0) return {};
    if (fpsLimit > 0.0) return framePeriod(fpsLimit);
    const double rate = refreshRate != 0 ? std::min(static_cast<double>(refreshRate), MaximumTestedFrameRate) : MaximumTestedFrameRate;
    return std::min(framePeriod(rate), vblankPeriod);
}

std::chrono::steady_clock::duration RefreshHoldInterval(std::uint32_t refreshRate, VkPresentModeKHR hostPresentMode) {
    if (hostPresentMode != VK_PRESENT_MODE_FIFO_KHR && hostPresentMode != VK_PRESENT_MODE_FIFO_RELAXED_KHR) return {};
    return framePeriod(FifoRefreshFraction * static_cast<double>(refreshRate != 0 ? refreshRate : AssumedRefreshRate));
}

FlipGate ComputeFlipGate(const FlipGateInput& input) {
    if (input.flipMode == VIDEO_OUT_FLIP_MODE_HSYNC) return {};
    const bool firstFlip = input.lastLatchTime == std::chrono::steady_clock::time_point{};
    const auto interval = static_cast<std::uint64_t>(input.flipRate) + 1;
    if (input.uncapped && !input.pegged) {
        if (firstFlip || input.limitInterval <= std::chrono::steady_clock::duration::zero()) return {};
        return {false, 0, std::max(input.lastLimitSlot + input.limitInterval, input.now - input.limitInterval)};
    }
    if (input.vrr && !input.pegged) {
        if (firstFlip) return {};
        return {false, 0, input.lastLatchTime + input.vblankPeriod * static_cast<std::chrono::steady_clock::rep>(interval)};
    }
    return {true, input.lastLatchVblank + interval, {}};
}

std::chrono::steady_clock::time_point LimitSlot(const FlipGate& gate, std::chrono::steady_clock::time_point latch) {
    return gate.notBefore == std::chrono::steady_clock::time_point{} ? latch : gate.notBefore;
}

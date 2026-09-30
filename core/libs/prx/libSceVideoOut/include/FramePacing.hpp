#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_FRAMEPACING_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_FRAMEPACING_HPP

#include <chrono>
#include <cstdint>

enum class DisplayProfile { Hz60, Hz120, Vrr };

struct FramePacingSettings {
    DisplayProfile display = DisplayProfile::Hz60;
    bool uncapped = false;
    double fpsLimit = -1.0;
};

struct FlipGateInput {
    int flipMode = 0;
    int flipRate = 0;
    bool uncapped = false;
    bool vrr = false;
    bool pegged = false;
    std::uint64_t lastLatchVblank = 0;
    std::chrono::steady_clock::time_point lastLatchTime{};
    std::chrono::steady_clock::duration vblankPeriod{};
    std::chrono::steady_clock::duration limitInterval{};
};

struct FlipGate {
    bool byVblank = false;
    std::uint64_t targetVblank = 0;
    std::chrono::steady_clock::time_point notBefore{};
};

FramePacingSettings ParseFramePacing(const char* display, const char* uncapped, const char* fpsLimit);
const FramePacingSettings& GetFramePacing();
std::chrono::steady_clock::duration VblankPeriod(std::uint64_t outputMode);
FlipGate ComputeFlipGate(const FlipGateInput& input);

#endif

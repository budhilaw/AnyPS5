#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "prx/libSceVideoOut/include/FramePacing.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

#undef main

extern "C" int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
extern "C" int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);

namespace {

void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

template<typename TAction>
std::string expectFailure(TAction action) {
    try { action(); }
    catch (const std::runtime_error& error) { return error.what(); }
    throw std::runtime_error("expected an exception");
}

void setEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    check(_putenv_s(name, value != nullptr ? value : "") == 0, "environment update failed");
#else
    check((value != nullptr ? setenv(name, value, 1) : unsetenv(name)) == 0, "environment update failed");
#endif
}

struct PacingEnvironment {
    const char* display = nullptr;
    const char* uncapped = nullptr;
    const char* fpsLimit = nullptr;
};

void usePacingEnvironment(const PacingEnvironment& environment) {
    setEnvironment("ANYPS5_DISPLAY", environment.display);
    setEnvironment("ANYPS5_UNCAPPED", environment.uncapped);
    setEnvironment("ANYPS5_FPS_LIMIT", environment.fpsLimit);
}

class Gate final : public AgcDriver::IVideoOutput, public AgcDriver::IFlipRequest, public std::enable_shared_from_this<Gate> {
public:
    std::mutex mutex;
    std::condition_variable changed;
    bool released = false;
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override { return shared_from_this(); }
    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>&) override {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, std::chrono::seconds(10), [&] { return released; })) throw std::runtime_error("test gate timed out");
    }
    void Fail(std::exception_ptr error) noexcept override { if (!error) std::terminate(); }
    void Release() {
        std::lock_guard lock(mutex);
        released = true;
        changed.notify_all();
    }
};

std::span<std::byte> alignedBuffer(std::vector<std::byte>& allocation) {
    const auto address = reinterpret_cast<uintptr_t>(allocation.data());
    const auto offset = (65536u - (address & 65535u)) & 65535u;
    return {allocation.data() + offset, allocation.size() - 65535u};
}

void testLifetime(bool reopen) {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    auto gate = std::make_shared<Gate>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, gate);
    std::array<uint32_t, 6> words{0xc004105c, 7, 0xfffffffeu, 1, 0, 0};
    Packet packet{words.data(), 6, 0, {}};
    sceAgcDriverSubmitDcb(&packet);
    std::vector<std::byte> allocation(65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 64;
    attribute.height = 64;
    attribute.pixel_format = 0x8000000000000000ull;
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    sceVideoOutSubmitFlip(handle, 0, VIDEO_OUT_FLIP_MODE_HSYNC, -9);
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->flipStatus.flipPendingNum == 1 && cfg->flipStatus.count == 0, "reservation status is wrong");
    }
    attribute.dcc_control = 1;
    check(expectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute, nullptr); }).find("pending flip") != std::string::npos, "pending attributes were changed");
    check(expectFailure([&] { sceVideoOutUnregisterBuffers(handle, 0); }).find("pending flip") != std::string::npos, "pending buffer was unregistered");
    check(sceVideoOutWaitVblank(handle) == 0 && sceVideoOutWaitVblank(handle) == 0, "vblank stalled behind the GPU queue");
    std::shared_ptr<VideoOutConfig> replacement;
    sceVideoOutClose(handle);
    if (reopen) {
        check(sceVideoOutOpen(255, 0, 0, nullptr) == handle, "reopen changed handle");
        replacement = VideoOutDriver::Get().GetConfig(handle);
        check(replacement != cfg && replacement->generation > cfg->generation, "reopen reused old port state");
    }
    gate->Release();
    std::exception_ptr failure;
    {
        std::unique_lock lock(cfg->mutex);
        check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(10), [&] { return cfg->failure != nullptr; }), "flip failure did not wake waiters");
        failure = cfg->failure;
        check(cfg->flipStatus.count == 0 && cfg->flipStatus.flipPendingNum == 0, "failed flip has successful or pending status");
    }
    const auto message = expectFailure([&] { std::rethrow_exception(failure); });
    check(message.find("closed") != std::string::npos, "flip used a closed port");
    if (replacement) {
        std::lock_guard lock(replacement->mutex);
        check(replacement->flipStatus.flipPendingNum == 0 && replacement->flipStatus.count == 0, "old request changed new port counters");
    }
    check(expectFailure([&] { sceVideoOutWaitVblank(handle); }).find("closed") != std::string::npos, "VideoOut lost worker failure");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, gate);
    if (reopen) sceVideoOutClose(handle);
    const auto shutdown = expectFailure([] { LibcRunShutdown_nid_postfix(); });
    check(shutdown.find("closed") != std::string::npos, "shutdown lost asynchronous error");
}

std::size_t tiledOffset(uint32_t x, uint32_t y, uint32_t width) {
    constexpr std::array<uint32_t, 7> xMasks{4, 8, 128, 256, 0x2200, 0x800, 0x8400};
    constexpr std::array<uint32_t, 7> yMasks{16, 32, 64, 0x1100, 0x200, 0x400, 0x4800};
    uint32_t offset = 0;
    for (uint32_t bit = 0; bit < 7; ++bit) {
        if ((x & (1u << bit)) != 0) offset ^= xMasks[bit];
        if ((y & (1u << bit)) != 0) offset ^= yMasks[bit];
    }
    return (static_cast<std::size_t>(y / 128u) * ((width + 127u) / 128u) + x / 128u) * 65536u + offset;
}

void fillBuffer(std::span<std::byte> bytes, uint32_t width, uint32_t height) {
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const auto offset = tiledOffset(x, y, width);
            bytes[offset] = static_cast<std::byte>(x & 255u);
            bytes[offset + 1] = static_cast<std::byte>(y & 255u);
            bytes[offset + 2] = static_cast<std::byte>((x ^ y) & 255u);
            bytes[offset + 3] = std::byte{255};
        }
    }
}

void testDecode() {
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto tiled = alignedBuffer(allocation);
    AgcDriver::DisplayBuffer buffer{reinterpret_cast<uint64_t>(tiled.data()), 0x8000000000000000ull, 259, 137};
    check(AgcDriver::DisplayBufferSize(buffer) == tiled.size(), "incorrect padded display footprint");
    fillBuffer(tiled, buffer.width, buffer.height);
    for (const auto format : {0x8000000000000000ull, 0x8000000022000000ull}) {
        buffer.pixelFormat = format;
        const auto pixels = AgcDriver::ReadDisplayBuffer(buffer);
        for (uint32_t y = 0; y < buffer.height; ++y) {
            for (uint32_t x = 0; x < buffer.width; ++x) {
                const auto offset = (static_cast<std::size_t>(y) * buffer.width + x) * 4;
                const auto blue = format == 0x8000000000000000ull ? x : x ^ y;
                const auto red = format == 0x8000000000000000ull ? x ^ y : x;
                check(pixels[offset] == static_cast<std::byte>(blue & 255u) && pixels[offset + 1] == static_cast<std::byte>(y & 255u) && pixels[offset + 2] == static_cast<std::byte>(red & 255u) && pixels[offset + 3] == std::byte{255}, "detiled pixel or channel order mismatch");
            }
        }
    }
    expectFailure([&] { AgcDriver::DecodeDisplayBuffer(buffer, std::span(tiled).first(tiled.size() - 1)); });
    buffer.pixelFormat = 0;
    expectFailure([&] { AgcDriver::DisplayBufferSize(buffer); });
    buffer.pixelFormat = 0x8000000000000000ull;
    ++buffer.address;
    expectFailure([&] { AgcDriver::ReadDisplayBuffer(buffer); });
    buffer.address = std::numeric_limits<uint64_t>::max() & ~uint64_t{65535};
    expectFailure([&] { AgcDriver::DisplayBufferSize(buffer); });
}

std::chrono::steady_clock::duration vblankTicks(std::uint64_t count) {
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(VblankTick(static_cast<std::int64_t>(count)));
}

std::uint64_t pacedFlipVblanks(int flips, std::chrono::steady_clock::duration present) {
    VblankClock clock(std::chrono::steady_clock::time_point{});
    auto now = std::chrono::steady_clock::time_point{};
    std::uint64_t vblank = 0;
    std::uint64_t lastFlipVblank = 0;
    std::uint64_t firstLatch = 0;
    for (int flip = 0; flip < flips; ++flip) {
        while (vblank < lastFlipVblank + 1) {
            now = std::max(now, clock.Deadline());
            vblank += clock.Advance(now);
        }
        const auto latch = vblank;
        if (flip == 0) firstLatch = latch;
        now += present;
        while (clock.Deadline() <= now) vblank += clock.Advance(clock.Deadline());
        lastFlipVblank = latch;
    }
    return lastFlipVblank - firstLatch;
}

void checkRejected(const char* display, const char* uncapped, const char* fpsLimit, const char* variable, const char* accepted) {
    const auto message = expectFailure([&] { ParseFramePacing(display, uncapped, fpsLimit); });
    check(message.find(variable) != std::string::npos && message.find(accepted) != std::string::npos, "a rejected frame pacing value did not name its variable and the accepted values");
}

void checkFramePacingSettings() {
    const std::array<const char*, 2> unset{nullptr, ""};
    for (const char* value : unset) {
        const auto defaults = ParseFramePacing(value, value, value);
        check(defaults.display == DisplayProfile::Hz60 && !defaults.uncapped && defaults.fpsLimit < 0.0, "unset frame pacing variables did not select the PS5 60 Hz defaults");
    }
    check(ParseFramePacing("60hz", nullptr, nullptr).display == DisplayProfile::Hz60 && ParseFramePacing("120hz", nullptr, nullptr).display == DisplayProfile::Hz120 && ParseFramePacing("vrr", nullptr, nullptr).display == DisplayProfile::Vrr, "ANYPS5_DISPLAY was not parsed");
    check(!ParseFramePacing(nullptr, "0", nullptr).uncapped && ParseFramePacing(nullptr, "1", nullptr).uncapped, "ANYPS5_UNCAPPED was not parsed");
    check(ParseFramePacing(nullptr, nullptr, "0").fpsLimit == 0.0 && ParseFramePacing(nullptr, nullptr, "1").fpsLimit == 1.0 && ParseFramePacing(nullptr, nullptr, "144").fpsLimit == 144.0 && ParseFramePacing(nullptr, nullptr, "59.94").fpsLimit == 59.94, "ANYPS5_FPS_LIMIT was not parsed");
    for (const char* value : {"120", "120Hz", "VRR", "144hz", "vrr "}) checkRejected(value, nullptr, nullptr, "ANYPS5_DISPLAY", "60hz, 120hz or vrr");
    for (const char* value : {"2", "true", "yes", "01", "-1"}) checkRejected(nullptr, value, nullptr, "ANYPS5_UNCAPPED", "0 or 1");
    for (const char* value : {"-1", "-0.5", "0.5", "0.999", "fast", "60fps", "nan", "inf", "1e999"}) checkRejected(nullptr, nullptr, value, "ANYPS5_FPS_LIMIT", "0 for no limit or a frame rate of at least 1");
}

void checkUncappedLimits() {
    using Duration = std::chrono::steady_clock::duration;
    const auto perFrame = [](double rate) { return std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1.0 / rate)); };
    const auto slow = VblankPeriod(VIDEO_OUT_OUTPUT_MODE_DEFAULT);
    const auto fast = VblankPeriod(VIDEO_OUT_OUTPUT_MODE_119_88HZ);
    check(UncappedLimitInterval(-1.0, 255, slow) == perFrame(120.0) && UncappedLimitInterval(-1.0, 144, slow) == perFrame(120.0) && UncappedLimitInterval(-1.0, 144, fast) == perFrame(120.0), "the default uncapped limit rose above 120 fps on a faster monitor");
    check(UncappedLimitInterval(-1.0, 0, slow) == perFrame(120.0) && UncappedLimitInterval(-1.0, 0, fast) == perFrame(120.0), "an unknown monitor refresh rate did not set the default uncapped limit to 120 fps");
    check(UncappedLimitInterval(-1.0, 100, slow) == perFrame(100.0) && UncappedLimitInterval(-1.0, 60, slow) == perFrame(60.0), "the default uncapped limit did not follow a monitor refresh rate below 120 Hz");
    check(UncappedLimitInterval(-1.0, 59, slow) == slow && UncappedLimitInterval(-1.0, 60, fast) == fast && UncappedLimitInterval(-1.0, 100, fast) == fast, "the default uncapped limit fell below the emulated vblank rate");
    check(UncappedLimitInterval(144.0, 60, slow) == perFrame(144.0) && UncappedLimitInterval(30.0, 255, fast) == perFrame(30.0) && UncappedLimitInterval(1.0, 0, slow) == std::chrono::seconds(1), "an explicit ANYPS5_FPS_LIMIT was not used as the uncapped limit");
    check(UncappedLimitInterval(0.0, 144, slow) == Duration::zero(), "ANYPS5_FPS_LIMIT=0 did not remove the uncapped limit");
}

template<typename TFrame>
std::vector<std::chrono::steady_clock::time_point> uncappedLatches(std::chrono::steady_clock::duration interval, int flips, TFrame frame) {
    std::vector<std::chrono::steady_clock::time_point> latches;
    FlipGateInput input{.flipMode = VIDEO_OUT_FLIP_MODE_VSYNC, .uncapped = true, .limitInterval = interval};
    auto now = std::chrono::steady_clock::time_point{} + std::chrono::seconds(1);
    for (int flip = 0; flip < flips; ++flip) {
        const auto [render, present] = frame(flip);
        now += render;
        input.now = now;
        const auto gate = ComputeFlipGate(input);
        now = std::max(now, gate.notBefore);
        latches.push_back(now);
        input.lastLatchTime = now;
        input.lastLimitSlot = LimitSlot(gate, now);
        now += present;
    }
    return latches;
}

void checkUncappedTimeline() {
    using Duration = std::chrono::steady_clock::duration;
    using Frame = std::pair<Duration, Duration>;
    const auto interval = std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1.0 / 120.0));
    const auto spacedBy = [](const std::vector<std::chrono::steady_clock::time_point>& latches, std::size_t from, Duration spacing) {
        for (std::size_t index = from + 1; index < latches.size(); ++index) {
            if (latches[index] - latches[index - 1] != spacing) return false;
        }
        return true;
    };
    const auto steady = uncappedLatches(interval, 121, [](int) { return Frame{Duration{}, std::chrono::milliseconds(3)}; });
    check(spacedBy(steady, 0, interval), "the uncapped limit added the present time to the frame interval");
    const auto jittered = uncappedLatches(interval, 121, [](int flip) { return Frame{Duration{}, std::chrono::microseconds(flip % 2 == 0 ? 1900 : 3900)}; });
    check(spacedBy(jittered, 0, interval), "a present time varying between 1.9 and 3.9 ms moved uncapped flips off the limit timeline");
    const auto uneven = uncappedLatches(interval, 121, [](int flip) { return Frame{std::chrono::milliseconds(flip % 2 == 0 ? 1 : 9), std::chrono::milliseconds(3)}; });
    check(uneven.back() - uneven.front() == interval * 120, "uneven frame times that average below the limit did not reach the uncapped limit on average");
    const auto slow = uncappedLatches(interval, 121, [](int) { return Frame{Duration{}, std::chrono::milliseconds(10)}; });
    check(spacedBy(slow, 0, std::chrono::milliseconds(10)), "the uncapped limit delayed flips whose present is slower than the limit");
    const auto stalled = uncappedLatches(interval, 20, [](int flip) { return Frame{Duration{}, std::chrono::milliseconds(flip == 10 ? 100 : 3)}; });
    const auto resumed = stalled[10] + std::chrono::milliseconds(100);
    check(stalled[11] == resumed && stalled[12] == resumed + std::chrono::milliseconds(3) && stalled[13] == resumed + interval && spacedBy(stalled, 13, interval), "after a stalled present the uncapped limit released more than one interval of catch-up, or did not resume its timeline");
    const auto gapped = uncappedLatches(interval, 20, [](int flip) { return Frame{std::chrono::milliseconds(flip == 10 ? 100 : 0), std::chrono::milliseconds(3)}; });
    const auto submitted = gapped[9] + std::chrono::milliseconds(103);
    check(gapped[10] == submitted && gapped[11] == submitted + std::chrono::milliseconds(3) && gapped[12] == submitted + interval && spacedBy(gapped, 12, interval), "after a gap between flips the uncapped limit released more than one interval of catch-up, or did not resume its timeline");
}

void checkFlipGates() {
    using Period59 = std::chrono::duration<std::int64_t, std::ratio<1001, 60000>>;
    using Period119 = std::chrono::duration<std::int64_t, std::ratio<1001, 120000>>;
    const auto period = VblankPeriod(VIDEO_OUT_OUTPUT_MODE_119_88HZ);
    check(VblankPeriod(VIDEO_OUT_OUTPUT_MODE_DEFAULT) == std::chrono::duration_cast<std::chrono::steady_clock::duration>(Period59(1)) && period == std::chrono::duration_cast<std::chrono::steady_clock::duration>(Period119(1)), "the vblank period is not 1001/60000 s at 59.94 Hz and 1001/120000 s at 119.88 Hz");
    const auto latch = std::chrono::steady_clock::time_point{} + std::chrono::seconds(100);
    const FlipGateInput base{.flipMode = VIDEO_OUT_FLIP_MODE_VSYNC, .flipRate = 2, .lastLatchVblank = 1000, .lastLatchTime = latch, .lastLimitSlot = latch, .now = latch + std::chrono::milliseconds(3), .vblankPeriod = period, .limitInterval = std::chrono::milliseconds(7)};
    const auto onGrid = [](const FlipGate& gate, std::uint64_t target) { return gate.byVblank && gate.targetVblank == target; };
    const auto notBefore = [](const FlipGate& gate, std::chrono::steady_clock::time_point time) { return !gate.byVblank && gate.notBefore == time; };
    const auto immediate = [](const FlipGate& gate) { return !gate.byVblank && gate.notBefore == std::chrono::steady_clock::time_point{}; };
    auto input = base;
    check(onGrid(ComputeFlipGate(input), 1003), "a capped flip did not wait until flip rate + 1 vblanks after the latched vblank");
    for (const bool uncapped : {false, true}) {
        for (const bool vrr : {false, true}) {
            for (const bool pegged : {false, true}) {
                input = base;
                input.flipMode = VIDEO_OUT_FLIP_MODE_HSYNC;
                input.uncapped = uncapped;
                input.vrr = vrr;
                input.pegged = pegged;
                check(immediate(ComputeFlipGate(input)), "an immediate flip waited");
            }
        }
    }
    input = base;
    input.vrr = true;
    check(notBefore(ComputeFlipGate(input), latch + period * 3), "an unpegged VRR flip at flip rate 2 did not wait three vblank periods after the previous latch");
    input.flipRate = 0;
    check(notBefore(ComputeFlipGate(input), latch + period), "an unpegged VRR flip at flip rate 0 did not wait one vblank period after the previous latch");
    input.pegged = true;
    check(onGrid(ComputeFlipGate(input), 1001), "a pegged VRR flip left the vblank grid");
    input.lastLatchTime = {};
    input.pegged = false;
    check(immediate(ComputeFlipGate(input)), "the first unpegged VRR flip waited");
    input = base;
    input.uncapped = true;
    input.vrr = true;
    check(notBefore(ComputeFlipGate(input), latch + std::chrono::milliseconds(7)), "an uncapped flip did not wait for the frame rate limit alone");
    input.lastLimitSlot = latch - std::chrono::milliseconds(2);
    check(notBefore(ComputeFlipGate(input), latch + std::chrono::milliseconds(5)), "an uncapped flip was not scheduled one limit interval after the previous flip's slot on the limit timeline");
    input.now = latch + std::chrono::milliseconds(30);
    check(notBefore(ComputeFlipGate(input), latch + std::chrono::milliseconds(23)), "an uncapped flip that reached its gate late was scheduled more than one limit interval before it reached the gate");
    input.lastLimitSlot = latch;
    input.now = latch + std::chrono::milliseconds(3);
    input.pegged = true;
    check(onGrid(ComputeFlipGate(input), 1003), "a pegged uncapped flip left the vblank grid");
    input.pegged = false;
    input.limitInterval = {};
    check(immediate(ComputeFlipGate(input)), "an uncapped flip without a frame rate limit waited");
    input.limitInterval = std::chrono::milliseconds(7);
    input.lastLatchTime = {};
    check(immediate(ComputeFlipGate(input)), "the first uncapped flip waited");
    const auto scheduled = latch - std::chrono::milliseconds(4);
    check(LimitSlot(FlipGate{true, 1003, {}}, latch) == latch && LimitSlot(FlipGate{}, latch) == latch && LimitSlot(FlipGate{false, 0, scheduled}, latch) == scheduled, "a flip's limit slot is not the time its gate scheduled it, or its latch when the gate scheduled no time");
}

void testPacing() {
    constexpr std::uint64_t slow = VblankTicksAt59_94Hz;
    constexpr std::uint64_t fast = VblankTicksAt119_88Hz;
    constexpr std::uint64_t onTime = 60000;
    const std::chrono::steady_clock::time_point start{};
    VblankClock clock(start);
    check(clock.Deadline() == start + vblankTicks(slow), "the first vblank is not one 59.94 Hz period after the start");
    for (std::uint64_t vblank = 0; vblank < onTime; ++vblank) check(clock.Advance(clock.Deadline()) == 1, "an on-time wake did not advance the vblank count by one");
    check(clock.Deadline() == start + vblankTicks(slow * (onTime + 1)), "the 59.94 Hz vblank grid drifted");
    check(clock.Advance(clock.Deadline() + vblankTicks(slow * 5 / 2)) == 3, "a wake two and a half periods late did not catch up in one step");
    check(clock.Deadline() == start + vblankTicks(slow * (onTime + 4)), "a late wake moved the vblank grid");
    check(clock.Advance(clock.Deadline() - std::chrono::milliseconds(1)) == 1 && clock.Deadline() == start + vblankTicks(slow * (onTime + 5)), "an early wake moved the vblank grid");
    const auto switched = start + vblankTicks(slow * (onTime + 4));
    clock.SetTicksPerVblank(fast);
    check(clock.Deadline() == switched + vblankTicks(fast), "119.88 Hz did not continue from the last vblank");
    for (std::uint64_t vblank = 0; vblank < onTime; ++vblank) check(clock.Advance(clock.Deadline()) == 1, "an on-time 119.88 Hz wake did not advance the vblank count by one");
    check(clock.Deadline() == switched + vblankTicks(fast * (onTime + 1)), "the 119.88 Hz vblank grid drifted");
    clock.SetTicksPerVblank(fast);
    check(clock.Deadline() == switched + vblankTicks(fast * (onTime + 1)), "keeping the rate moved the vblank grid");
    clock.SetTicksPerVblank(slow);
    check(clock.Deadline() == switched + vblankTicks(fast * onTime) + vblankTicks(slow), "59.94 Hz did not continue from the last vblank");
    const auto period = vblankTicks(VblankTicksAt59_94Hz);
    check(pacedFlipVblanks(101, period * 9 / 10) == 100, "flips whose present fits in a vblank were not shown on every vblank");
    check(pacedFlipVblanks(101, period * 5 / 4) <= 126, "a present longer than a vblank cost the next flip an extra vblank");
    checkFramePacingSettings();
    checkFlipGates();
    checkUncappedLimits();
    checkUncappedTimeline();
}

void checkVblankRate(int handle, const std::shared_ptr<VideoOutConfig>& cfg, double rate, const char* reason) {
    check(sceVideoOutWaitVblank(handle) == 0, "vblank wait failed");
    const auto sample = [&] {
        std::lock_guard lock(cfg->mutex);
        return std::pair{cfg->vblankStatus.count, std::chrono::steady_clock::now()};
    };
    const auto [firstCount, firstTime] = sample();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const auto [lastCount, lastTime] = sample();
    const auto expected = std::chrono::duration<double>(lastTime - firstTime).count() * rate;
    check(std::abs(static_cast<double>(lastCount - firstCount) - expected) <= 3.0, reason);
}

void checkVblankCatchUp(int handle, const std::shared_ptr<VideoOutConfig>& cfg, const KernelEqueueRef& events) {
    KernelEvent event{};
    std::uint64_t stalledCount = 0;
    std::chrono::steady_clock::time_point stalledAt;
    {
        std::lock_guard lock(cfg->mutex);
        while (events->GetTriggeredEvents(&event, 1) == 1) {}
        stalledCount = cfg->vblankStatus.count;
        stalledAt = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    check(sceVideoOutWaitVblank(handle) == 0 && sceVideoOutWaitVblank(handle) == 0, "vblank wait failed");
    std::uint64_t count = 0;
    std::chrono::steady_clock::time_point now;
    {
        std::lock_guard lock(cfg->mutex);
        count = cfg->vblankStatus.count;
        now = std::chrono::steady_clock::now();
    }
    const auto expected = std::chrono::duration<double>(now - stalledAt).count() * 60000.0 / 1001.0;
    check(std::abs(static_cast<double>(count - stalledCount) - expected) <= 3.0, "the vblank count did not catch up with the vblanks a stalled vblank thread missed");
    std::uint64_t previous = stalledCount;
    std::uint64_t largestStep = 0;
    while (events->GetTriggeredEvents(&event, 1) == 1) {
        int64_t signalled = 0;
        check(sceVideoOutGetEventId(&event) == VIDEO_OUT_EVENT_VBLANK && sceVideoOutGetEventData(&event, &signalled) == 0 && static_cast<std::uint64_t>(signalled) > previous, "vblank events after a stall are missing or out of order");
        largestStep = std::max(largestStep, static_cast<std::uint64_t>(signalled) - previous);
        previous = static_cast<std::uint64_t>(signalled);
    }
    check(largestStep >= 5, "a stalled vblank thread signalled the vblanks it missed one event at a time");
}

void testControls() {
    const auto handle = sceVideoOutOpen(255, 0, 0, nullptr);
    const auto cfg = VideoOutDriver::Get().GetConfig(handle);
    for (int rate = 0; rate <= 2; ++rate) {
        check(sceVideoOutSetFlipRate(handle, rate) == 0 && cfg->flipRate == rate, "flip rate was not applied");
    }
    expectFailure([&] { sceVideoOutSetFlipRate(handle, 3); });
    VideoOutColorSettings settings{};
    expectFailure([&] { sceVideoOutColorSettingsSetGamma(&settings, std::numeric_limits<float>::quiet_NaN()); });
    KernelEqueue queue = 0;
    check(sceKernelCreateEqueue(&queue, "VideoOut test") == 0, "event queue creation failed");
    auto owner = EqueuePin_nid_postfix(queue);
    check(sceVideoOutAddOutputModeEvent(queue, handle, nullptr) == 0, "output mode subscription failed");
    KernelEvent event{};
    check(owner->GetTriggeredEvents(&event, 1) == 1, "initial output mode event missing");
    int64_t mode = 0;
    check(sceVideoOutGetEventData(&event, &mode) == 0 && mode == VIDEO_OUT_OUTPUT_MODE_DEFAULT && sceVideoOutGetEventCount(&event) == 1, "initial output mode event encoding is wrong");
    check(sceVideoOutAddVrrActiveStatusEvent(queue, handle, nullptr) == 0, "VRR status subscription failed");
    int64_t vrrStatus = -1;
    check(owner->GetTriggeredEvents(&event, 1) == 1 && sceVideoOutGetEventId(&event) == VIDEO_OUT_EVENT_VRR_ACTIVE_STATUS && sceVideoOutGetEventData(&event, &vrrStatus) == 0 && vrrStatus == 0 && sceVideoOutGetEventCount(&event) == 1, "the VRR status was not signalled as inactive at registration");
    VideoOutVblankStatus vblankStatus{};
    check(sceVideoOutGetVblankStatus(handle, &vblankStatus) == 0 && vblankStatus.flags == 0, "the 60 Hz display reported an active VRR");
    check(sceVideoOutIsOutputSupported(handle, VIDEO_OUT_OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 1 && sceVideoOutIsOutputSupported(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == 0, "output mode support is wrong");
    check(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE, "unavailable 119.88 Hz output was not refused");
    VideoOutOutputStatus output{};
    check(sceVideoOutGetOutputStatus(handle, &output) == 0 && output.refreshRate == VIDEO_OUT_REFRESH_RATE_59_94HZ, "refused output mode changed the refresh rate");
    check(output.flags == 0, "the 60 Hz display reported itself VRR capable");
    check(owner->GetTriggeredEvents(&event, 1) == 0, "refused output mode triggered an event");
    check(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 0, "default output mode was refused");
    for (const int flipMode : {3, VIDEO_OUT_FLIP_MODE_VSYNC_MULTI}) {
        check(expectFailure([&] { sceVideoOutSubmitFlip(handle, VIDEO_OUT_BUFFER_INDEX_BLANK, flipMode, 0); }).find("flip mode not implemented") != std::string::npos, "unimplemented flip mode was accepted");
    }
    checkVblankRate(handle, cfg, 60000.0 / 1001.0, "the vblank count did not advance at 59.94 Hz");
    {
        std::lock_guard lock(cfg->mutex);
        cfg->outputMode = VIDEO_OUT_OUTPUT_MODE_119_88HZ;
    }
    checkVblankRate(handle, cfg, 120000.0 / 1001.0, "the vblank count did not switch to 119.88 Hz");
    {
        std::lock_guard lock(cfg->mutex);
        cfg->outputMode = VIDEO_OUT_OUTPUT_MODE_DEFAULT;
    }
    checkVblankRate(handle, cfg, 60000.0 / 1001.0, "the vblank count did not switch back to 59.94 Hz");
    check(sceVideoOutAddVblankEvent(queue, handle, nullptr) == 0 && sceVideoOutAddVblankEvent(queue, handle, &settings) == 0, "vblank subscription failed");
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->vblankEvents.size() == 1, "duplicate vblank subscription");
    }
    check(sceVideoOutWaitVblank(handle) == 0, "vblank wait failed");
    check(owner->GetTriggeredEvents(&event, 1) == 1 && event.udata == &settings && sceVideoOutGetEventId(&event) == VIDEO_OUT_EVENT_VBLANK, "vblank event or updated user data missing");
    checkVblankCatchUp(handle, cfg, owner);
    sceVideoOutClose(handle);
    check(owner->GetTriggeredEvents(&event, 1) == 0, "closed port retained pending events");
    check(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    LibcRunShutdown_nid_postfix();
}

void checkLatchedVblank(const VideoOutConfig& cfg, std::uint64_t submittedVblank, std::chrono::steady_clock::time_point submitted) {
    check(cfg.lastFlipLatch >= submitted, "flip latched before it was submitted");
    const auto vblanks = std::chrono::duration<double>(cfg.lastFlipLatch - submitted).count() * 60000.0 / 1001.0;
    check(std::abs(static_cast<double>(cfg.lastFlipVblank) - static_cast<double>(submittedVblank) - vblanks) <= 3.0, "the last flip vblank is not the vblank count at which the flip latched");
}

void checkBackToBackFlips(int handle, const std::shared_ptr<VideoOutConfig>& cfg, const KernelEqueueRef& events) {
    for (int attempt = 0; attempt < 10; ++attempt) {
        std::uint64_t completed = 0;
        std::uint64_t submittedVblank = 0;
        std::chrono::steady_clock::time_point submitted;
        {
            std::lock_guard lock(cfg->mutex);
            completed = cfg->flipStatus.count;
            submittedVblank = cfg->vblankStatus.count;
            submitted = std::chrono::steady_clock::now();
        }
        sceVideoOutSubmitFlip(handle, 0, VIDEO_OUT_FLIP_MODE_VSYNC, 1);
        sceVideoOutSubmitFlip(handle, 0, VIDEO_OUT_FLIP_MODE_VSYNC, 2);
        std::optional<std::uint64_t> firstLatch;
        std::uint64_t secondLatch = 0;
        {
            std::unique_lock lock(cfg->mutex);
            check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] {
                if (cfg->flipStatus.count == completed + 1) firstLatch = cfg->lastFlipVblank;
                return cfg->failure != nullptr || cfg->flipStatus.count == completed + 2;
            }), "back-to-back flips did not complete");
            if (cfg->failure) std::rethrow_exception(cfg->failure);
            check(cfg->flipStatus.flipPendingNum == 0 && cfg->bufferPending[0] == 0, "back-to-back flips left pending state");
            checkLatchedVblank(*cfg, submittedVblank, submitted);
            secondLatch = cfg->lastFlipVblank;
        }
        std::array<KernelEvent, 2> flipEvents{};
        std::array<int64_t, 2> arguments{};
        check(events->GetTriggeredEvents(flipEvents.data(), 2) == 2 && sceVideoOutGetEventData(&flipEvents[0], &arguments[0]) == 0 && sceVideoOutGetEventData(&flipEvents[1], &arguments[1]) == 0 && arguments[0] == 1 && arguments[1] == 2, "back-to-back flip events are missing or out of order");
        if (!firstLatch) continue;
        check(secondLatch >= *firstLatch + 1 && secondLatch <= *firstLatch + 2, "back-to-back flips at flip rate 0 did not latch on consecutive vblanks");
        return;
    }
    throw std::runtime_error("the first of two back-to-back flips was never seen complete");
}

void testPresentation(bool expectUnavailable) {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    KernelEqueue queue = 0;
    check(sceKernelCreateEqueue(&queue, "VideoOut flips") == 0, "event queue creation failed");
    auto owner = EqueuePin_nid_postfix(queue);
    check(sceVideoOutAddFlipEvent(queue, handle, nullptr) == 0, "flip subscription failed");
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    fillBuffer(storage, 259, 137);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    auto compressed = attribute;
    compressed.dcc_control = 1;
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &compressed, 0, nullptr);
    check(cfg->groups[0].occupied, "DCC buffer was rejected");
    sceVideoOutUnregisterBuffers(handle, 0);
    check(!cfg->groups[0].occupied, "unregistered DCC buffer kept its group");
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    sceVideoOutSetFlipRate(handle, 2);
    constexpr std::array<std::pair<int, int>, 6> flips{{{0, VIDEO_OUT_FLIP_MODE_VSYNC}, {-2, VIDEO_OUT_FLIP_MODE_VSYNC}, {-1, VIDEO_OUT_FLIP_MODE_VSYNC}, {0, VIDEO_OUT_FLIP_MODE_VSYNC}, {-1, VIDEO_OUT_FLIP_MODE_HSYNC}, {0, VIDEO_OUT_FLIP_MODE_HSYNC}}};
    for (const auto& [index, flipMode] : flips) {
        uint64_t target;
        uint64_t previousVblank;
        uint64_t submittedVblank;
        std::chrono::steady_clock::time_point submitted;
        {
            std::lock_guard lock(cfg->mutex);
            if (flipMode == VIDEO_OUT_FLIP_MODE_HSYNC) cfg->lastFlipVblank = cfg->vblankStatus.count + 1000000;
            target = cfg->flipStatus.count + 1;
            previousVblank = cfg->lastFlipVblank;
            submittedVblank = cfg->vblankStatus.count;
            submitted = std::chrono::steady_clock::now();
        }
        sceVideoOutSubmitFlip(handle, index, flipMode, -123456789);
        std::unique_lock lock(cfg->mutex);
        check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return (cfg->failure && cfg->flipStatus.flipPendingNum == 0) || cfg->flipStatus.count == target; }), "presentation did not complete");
        if (expectUnavailable) {
            check(cfg->failure != nullptr && cfg->flipStatus.count == 0 && cfg->bufferPending[0] == 0, "failed presentation was marked complete or retained its buffer");
            const auto error = cfg->failure;
            lock.unlock();
            const auto message = expectFailure([&] { std::rethrow_exception(error); });
            check(message.find("required instance extension missing") != std::string::npos, "unexpected presentation failure");
            check(expectFailure([] { AgcDriverWaitIdle_nid_postfix(); }) == message, "AGC lost presentation failure");
            check(expectFailure([] { LibcRunShutdown_nid_postfix(); }) == message, "shutdown lost presentation failure");
            return;
        }
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(cfg->flipStatus.flipArg == -123456789 && cfg->flipStatus.currentBuffer == index && cfg->flipStatus.flipPendingNum == 0 && (index < 0 || cfg->bufferPending[index] == 0), "presentation status is wrong");
        if (flipMode == VIDEO_OUT_FLIP_MODE_HSYNC) check(cfg->lastFlipVblank < previousVblank, "immediate flip waited for the flip rate");
        else check(cfg->lastFlipVblank >= previousVblank + 3, "flip rate did not wait for its interval");
        checkLatchedVblank(*cfg, submittedVblank, submitted);
        lock.unlock();
        KernelEvent event{};
        int64_t argument = 0;
        check(owner->GetTriggeredEvents(&event, 1) == 1 && sceVideoOutGetEventId(&event) == VIDEO_OUT_EVENT_FLIP && sceVideoOutGetEventData(&event, &argument) == 0 && argument == -123456789, "flip event is missing or wrong");
        attribute.width = 65;
        attribute.height = 33;
        sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute, nullptr);
    }
    sceVideoOutSetFlipRate(handle, 0);
    checkBackToBackFlips(handle, cfg, owner);
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    check(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    LibcRunShutdown_nid_postfix();
}

bool vrrActive(int handle) {
    VideoOutVblankStatus status{};
    check(sceVideoOutGetVblankStatus(handle, &status) == 0, "vblank status query failed");
    return status.flags == VIDEO_OUT_VBLANK_STATUS_FLAG_VRR_ACTIVE;
}

bool pegged(const std::shared_ptr<VideoOutConfig>& cfg) {
    std::lock_guard lock(cfg->mutex);
    return cfg->vrrPegged;
}

void checkModeEvent(const KernelEqueueRef& events, std::uint64_t mode, const char* reason) {
    KernelEvent event{};
    int64_t data = -1;
    check(events->GetTriggeredEvents(&event, 1) == 1 && sceVideoOutGetEventId(&event) == VIDEO_OUT_EVENT_SET_MODE && sceVideoOutGetEventData(&event, &data) == 0 && data == static_cast<int64_t>(mode), reason);
}

void testDisplay(bool vrr) {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    const auto cfg = VideoOutDriver::Get().GetConfig(handle);
    KernelEqueue queue = 0;
    check(sceKernelCreateEqueue(&queue, "VideoOut display") == 0, "event queue creation failed");
    auto owner = EqueuePin_nid_postfix(queue);
    check(sceVideoOutAddOutputModeEvent(queue, handle, nullptr) == 0 && sceVideoOutAddVrrActiveStatusEvent(queue, handle, nullptr) == 0, "display event subscription failed");
    std::array<KernelEvent, 3> registered{};
    check(owner->GetTriggeredEvents(registered.data(), static_cast<int>(registered.size())) == 2, "the output mode and the VRR status were not both signalled at registration");
    std::optional<int64_t> initialMode;
    std::optional<int64_t> initialVrr;
    for (std::size_t index = 0; index < 2; ++index) {
        int64_t data = -1;
        check(sceVideoOutGetEventData(&registered[index], &data) == 0 && sceVideoOutGetEventCount(&registered[index]) == 1, "registration event encoding is wrong");
        if (sceVideoOutGetEventId(&registered[index]) == VIDEO_OUT_EVENT_SET_MODE) initialMode = data;
        else if (sceVideoOutGetEventId(&registered[index]) == VIDEO_OUT_EVENT_VRR_ACTIVE_STATUS) initialVrr = data;
    }
    check(initialMode == static_cast<int64_t>(VIDEO_OUT_OUTPUT_MODE_DEFAULT) && initialVrr == (vrr ? 1 : 0), "registration did not signal the 59.94 Hz output mode and the display's VRR status");
    check(sceVideoOutIsOutputSupported(handle, VIDEO_OUT_OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 1 && sceVideoOutIsOutputSupported(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == 1, "the 120 Hz display does not offer 119.88 Hz");
    const uint64_t flags = vrr ? VIDEO_OUT_OUTPUT_STATUS_FLAG_VRR_CAPABLE : 0;
    VideoOutOutputStatus output{};
    check(sceVideoOutGetOutputStatus(handle, &output) == 0 && output.refreshRate == VIDEO_OUT_REFRESH_RATE_59_94HZ && output.flags == flags, "the 120 Hz display did not start at 59.94 Hz with the VRR capability of its profile");
    check(vrrActive(handle) == vrr, "the VRR active status does not match the display profile");
    check(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == 0, "119.88 Hz output was refused");
    checkModeEvent(owner, VIDEO_OUT_OUTPUT_MODE_119_88HZ, "switching to 119.88 Hz did not signal SET_MODE with the new mode");
    KernelEvent event{};
    check(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == 0 && owner->GetTriggeredEvents(&event, 1) == 0, "configuring the current output mode again signalled SET_MODE");
    check(sceVideoOutGetOutputStatus(handle, &output) == 0 && output.refreshRate == VIDEO_OUT_REFRESH_RATE_119_88HZ && output.flags == flags, "the 119.88 Hz output status is wrong");
    checkVblankRate(handle, cfg, 120000.0 / 1001.0, "the vblank count did not advance at 119.88 Hz after ConfigureOutput");
    check(!pegged(cfg), "a new port started pegged");
    check(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0 && pegged(cfg), "VrrPegToFixedRate did not peg the port");
    check(sceVideoOutVrrUnpegFromFixedRate(handle) == 0 && !pegged(cfg), "VrrUnpegFromFixedRate did not unpeg the port");
    check(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 0, "59.94 Hz output was refused");
    checkModeEvent(owner, VIDEO_OUT_OUTPUT_MODE_DEFAULT, "switching back to 59.94 Hz did not signal SET_MODE with the new mode");
    checkVblankRate(handle, cfg, 60000.0 / 1001.0, "the vblank count did not return to 59.94 Hz");
    check(vrrActive(handle) == vrr && owner->GetTriggeredEvents(&event, 1) == 0, "the VRR status changed or was signalled again after registration");
    sceVideoOutClose(handle);
    check(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    LibcRunShutdown_nid_postfix();
}

std::uint64_t completedFlips(const std::shared_ptr<VideoOutConfig>& cfg) {
    std::lock_guard lock(cfg->mutex);
    return cfg->flipStatus.count;
}

void waitForFlip(const std::shared_ptr<VideoOutConfig>& cfg, std::uint64_t count, std::chrono::seconds timeout, const char* reason) {
    std::unique_lock lock(cfg->mutex);
    check(cfg->vblankCond.wait_for(lock, timeout, [&] { return cfg->failure != nullptr || cfg->flipStatus.count >= count; }), reason);
    if (cfg->failure) std::rethrow_exception(cfg->failure);
}

void submitHeldFlip(const std::atomic<std::uint32_t>& label, std::uint32_t release, int handle, int64_t argument) {
    const auto address = reinterpret_cast<std::uint64_t>(&label);
    const auto flipArgument = static_cast<std::uint64_t>(argument);
    std::array<std::uint32_t, 13> words{0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), release, 0xffffffffu, 0x10u, AgcDriver::FlipPacketHeader, static_cast<std::uint32_t>(handle), static_cast<std::uint32_t>(VIDEO_OUT_BUFFER_INDEX_BLANK), static_cast<std::uint32_t>(VIDEO_OUT_FLIP_MODE_VSYNC), static_cast<std::uint32_t>(flipArgument), static_cast<std::uint32_t>(flipArgument >> 32u)};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "held flip submission failed");
}

void checkReserved(const std::shared_ptr<VideoOutConfig>& cfg) {
    std::lock_guard lock(cfg->mutex);
    check(cfg->flipStatus.flipPendingNum == 1, "the held flip was not reserved when it was submitted");
}

void testVrrFlips() {
    static std::atomic<std::uint32_t> label{0};
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    const auto cfg = VideoOutDriver::Get().GetConfig(handle);
    check(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == 0 && sceVideoOutSetFlipRate(handle, 2) == 0, "the VRR display did not switch to 119.88 Hz at flip rate 2");
    const auto period = VblankPeriod(VIDEO_OUT_OUTPUT_MODE_119_88HZ);
    const auto flip = [&](int64_t argument, const char* reason) {
        const auto target = completedFlips(cfg) + 1;
        sceVideoOutSubmitFlip(handle, VIDEO_OUT_BUFFER_INDEX_BLANK, VIDEO_OUT_FLIP_MODE_VSYNC, argument);
        waitForFlip(cfg, target, std::chrono::seconds(15), reason);
    };
    flip(1, "the first flip did not complete");
    std::chrono::steady_clock::time_point previousLatch;
    std::uint64_t unreachable = 0;
    {
        std::lock_guard lock(cfg->mutex);
        previousLatch = std::chrono::steady_clock::now();
        cfg->lastFlipLatch = previousLatch;
        unreachable = cfg->vblankStatus.count + 1000000;
        cfg->lastFlipVblank = unreachable;
    }
    flip(2, "an unpegged VRR flip waited for the vblank grid");
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->lastFlipVblank < unreachable, "an unpegged VRR flip waited for the vblank grid");
        check(cfg->lastFlipLatch >= previousLatch + period * 3, "an unpegged VRR flip at flip rate 2 was shown sooner than three 119.88 Hz periods after the previous flip");
    }
    check(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0, "VRR peg failed");
    std::uint64_t gridStart = 0;
    {
        std::lock_guard lock(cfg->mutex);
        gridStart = cfg->vblankStatus.count;
        cfg->lastFlipVblank = gridStart;
        cfg->lastFlipLatch = std::chrono::steady_clock::now() + std::chrono::hours(1);
    }
    flip(3, "a pegged VRR flip did not follow the vblank grid");
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->lastFlipVblank >= gridStart + 3, "a pegged VRR flip at flip rate 2 latched sooner than three vblanks after the previous flip");
    }
    check(sceVideoOutVrrUnpegFromFixedRate(handle) == 0, "VRR unpeg failed");
    auto target = completedFlips(cfg) + 1;
    submitHeldFlip(label, 1, handle, 4);
    checkReserved(cfg);
    check(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0, "VRR peg failed");
    {
        std::lock_guard lock(cfg->mutex);
        unreachable = cfg->vblankStatus.count + 1000000;
        cfg->lastFlipVblank = unreachable;
    }
    label.store(1);
    waitForFlip(cfg, target, std::chrono::seconds(5), "a flip reserved while unpegged followed a peg made after its reservation");
    target = completedFlips(cfg) + 1;
    submitHeldFlip(label, 2, handle, 5);
    checkReserved(cfg);
    check(sceVideoOutVrrUnpegFromFixedRate(handle) == 0, "VRR unpeg failed");
    {
        std::lock_guard lock(cfg->mutex);
        gridStart = cfg->vblankStatus.count;
        cfg->lastFlipVblank = gridStart;
        cfg->lastFlipLatch = std::chrono::steady_clock::now() + std::chrono::hours(1);
    }
    label.store(2);
    waitForFlip(cfg, target, std::chrono::seconds(5), "a flip reserved while pegged followed an unpeg made after its reservation");
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->lastFlipVblank >= gridStart + 3, "a flip reserved while pegged left the vblank grid");
    }
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

std::uint64_t vblankCount(const std::shared_ptr<VideoOutConfig>& cfg) {
    std::lock_guard lock(cfg->mutex);
    return cfg->vblankStatus.count;
}

std::uint64_t latchedVblank(const std::shared_ptr<VideoOutConfig>& cfg) {
    std::lock_guard lock(cfg->mutex);
    return cfg->lastFlipVblank;
}

std::chrono::steady_clock::time_point latchedTime(const std::shared_ptr<VideoOutConfig>& cfg) {
    std::lock_guard lock(cfg->mutex);
    return cfg->lastFlipLatch;
}

std::chrono::steady_clock::time_point limitSlot(const std::shared_ptr<VideoOutConfig>& cfg) {
    std::lock_guard lock(cfg->mutex);
    return cfg->lastLimitSlot;
}

void flipBlank(int handle, const std::shared_ptr<VideoOutConfig>& cfg, int64_t argument, const char* reason) {
    const auto target = completedFlips(cfg) + 1;
    sceVideoOutSubmitFlip(handle, VIDEO_OUT_BUFFER_INDEX_BLANK, VIDEO_OUT_FLIP_MODE_VSYNC, argument);
    waitForFlip(cfg, target, std::chrono::seconds(15), reason);
}

int openAtFlipRate2() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    check(sceVideoOutSetFlipRate(handle, 2) == 0, "flip rate 2 was refused");
    return handle;
}

void testUncapped() {
    check(IsTscCalibrated_nid_postfix(), "the uncapped test needs the calibrated TSC; run it without ANYPS5_LEGACY_TSC");
    const int handle = openAtFlipRate2();
    const auto cfg = VideoOutDriver::Get().GetConfig(handle);
    flipBlank(handle, cfg, 0, "the warm-up flip did not complete");
    const auto start = vblankCount(cfg);
    for (int64_t argument = 1; argument <= 10; ++argument) flipBlank(handle, cfg, argument, "an uncapped flip did not complete");
    check(vblankCount(cfg) - start < 20, "ten uncapped flips at flip rate 2 took 20 vblanks or more, close to the 27 that PS5 pacing needs");
    VideoOutDriver::Get().ToggleUncapped();
    const auto uncappedLatch = latchedVblank(cfg);
    flipBlank(handle, cfg, 11, "a flip after switching to PS5 pacing did not complete");
    check(latchedVblank(cfg) >= uncappedLatch + 3, "a flip after switching to PS5 pacing did not wait for flip rate 2");
    VideoOutDriver::Get().ToggleUncapped();
    std::uint64_t unreachable = 0;
    {
        std::lock_guard lock(cfg->mutex);
        unreachable = cfg->vblankStatus.count + 1000000;
        cfg->lastFlipVblank = unreachable;
    }
    flipBlank(handle, cfg, 12, "a flip after switching back to uncapped waited for the vblank grid");
    check(latchedVblank(cfg) < unreachable, "a flip after switching back to uncapped waited for the vblank grid");
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

void testUncappedLimit() {
    check(IsTscCalibrated_nid_postfix(), "the uncapped limit test needs the calibrated TSC; run it without ANYPS5_LEGACY_TSC");
    const int handle = openAtFlipRate2();
    const auto cfg = VideoOutDriver::Get().GetConfig(handle);
    for (int64_t argument = 0; argument < 4; ++argument) flipBlank(handle, cfg, argument, "a warm-up flip did not complete");
    const auto submitted = std::chrono::steady_clock::now();
    flipBlank(handle, cfg, 4, "a warm-up flip did not complete");
    const auto firstSlot = limitSlot(cfg);
    const auto firstLatch = latchedTime(cfg);
    for (int64_t argument = 5; argument < 14; ++argument) flipBlank(handle, cfg, argument, "a flip limited to 30 fps did not complete");
    const auto lastLatch = latchedTime(cfg);
    const auto interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0 / 30.0));
    check(firstSlot + interval >= submitted, "a flip limited to 30 fps was scheduled on a limit timeline more than one interval behind its submission");
    check(lastLatch - firstSlot >= interval * 9, "nine uncapped flips limited to 30 fps were shown sooner than nine 30 fps intervals after the limit slot of the flip before them");
    check(lastLatch - firstLatch < std::chrono::milliseconds(400), "nine uncapped flips limited to 30 fps took as long as flip rate 2 at 59.94 Hz, 450 ms");
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

void testUncappedLegacy() {
    check(!IsTscCalibrated_nid_postfix(), "the legacy uncapped test needs ANYPS5_LEGACY_TSC=1 in its environment");
    const int handle = openAtFlipRate2();
    const auto cfg = VideoOutDriver::Get().GetConfig(handle);
    flipBlank(handle, cfg, 0, "the first warm-up flip did not complete");
    flipBlank(handle, cfg, 1, "the second warm-up flip did not complete");
    for (int64_t argument = 2; argument < 4; ++argument) {
        const auto previous = latchedVblank(cfg);
        flipBlank(handle, cfg, argument, "a flip with the legacy TSC did not complete");
        check(latchedVblank(cfg) >= previous + 3, "ANYPS5_UNCAPPED or the frame-rate cap toggle took effect while the guest TSC runs on the legacy clock");
        VideoOutDriver::Get().ToggleUncapped();
    }
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

PacingEnvironment pacingEnvironmentFor(int argc, char** argv) {
    if (argc != 2) return {};
    const std::string command = argv[1];
    if (command == "display" || command == "vrr") return {"vrr"};
    if (command == "display120") return {"120hz"};
    if (command == "uncapped" || command == "uncappedlegacy") return {nullptr, "1", "0"};
    if (command == "uncappedlimit") return {nullptr, "1", "30"};
    return {};
}

int run(int argc, char** argv) {
    try {
        usePacingEnvironment(pacingEnvironmentFor(argc, argv));
        if (argc == 2 && std::string(argv[1]) == "decode") testDecode();
        else if (argc == 2 && std::string(argv[1]) == "pacing") testPacing();
        else if (argc == 2 && std::string(argv[1]) == "controls") testControls();
        else if (argc == 2 && std::string(argv[1]) == "display") testDisplay(true);
        else if (argc == 2 && std::string(argv[1]) == "display120") testDisplay(false);
        else if (argc == 2 && std::string(argv[1]) == "vrr") testVrrFlips();
        else if (argc == 2 && std::string(argv[1]) == "uncapped") testUncapped();
        else if (argc == 2 && std::string(argv[1]) == "uncappedlimit") testUncappedLimit();
        else if (argc == 2 && std::string(argv[1]) == "uncappedlegacy") testUncappedLegacy();
        else if (argc == 2 && std::string(argv[1]) == "present") testPresentation(false);
        else if (argc == 2 && std::string(argv[1]) == "unavailable") testPresentation(true);
        else testLifetime(argc == 2 && std::string(argv[1]) == "reopen");
        std::puts("VideoOut flip tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}

struct Arguments {
    int count;
    char** values;
};

}

#if defined(__APPLE__)
extern "C" int LibcRunGuestMain_nid_no_patch(void* args, int (*entry)(void*, void*));
#endif

int main(int argc, char** argv) {
#if defined(__APPLE__)
    Arguments arguments{argc, argv};
    return LibcRunGuestMain_nid_no_patch(&arguments, [](void* opaque, void*) {
        const auto& start = *static_cast<Arguments*>(opaque);
        return run(start.count, start.values);
    });
#else
    return run(argc, argv);
#endif
}

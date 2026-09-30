#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
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
    check(sceVideoOutIsOutputSupported(handle, VIDEO_OUT_OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 1 && sceVideoOutIsOutputSupported(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == 0, "output mode support is wrong");
    check(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE, "unavailable 119.88 Hz output was not refused");
    VideoOutOutputStatus output{};
    check(sceVideoOutGetOutputStatus(handle, &output) == 0 && output.refreshRate == VIDEO_OUT_REFRESH_RATE_59_94HZ, "refused output mode changed the refresh rate");
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

int run(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "decode") testDecode();
        else if (argc == 2 && std::string(argv[1]) == "pacing") testPacing();
        else if (argc == 2 && std::string(argv[1]) == "controls") testControls();
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

#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/PublishedPointer.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Eq/include/Query.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static_assert(sizeof(Packet) == 16);
static_assert(offsetof(Packet, addr) == 0);
static_assert(offsetof(Packet, dw_num) == 8);
static_assert(offsetof(Packet, flags) == 12);

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
std::string expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    throw std::runtime_error("expected exception");
}

void testEvents() {
    KernelEvent event{};
    event.filter = -14;
    event.ident = 0x40;
    event.data = 123;
    check(sceAgcDriverGetEqEventType(&event) == 0x40, "graphics event uses wrong field");
    event.filter = -1;
    event.data = -17;
    check(sceAgcDriverGetEqEventType(&event) == -17, "non-graphics event uses wrong field");
    event.data = std::numeric_limits<std::intptr_t>::max();
    expectFailure([&] { sceAgcDriverGetEqEventType(&event); });
    event.filter = -14;
    event.ident = std::numeric_limits<std::uintptr_t>::max();
    expectFailure([&] { sceAgcDriverGetEqEventType(&event); });
    expectFailure([] { sceAgcDriverGetEqEventType(nullptr); });
    expectFailure([&] { sceAgcDriverGetEqEventType(reinterpret_cast<const KernelEvent*>(reinterpret_cast<const std::byte*>(&event) + 1)); });
}

void testValidation() {
    std::array<std::uint32_t, 3> commands{0xc0017600, 0x20c, 0};
    Packet packet{commands.data(), 3, 0, {}};
    expectFailure([] { sceAgcDriverSubmitDcb(nullptr); });
    expectFailure([] { sceAgcDriverAgrSubmitDcb(nullptr); });
    expectFailure([] { sceAgcDriverSubmitAcb(0x20, nullptr); });
    expectFailure([&] { sceAgcDriverSubmitAcb(0, &packet); });
    expectFailure([&] { sceAgcDriverSubmitAcb(0x58, &packet); });
    packet.dw_num = 2;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.dw_num = 3;
    packet.flags = 1;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.flags = 0;
    commands[0] = 0xc001ff00;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    commands[0] = 0xc001105c;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    commands[0] = 0xc0017601;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    commands[0] = 0xc0017600;
    commands[1] = 0x10000;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.addr = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(commands.data()) + 1);
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.addr = reinterpret_cast<std::uint32_t*>(std::numeric_limits<std::uintptr_t>::max() - 3);
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.addr = reinterpret_cast<std::uint32_t*>(0x1000);
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    AgcDriverWaitIdle_nid_postfix();
}

void testClearState() {
    AgcDriver::QueueState graphics{0, {{0x20c, 1}}, {{0x10, 17}, {0x11, 23}}, {{0x242, 5}}};
    const auto shader = graphics.shader;
    const auto userConfig = graphics.userConfig;
    graphics.ClearContext();
    check(graphics.context == AgcDriver::InitialContextRegisters(), "CLEAR_STATE retained context registers");
    check(graphics.shader == shader && graphics.userConfig == userConfig, "CLEAR_STATE reset unrelated registers");
    graphics.context.emplace(0x10, 31);
    graphics.ClearContext();
    check(graphics.context == AgcDriver::InitialContextRegisters(), "repeated CLEAR_STATE retained context registers");

    std::array<std::uint32_t, 3> words{0xc0001200, 0, 0};
    Packet packet{words.data(), 2, 0, {}};
    expectFailure([&] { sceAgcDriverSubmitAcb(0x20, &packet); });
    words[1] = 0x10;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    words[1] = 0;
    words[0] = 0xc0011200;
    packet.dw_num = 3;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    words[0] = 0xc0001201;
    packet.dw_num = 2;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    words[0] = 0xc0001200;
    packet.dw_num = 1;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.dw_num = 2;
    for (std::uint32_t state = 0; state <= 0xf; ++state) {
        words[1] = state;
        check(sceAgcDriverSubmitDcb(&packet) == 0, "CLEAR_STATE submit failed");
    }
    AgcDriverWaitIdle_nid_postfix();
}

void testSubmissions() {
    std::vector<std::thread> producers;
    std::array<std::exception_ptr, 4> errors{};
    for (std::uint32_t i = 0; i < errors.size(); ++i) {
        producers.emplace_back([&, i] {
            try {
                for (std::uint32_t j = 0; j < 100; ++j) {
                    std::array<std::uint32_t, 5> words{0xc0017600, 0x240, j, 0xc0001000, 0};
                    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
                    if (i == 0) check(sceAgcDriverSubmitDcb(&packet) == 0, "DCB submit failed");
                    else if (i == 1) check(sceAgcDriverAgrSubmitDcb(&packet) == 0, "AGR submit failed");
                    else check(sceAgcDriverSubmitAcb(i == 2 ? 0x20 : 0x57, &packet) == 0, "ACB submit failed");
                    words.fill(0xffffffffu);
                }
            } catch (...) {
                errors[i] = std::current_exception();
            }
        });
    }
    for (auto& producer : producers) producer.join();
    for (auto& error : errors) if (error) std::rethrow_exception(error);
    AgcDriverWaitIdle_nid_postfix();
    Packet empty{};
    check(sceAgcDriverSubmitDcb(&empty) == 0, "empty submit failed");
    AgcDriverWaitIdle_nid_postfix();
}

void testWorkerFailure() {
    std::array<std::uint32_t, 5> words{0xc0031500, 1, 1, 1, 0x41};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(0x21, &packet) == 0, "dispatch was not accepted");
    std::array<std::string, 4> messages;
    std::vector<std::thread> waiters;
    for (auto& message : messages) {
        waiters.emplace_back([&message] { message = expectFailure([] { AgcDriverWaitIdle_nid_postfix(); }); });
    }
    for (auto& waiter : waiters) waiter.join();
    for (const auto& message : messages) check(message.find("required shader register") != std::string::npos, "worker failure was lost");
    check(expectFailure([&] { sceAgcDriverSubmitDcb(&packet); }) == messages[0], "subsequent DCB lost worker failure");
    check(expectFailure([&] { sceAgcDriverAgrSubmitDcb(&packet); }) == messages[0], "subsequent AGR lost worker failure");
    check(expectFailure([&] { sceAgcDriverSubmitAcb(0x20, &packet); }) == messages[0], "subsequent ACB lost worker failure");
}

void testPublishedPointer() {
    AgcDriver::PublishedPointer<int> device;
    AgcDriver::PublishedPointer<int>::Cache worker;
    AgcDriver::PublishedPointer<int>::Cache observer;
    check(device.Get(worker) == nullptr && device.Get(observer) == nullptr, "an empty published pointer was cached as a value");
    const auto created = device.GetOrCreate(worker, [] { return std::make_shared<int>(1); });
    check(created != nullptr && device.Get() == created && device.Get(worker) == created, "a lazily created value was not published to its creator");
    check(device.Get(observer) == created, "a value created lazily stayed invisible to a cache that had seen no value");
    check(device.GetOrCreate(worker, [] { return std::make_shared<int>(2); }) == created && device.Get() == created, "a lazy creation replaced an existing value");
    const auto replacement = std::make_shared<int>(3);
    std::thread presenter([&] { device.Publish(replacement); });
    presenter.join();
    check(device.Get(worker) == replacement && device.Get(observer) == replacement, "a value published on another thread stayed invisible to a cache");
    device.Publish(nullptr);
    check(device.Get(worker) == nullptr && device.Get(observer) == nullptr && device.Get() == nullptr, "a released value stayed cached");
    check(created.use_count() == 1 && replacement.use_count() == 1, "a cache retained a released value");
    std::future<std::shared_ptr<int>> reader;
    const auto presented = std::make_shared<int>(4);
    std::weak_ptr<int> discarded;
    const auto adopted = device.GetOrCreate(worker, [&] {
        reader = std::async(std::launch::async, [&] { return device.Get(); });
        check(reader.wait_for(std::chrono::seconds(10)) == std::future_status::ready, "a lazy creation held the lock of the published pointer, so a reader on another thread waited for it");
        std::thread publisher([&] { device.Publish(presented); });
        publisher.join();
        auto value = std::make_shared<int>(5);
        discarded = value;
        return value;
    });
    check(adopted == presented && device.Get() == presented && device.Get(observer) == presented, "a lazy creation replaced a value published while it ran");
    check(discarded.expired(), "a value created while another was published was retained");
    device.Publish(nullptr);
    const auto stored = device.GetOrCreate(worker, [&] {
        std::thread window([&] {
            device.Publish(std::make_shared<int>(6));
            device.Publish(nullptr);
        });
        window.join();
        return std::make_shared<int>(7);
    });
    check(stored != nullptr && *stored == 7 && device.Get() == stored && device.Get(observer) == stored, "a lazy creation returned no value after a value was published and released while it ran");
    device.Publish(nullptr);
    std::atomic<bool> observed = false;
    std::thread polling([&] {
        AgcDriver::PublishedPointer<int>::Cache cache;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (device.Get(cache) == nullptr && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        observed = device.Get(cache) != nullptr;
    });
    AgcDriver::PublishedPointer<int>::Cache creator;
    device.GetOrCreate(creator, [] { return std::make_shared<int>(8); });
    polling.join();
    check(observed, "a polling cache never observed a value created on another thread");
    AgcDriver::PublishedPointer<int> first;
    AgcDriver::PublishedPointer<int> second;
    AgcDriver::PublishedPointer<int>::Cache shared;
    const auto firstValue = std::make_shared<int>(9);
    const auto secondValue = std::make_shared<int>(10);
    first.Publish(firstValue);
    second.Publish(secondValue);
    check(first.Get(shared) == firstValue && second.Get(shared) == secondValue && first.Get(shared) == firstValue, "a cache shared by two published pointers served the value of one for the other");
    std::optional<AgcDriver::PublishedPointer<int>> replaced(std::in_place);
    replaced->Publish(firstValue);
    check(replaced->Get(shared) == firstValue, "a cache shared by published pointers missed a published value");
    replaced.emplace();
    replaced->Publish(secondValue);
    check(replaced->Get(shared) == secondValue, "a cache served the value of a replaced published pointer instead of its replacement's");
}

alignas(256) std::array<std::uint32_t, 64> programCode{};
Shader programHeader{};
volatile std::uint32_t label = 0;
volatile std::uint32_t marker = 0;

void registerProgram(std::uint8_t type) {
    programCode.fill(0xbf810000u);
    programHeader = Shader{};
    programHeader.file_header = 0x34333231u;
    programHeader.version = 0x18u;
    programHeader.code = programCode.data();
    programHeader.header_size = sizeof(Shader);
    programHeader.shader_size = sizeof(programCode);
    programHeader.type = type;
    AgcDriverRegisterShader_nid_postfix(&programHeader);
}

void submitCompute(std::vector<std::uint32_t> commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "compute submission was not accepted");
}

std::vector<std::uint32_t> labelWait() {
    const auto address = reinterpret_cast<std::uintptr_t>(&label);
    return {0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 1, 0xffffffffu, 10};
}

std::vector<std::uint32_t> programDispatch() {
    const auto address = reinterpret_cast<std::uintptr_t>(programCode.data());
    return {0xc0027600u, 0x20c, static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u), 0xc0031500u, 1, 1, 1, 0x41};
}

std::string testShaderRegisteredAfterSubmit() {
    auto commands = labelWait();
    const auto dispatch = programDispatch();
    commands.insert(commands.end(), dispatch.begin(), dispatch.end());
    submitCompute(commands);
    registerProgram(1);
    label = 1;
    const auto message = expectFailure([] { AgcDriverWaitIdle_nid_postfix(); });
    check(message.find("compute program does not belong to a registered shader") != std::string::npos, "a submission saw a shader registered after it");
    return message;
}

std::string testShaderRegisteredBeforeSubmit() {
    submitCompute({0xc0017600u, 0x240, 0});
    AgcDriverWaitIdle_nid_postfix();
    registerProgram(1);
    submitCompute(programDispatch());
    const auto message = expectFailure([] { AgcDriverWaitIdle_nid_postfix(); });
    check(message.find("compute program refers to a non-compute shader") != std::string::npos, "a submission missed a shader registered before it");
    return message;
}

std::string testGraphicsFailure() {
    const auto markerAddress = reinterpret_cast<std::uintptr_t>(&marker);
    auto commands = labelWait();
    commands.insert(commands.end(), {0xc0033700u, 0x100, static_cast<std::uint32_t>(markerAddress), static_cast<std::uint32_t>(markerAddress >> 32u), 1});
    submitCompute(commands);
    std::thread graphics([] { AgcDriverReportFailure_nid_postfix(std::make_exception_ptr(std::runtime_error("intentional graphics failure"))); });
    graphics.join();
    label = 1;
    check(expectFailure([] { submitCompute({0xc0017600u, 0x240, 0}); }) == "intentional graphics failure", "the next submission lost a failure reported on the graphics thread");
    check(expectFailure([] { AgcDriverWaitIdle_nid_postfix(); }) == "intentional graphics failure", "a suspend point lost a failure reported on the graphics thread");
    check(marker == 0, "the worker executed a packet after a failure reported on the graphics thread");
    return "intentional graphics failure";
}

std::vector<std::uint32_t> indirectRegisters(std::uint32_t opcode, const std::uint32_t* pairs, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(pairs);
    return {0xc0030000u | (opcode << 8u), static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 0x80000000u, count};
}

void testIndirectContextRegisters() {
    const std::array<std::uint32_t, 6> pairs{0x80000202u, 0x00cc0010u, 0x0bad0202u, 0x00cc0010u, 0x202u, 0x00cc0040u};
    const auto markerAddress = reinterpret_cast<std::uintptr_t>(&marker);
    auto commands = indirectRegisters(0x9f, pairs.data(), 3);
    commands.insert(commands.end(), {0xc0012d00u, 3, 2, 0xc0033700u, 0x100, static_cast<std::uint32_t>(markerAddress), static_cast<std::uint32_t>(markerAddress >> 32u), 1, 0xc0001200u, 0});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "context register list submission was not accepted");
    AgcDriverWaitIdle_nid_postfix();
    check(marker == 1, "a draw after a context register list saw other context registers than the list set");
    marker = 0;
}

const std::array<std::uint32_t, 10> shaderRegisterPairs{0x20cu, 0x11u, 0x0bad020eu, 0x99u, 0x8000020eu, 0x22u, 0x20fu, 0x33u, 0x20eu, 0x44u};

std::string expectListedShaderRegisters(const char* reason) {
    const auto message = expectFailure([] { AgcDriverWaitIdle_nid_postfix(); });
    check(message.find("written neighbours: 0x20c=0x11 0x20e=0x44 0x20f=0x33") != std::string::npos, reason);
    return message;
}

std::vector<std::uint32_t> shaderRegisterDispatch(const std::uint32_t* pairs) {
    auto commands = labelWait();
    const auto list = indirectRegisters(0x63, pairs, static_cast<std::uint32_t>(shaderRegisterPairs.size() / 2));
    commands.insert(commands.end(), list.begin(), list.end());
    commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, 0x41});
    return commands;
}

std::string testIndirectRegistersFromCopy() {
    auto pairs = shaderRegisterPairs;
    submitCompute(shaderRegisterDispatch(pairs.data()));
    pairs.fill(0x20du);
    label = 1;
    return expectListedShaderRegisters("a register list was not applied from its copy taken at submission");
}

std::uint32_t* allocatePage() {
#ifdef _WIN32
    return static_cast<std::uint32_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
    auto* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return page == MAP_FAILED ? nullptr : static_cast<std::uint32_t*>(page);
#endif
}

void protectPage(void* page, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    check(VirtualProtect(page, 4096, readable ? PAGE_READWRITE : PAGE_NOACCESS, &previous) != 0, "VirtualProtect failed");
#else
    check(mprotect(page, 4096, readable ? PROT_READ | PROT_WRITE : PROT_NONE) == 0, "mprotect failed");
#endif
}

void releasePage(void* page) {
#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, 4096);
#endif
}

std::string testIndirectRegistersInPlace() {
    auto* pairs = allocatePage();
    check(pairs != nullptr, "register list page allocation failed");
    std::copy(shaderRegisterPairs.begin(), shaderRegisterPairs.end(), pairs);
    protectPage(pairs, false);
    submitCompute(shaderRegisterDispatch(pairs));
    protectPage(pairs, true);
    label = 1;
    const auto message = expectListedShaderRegisters("a register list that could not be copied at submission was not read in place when it executed");
    releasePage(pairs);
    return message;
}

std::string testIndirectRegisterSentinel() {
    const std::array<std::uint32_t, 4> pairs{0x20cu, 0x11u, 0xffffffffu, 0};
    auto commands = indirectRegisters(0x63, pairs.data(), 2);
    commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, 0x41});
    submitCompute(commands);
    const auto message = expectFailure([] { AgcDriverWaitIdle_nid_postfix(); });
    check(message.find("indirect register sentinel semantics are not implemented: indirect register packet") != std::string::npos, "a register list the packet executor rejects did not fail with its report");
    return message;
}

std::shared_ptr<AgcDriver::FrameTiming> flippedFrame;

class FrameCapture final : public AgcDriver::IFlipRequest {
public:
    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>& timing) override { flippedFrame = timing; }
    void Fail(std::exception_ptr) noexcept override {}
};

class FrameCaptureOutput final : public AgcDriver::IVideoOutput {
public:
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override { return std::make_shared<FrameCapture>(); }
    void Fail(std::exception_ptr) noexcept override {}
};

void testIndirectRegisterTiming() {
#ifdef _WIN32
    check(_putenv_s("ANYPS5_TRACE_TIMING", "1") == 0, "frame timing could not be enabled");
#else
    check(setenv("ANYPS5_TRACE_TIMING", "1", 1) == 0, "frame timing could not be enabled");
#endif
    const auto output = std::make_shared<FrameCaptureOutput>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    const std::array<std::uint32_t, 4> contextPairs{0x202u, 0x00cc0010u, 0x203u, 0};
    const std::array<std::uint32_t, 2> userConfigPairs{0x243u, 1};
    std::vector<std::uint32_t> commands;
    const auto append = [&](const std::vector<std::uint32_t>& words) { commands.insert(commands.end(), words.begin(), words.end()); };
    append(indirectRegisters(0x9f, contextPairs.data(), 2));
    append(indirectRegisters(0x63, shaderRegisterPairs.data(), static_cast<std::uint32_t>(shaderRegisterPairs.size() / 2)));
    append(indirectRegisters(0x64, userConfigPairs.data(), 1));
    append({0xc004105cu, 7, 0xfffffffeu, 1, 0, 0});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "register lists before a flip were not accepted");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
    check(flippedFrame != nullptr, "the flip after the register lists did not hand over its frame timing");
    check(flippedFrame->Get("Driver.Packet", "register_list")->count == 3 && flippedFrame->Get("Driver.Packet", "pm4_execute")->count == 0, "register lists copied at submission went through the packet executor instead of being applied from their copies");
    flippedFrame.reset();
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc == 2 ? argv[1] : "";
        std::string expected = "required shader register";
        if (mode == "shader-after-submit") expected = testShaderRegisteredAfterSubmit();
        else if (mode == "shader-before-submit") expected = testShaderRegisteredBeforeSubmit();
        else if (mode == "graphics-failure") expected = testGraphicsFailure();
        else if (mode == "indirect-registers") expected = testIndirectRegistersFromCopy();
        else if (mode == "indirect-registers-in-place") expected = testIndirectRegistersInPlace();
        else if (mode == "indirect-register-sentinel") expected = testIndirectRegisterSentinel();
        else if (mode == "indirect-register-timing") {
            testIndirectRegisterTiming();
            testWorkerFailure();
        } else {
            testEvents();
            testValidation();
            testClearState();
            testSubmissions();
            testIndirectContextRegisters();
            testPublishedPointer();
            testWorkerFailure();
        }
        check(expectFailure([] { LibcRunShutdown_nid_postfix(); }).find(expected) != std::string::npos, "shutdown lost worker failure");
        std::puts("AGC driver submit tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}

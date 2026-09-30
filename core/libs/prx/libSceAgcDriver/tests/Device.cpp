#include "DeviceTests.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <future>
#include <initializer_list>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::vector<std::uint32_t> makePacket(std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    std::vector<std::uint32_t> result{0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u)};
    result.insert(result.end(), payload);
    return result;
}

std::vector<std::uint32_t> emptyComputeShader() {
    enum : std::uint32_t { OpMemoryModel = 14, OpEntryPoint = 15, OpExecutionMode = 16, OpCapability = 17, OpTypeVoid = 19, OpTypeFunction = 33, OpFunction = 54, OpFunctionEnd = 56, OpLabel = 248, OpReturn = 253 };
    enum : std::uint32_t { CapabilityShader = 1, AddressingLogical = 0, MemoryGlsl450 = 1, ModelGlCompute = 5, ModeLocalSize = 17, NameMain = 0x6e69616du, FunctionControlNone = 0 };
    enum : std::uint32_t { Main = 1, Void = 2, Signature = 3, Entry = 4, Bound = 5 };
    std::vector<std::uint32_t> words{0x07230203u, 0x00010000u, 0, Bound, 0};
    const auto instruction = [&words](std::uint32_t opcode, std::initializer_list<std::uint32_t> operands) {
        words.push_back((static_cast<std::uint32_t>(operands.size() + 1) << 16u) | opcode);
        words.insert(words.end(), operands);
    };
    instruction(OpCapability, {CapabilityShader});
    instruction(OpMemoryModel, {AddressingLogical, MemoryGlsl450});
    instruction(OpEntryPoint, {ModelGlCompute, Main, NameMain, 0});
    instruction(OpExecutionMode, {Main, ModeLocalSize, 1, 1, 1});
    instruction(OpTypeVoid, {Void});
    instruction(OpTypeFunction, {Signature, Void});
    instruction(OpFunction, {Void, Main, FunctionControlNone, Signature});
    instruction(OpLabel, {Entry});
    instruction(OpReturn, {});
    instruction(OpFunctionEnd, {});
    return words;
}

void testDirectDispatchWithoutGroups() {
    std::array<std::uint32_t, 1> destination{};
    const auto unregistered = reinterpret_cast<std::uintptr_t>(destination.data());
    std::vector<std::uint32_t> commands;
    for (const auto& packet : {
        makePacket(0x76, {0x20c, static_cast<std::uint32_t>(unregistered >> 8u), static_cast<std::uint32_t>(unregistered >> 40u)}),
        makePacket(0x15, {0, 1, 1, 0x41}),
        makePacket(0x15, {1, 0, 1, 0x8041}),
        makePacket(0x15, {1, 1, 0, 0x41}),
        makePacket(0x37, {0x100, static_cast<std::uint32_t>(unregistered), static_cast<std::uint32_t>(unregistered >> 32u), 57})
    }) commands.insert(commands.end(), packet.begin(), packet.end());
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "a submission of dispatches without thread groups failed");
    AgcDriverWaitIdle_nid_postfix();
    check(destination[0] == 57, "a direct dispatch without thread groups was prepared or stopped the command stream");
}

void testDispatchWithoutGroups(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compute) {
    const std::array<std::array<std::uint32_t, 3>, 3> empty{{{0, 1, 1}, {1, 0, 1}, {1, 1, 0}}};
    for (const auto& groups : empty) {
        device.Dispatch(compute, groups[0], groups[1], groups[2]);
        check(!device.HasPendingWork(), "a dispatch without thread groups recorded GPU work");
    }
    device.Dispatch(compute, 1, 1, 1);
    check(device.HasPendingWork(), "a dispatch with thread groups recorded no GPU work");
    device.WaitIdle();
    check(!device.HasPendingWork(), "an idle device kept GPU work pending");
}

void testPublishedWriterRanges(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compute) {
    const auto pageSize = GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix();
    const std::size_t mapped = 16 * pageSize;
    void* memory = GuestMemoryBacking::GuestMemoryBackingMap_nid_postfix(nullptr, mapped, 1u << 16u, 3);
    check(memory != nullptr, "guest memory for a writing dispatch could not be mapped");
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    try {
        auto writer = compute;
        ShaderRecompiler::DescriptorBinding binding;
        binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
        binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
        binding.descriptorSet = 0;
        binding.binding = 0;
        binding.count = 1;
        binding.guestDescriptor = {static_cast<std::uint32_t>(base), static_cast<std::uint32_t>(base >> 32u) & 0xffffu, 256, 0x31000000u};
        writer.bindings.push_back(binding);
        device.Dispatch(writer, 1, 1, 1);
        check(device.NeedsResolve(base, 256) && device.NeedsResolve(base + 255, 1) && !device.NeedsResolve(base + 256, 256), "a queued dispatch did not publish exactly the range it writes");
        device.ResolveMemory(base + 8 * pageSize, 64, false);
        check(device.HasPendingWork() && device.NeedsResolve(base, 256), "resolving memory a queued dispatch does not write waited for the dispatch");
        device.ResolveMemory(base + 16, 4, false);
        check(!device.HasPendingWork() && !device.NeedsResolve(base, 256), "resolving memory a queued dispatch writes did not wait for it or kept its range published");
    } catch (...) {
        device.WaitIdle();
        GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(memory, mapped);
        throw;
    }
    device.WaitIdle();
    GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(memory, mapped);
}

void testResolveWithoutGpuWork(AgcDriver::VulkanDevice& device) {
    std::array<std::uint32_t, 16> memory{};
    const auto address = reinterpret_cast<std::uintptr_t>(memory.data());
    const auto bytes = sizeof(memory);
    check(!device.NeedsResolve(address, bytes), "memory that no GPU work or render target covers needs resolving");
    std::promise<void> locked;
    std::promise<void> release;
    auto released = release.get_future();
    std::thread holder([&locked, &released] {
        const std::lock_guard lock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
        locked.set_value();
        released.wait();
    });
    locked.get_future().wait();
    auto resolved = std::async(std::launch::async, [&device, address, bytes] { device.ResolveMemory(address, bytes, true); });
    const bool prompt = resolved.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
    release.set_value();
    holder.join();
    resolved.get();
    check(prompt, "resolving memory that no GPU work or render target covers waited for the device lock");
}

}

void RunDeviceTests() {
    testDirectDispatchWithoutGroups();
    ShaderRecompiler::RecompileResult compute;
    compute.spirv = emptyComputeShader();
    AgcDriver::VulkanDevice device;
    testDispatchWithoutGroups(device, compute);
    testPublishedWriterRanges(device, compute);
    testResolveWithoutGpuWork(device);
}

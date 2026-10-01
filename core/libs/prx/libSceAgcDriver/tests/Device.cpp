#include "DeviceTests.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CaptureFormat.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "tests/SyntheticPrograms.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <initializer_list>
#include <mutex>
#include <stdexcept>
#include <string>
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

void testDispatchCapture(AgcDriver::VulkanDevice& device) {
    namespace Synthetic = ShaderRecompiler::SyntheticPrograms;
    constexpr std::uint32_t threads = 64u;
    constexpr std::uint32_t groups = 2u;
    constexpr std::uint32_t total = threads * groups;
    constexpr std::uint32_t count = 100u;
    constexpr std::uint32_t threshold = 40u;
    constexpr std::uint32_t sentinel = 0xdeadbeefu;
    const auto pattern = [](std::uint32_t word) { return word * 2654435761u + 0x4321u; };
    const auto pageSize = GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix();
    const std::size_t mapped = 16 * pageSize;
    void* memory = GuestMemoryBacking::GuestMemoryBackingMap_nid_postfix(nullptr, mapped, 1u << 16u, 3);
    check(memory != nullptr, "guest memory for a captured dispatch could not be mapped");
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    const auto input = base + 0x40u;
    const auto output = base + 4 * pageSize + 0x20u;
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-driver-capture-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto cleanup = [&] {
        std::error_code error;
        std::filesystem::remove_all(root, error);
        device.WaitIdle();
        GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(memory, mapped);
    };
    try {
        for (std::uint32_t word = 0; word < total; ++word) {
            const auto value = pattern(word);
            std::memcpy(reinterpret_cast<void*>(input + word * 4u), &value, sizeof(value));
            std::memcpy(reinterpret_cast<void*>(output + word * 4u), &sentinel, sizeof(sentinel));
        }
        const auto program = Synthetic::PerThreadProgram({threads, 1u, 1u}, count, threshold, input, output, total);
        const auto request = program.Request(64u, device.Target().subgroupSize, false);
        const auto compiled = ShaderRecompiler::Recompile(request);
        const AgcDriver::Graphics::CaptureTarget target{root, Synthetic::CodeAddress, 0xabcdefull, 1, ShaderRecompiler::RequestSerializer{}.Serialize(request)};
        device.Dispatch(compiled, groups, 1, 1, {}, &target);
        device.WaitIdle();
        std::vector<std::byte> produced(total * 4u);
        std::memcpy(produced.data(), reinterpret_cast<const void*>(output), produced.size());
        for (std::uint32_t word = 0; word < total; ++word) {
            std::uint32_t value = 0;
            std::memcpy(&value, produced.data() + word * 4u, sizeof(value));
            check(value == (word < count ? Synthetic::PerThreadExpected(word, pattern(word), threshold) : sentinel), "a captured dispatch wrote the wrong guest memory");
        }
        std::vector<std::filesystem::path> captures;
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (std::filesystem::is_regular_file(entry.path() / AgcDriver::Graphics::CaptureManifestName)) captures.push_back(entry.path());
        }
        check(captures.size() == 1 && captures[0].filename().string() == "20000-2x1x1-1", "a captured dispatch did not write exactly one capture named after its program and groups");
        const auto manifest = AgcDriver::Graphics::ReadCaptureManifest(captures[0]);
        check(manifest.program == Synthetic::CodeAddress && manifest.codeHash == 0xabcdefull && manifest.groups == std::array<std::uint32_t, 3>{groups, 1u, 1u} && manifest.lanes == compiled.lanesPerInvocation && manifest.notes.empty(), "a driver capture has the wrong header or notes");
        check(manifest.writes.size() == 1 && manifest.writes[0].begin == output && manifest.writes[0].end == output + total * 4u, "a driver capture did not record the written guest range");
        check(AgcDriver::Graphics::ReadCaptureBlob(root, manifest.writes[0].blob) == produced, "a driver capture's reference output differs from the guest memory the dispatch wrote");
        const auto inputRegion = std::find_if(manifest.regions.begin(), manifest.regions.end(), [&](const auto& region) { return region.begin <= input && input + total * 4u <= region.end; });
        check(inputRegion != manifest.regions.end() && !inputRegion->writable, "a driver capture did not record the input region");
        const auto inputBytes = AgcDriver::Graphics::ReadCaptureBlob(root, inputRegion->blob);
        const auto inputOffset = static_cast<std::size_t>(inputRegion->padding + (input - inputRegion->begin));
        std::uint32_t firstInput = 0;
        std::memcpy(&firstInput, inputBytes.data() + inputOffset + 12u, sizeof(firstInput));
        check(inputBytes.size() == inputRegion->padding + (inputRegion->end - inputRegion->begin) && firstInput == pattern(3), "a driver capture's input region differs from guest memory");
        const auto outputRegion = std::find_if(manifest.regions.begin(), manifest.regions.end(), [&](const auto& region) { return region.begin <= output && output + total * 4u <= region.end; });
        check(outputRegion != manifest.regions.end() && outputRegion->writable, "a driver capture did not record the output region as writable");
        const auto outputBytes = AgcDriver::Graphics::ReadCaptureBlob(root, outputRegion->blob);
        std::uint32_t firstOutput = 0;
        std::memcpy(&firstOutput, outputBytes.data() + outputRegion->padding + (output - outputRegion->begin), sizeof(firstOutput));
        check(firstOutput == sentinel, "a driver capture's output region was not recorded before the dispatch ran");
        check(std::count_if(manifest.buffers.begin(), manifest.buffers.end(), [](const auto& buffer) { return buffer.source == AgcDriver::Graphics::CaptureBufferSource::Guest; }) == 2, "a driver capture did not record both guest buffers");
        std::ifstream spirv(captures[0] / manifest.spirv, std::ios::binary | std::ios::ate);
        check(spirv && static_cast<std::size_t>(spirv.tellg()) == compiled.spirv.size() * sizeof(std::uint32_t), "a driver capture did not save the SPIR-V it dispatched");
        std::ifstream requestFile(captures[0] / manifest.request, std::ios::binary);
        std::string header;
        std::string payload;
        check(requestFile && std::getline(requestFile, header) && std::getline(requestFile, payload) && header == AgcDriver::Graphics::CaptureRequestHeader && payload == target.request, "a driver capture did not save its recompile request");
    } catch (...) {
        cleanup();
        throw;
    }
    cleanup();
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
    testDispatchCapture(device);
    testResolveWithoutGpuWork(device);
}

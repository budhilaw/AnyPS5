#include "DispatchCaptureTests.hpp"
#include "BdaShader.hpp"
#include "BdaAbi.hpp"
#include "prx/libSceAgcDriver/Execution/include/DispatchCapture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CaptureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CaptureObjects.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DispatchRecorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libSceAgcDriver/Replay/include/DispatchReplay.hpp"
#include "prx/libSceAgcDriver/tests/shaders/CaptureReplay_spv.h"
#include "ControlFlow/RequestSerializer.hpp"
#include "Recompiler.hpp"
#include "tests/SyntheticPrograms.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::Replay::ReplayCapture;
using AgcDriver::Replay::ReplayDevice;
using AgcDriver::Replay::ReplayMemory;
using AgcDriver::Replay::ReplayOptions;
using AgcDriver::Replay::ReplayShader;
namespace Synthetic = ShaderRecompiler::SyntheticPrograms;

constexpr VkMemoryPropertyFlags HostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(const std::string& name) {
        path = std::filesystem::temp_directory_path() / (name + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    std::filesystem::path path;
};

std::vector<std::byte> readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    Require(static_cast<bool>(file), "cannot open " + path.string());
    std::vector<std::byte> bytes(static_cast<std::size_t>(file.tellg()));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "cannot read " + path.string());
    return bytes;
}

void writeFile(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "cannot rewrite " + path.string());
}

std::uint32_t inputPattern(std::size_t word) {
    return static_cast<std::uint32_t>(word) * 2654435761u + 0x1234u;
}

std::uint32_t wordAt(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

struct HarnessBinding {
    std::uint32_t binding = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    std::vector<VkDescriptorBufferInfo> buffers;
    std::vector<VkDescriptorImageInfo> images;
};

void runHarness(const Context& context, std::span<const std::uint32_t> code, std::span<const HarnessBinding> bindings, std::span<const std::byte> push, std::array<std::uint32_t, 3> groups) {
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    const auto release = [&] {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (setLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    };
    try {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
        std::vector<VkDescriptorPoolSize> sizes;
        for (const auto& binding : bindings) {
            const auto count = static_cast<std::uint32_t>(binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ? binding.buffers.size() : binding.images.size());
            layoutBindings.push_back({binding.binding, binding.type, count, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
            sizes.push_back({binding.type, count});
        }
        VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setInfo.bindingCount = static_cast<std::uint32_t>(layoutBindings.size());
        setInfo.pBindings = layoutBindings.data();
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        poolInfo.pPoolSizes = sizes.data();
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1, &setLayout};
        Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets");
        std::vector<VkWriteDescriptorSet> writes;
        for (const auto& binding : bindings) {
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = set;
            write.dstBinding = binding.binding;
            write.descriptorType = binding.type;
            write.descriptorCount = static_cast<std::uint32_t>(binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ? binding.buffers.size() : binding.images.size());
            if (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) write.pBufferInfo = binding.buffers.data();
            else write.pImageInfo = binding.images.data();
            writes.push_back(write);
        }
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
        const VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, static_cast<std::uint32_t>(push.size())};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &setLayout;
        layoutInfo.pushConstantRangeCount = push.empty() ? 0u : 1u;
        layoutInfo.pPushConstantRanges = push.empty() ? nullptr : &range;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = code.size_bytes();
        moduleInfo.pCode = code.data();
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
        pipelineInfo.layout = layout;
        Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        const VkMemoryBarrier upload{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &upload, 0, nullptr, 0, nullptr);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
        if (!push.empty()) context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, static_cast<std::uint32_t>(push.size()), push.data());
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, groups[0], groups[1], groups[2]);
        const VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
        batch.SubmitAndWait();
    } catch (...) {
        release();
        throw;
    }
    release();
}

struct TestRegion {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    std::uint64_t padding = 0;
    bool writable = false;
    std::unique_ptr<Buffer> buffer;
};

TestRegion makeRegion(const Context& context, std::uint64_t begin, std::uint64_t bytes, bool writable, std::uint32_t (*pattern)(std::size_t)) {
    TestRegion region{begin, begin + bytes, begin % 256u, writable, nullptr};
    region.buffer = std::make_unique<Buffer>(context, static_cast<std::size_t>(region.padding + bytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto target = region.buffer->Bytes();
    std::memset(target.data(), 0x6b, static_cast<std::size_t>(region.padding));
    for (std::size_t word = 0; word < bytes / 4; ++word) {
        const auto value = pattern(word);
        std::memcpy(target.data() + region.padding + word * 4, &value, sizeof(value));
    }
    return region;
}

DispatchBindings::BoundRegion boundRegion(TestRegion& region) {
    const auto bytes = region.buffer->Bytes();
    return {region.begin, region.end, region.padding, region.writable, false, false, HostMemory, std::span<const std::byte>(bytes.data(), bytes.size()), nullptr};
}

void flipBlob(const std::filesystem::path& root, const std::string& name, std::vector<std::byte>& original) {
    const auto path = root / CaptureBlobDirectory / name;
    original = readFile(path);
    Require(original.size() > 8, "the blob to corrupt is too small");
    auto corrupted = original;
    corrupted[5] ^= std::byte{0x40};
    writeFile(path, corrupted);
}

void selectionParses() {
    const auto settings = AgcDriver::ParseDispatchCaptureSettings("0x140019d600, 0x140019C400:3,9f3a", "captures-root", "12.5");
    Require(settings.errors.empty() && settings.selectors.size() == 3, "a valid capture selection was rejected");
    Require(settings.selectors[0].value == 0x140019d600ull && settings.selectors[0].count == 1 && settings.selectors[1].value == 0x140019c400ull && settings.selectors[1].count == 3 && settings.selectors[2].value == 0x9f3au, "a capture selection parsed to the wrong programs or counts");
    Require(settings.after == 12.5 && settings.root.is_absolute() && settings.root.filename() == "captures-root", "the capture delay or directory was parsed wrongly");
    const auto invalid = AgcDriver::ParseDispatchCaptureSettings("zz,0x10:0,0x20:abc,0x30:5,,0x40:1001", nullptr, "soon");
    Require(invalid.selectors.size() == 1 && invalid.selectors[0].value == 0x30u && invalid.selectors[0].count == 5 && invalid.errors.size() == 5, "invalid capture selections were accepted or valid ones dropped");
    Require(invalid.root.filename() == "dispatch-captures" && invalid.root.is_absolute() && invalid.after == 0.0, "the default capture directory or delay is wrong");
    Require(AgcDriver::ParseDispatchCaptureSettings(nullptr, nullptr, nullptr).selectors.empty(), "an unset capture selection selected programs");
    const auto costliest = AgcDriver::ParseDispatchCaptureSettings("top8,0x50", nullptr, nullptr);
    Require(costliest.errors.empty() && costliest.costliest == 8 && costliest.selectors.size() == 1 && costliest.selectors[0].value == 0x50u, "a capture of the costliest programs was parsed wrongly");
    const auto badCostliest = AgcDriver::ParseDispatchCaptureSettings("top0,top21,topx", nullptr, nullptr);
    Require(badCostliest.costliest == 0 && badCostliest.errors.size() == 3, "an invalid count of costliest programs was accepted");
}

void manifestRoundTrips() {
    TemporaryDirectory directory("anyps5-capture-format");
    const std::vector<std::byte> content{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}};
    const auto blob = WriteCaptureBlob(directory.path, content);
    Require(WriteCaptureBlob(directory.path, content) == blob && ReadCaptureBlob(directory.path, blob) == content, "identical capture blobs are not stored once or do not read back");
    Require(WriteCaptureBlob(directory.path, std::span(content).first(4)) != blob, "different capture blobs share a name");
    Require(CaptureContentHash({}) != CaptureContentHash(std::span(content).first(1)), "the capture content hash ignores the size");
    CaptureManifest manifest;
    manifest.program = 0x140019c400ull;
    manifest.codeHash = 0x0123456789abcdefull;
    manifest.groups = {320, 180, 1};
    manifest.lanes = 2;
    manifest.spirv = CaptureSpirvName;
    manifest.request = CaptureRequestName;
    manifest.push = {std::byte{0xab}, std::byte{0x00}, std::byte{0x10}};
    manifest.layout = {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4}, {48, VK_DESCRIPTOR_TYPE_SAMPLER, 2}};
    manifest.descriptors.push_back({0, 0, 1, 1, 1, {1, 0}, {0xffffffffu, 2, 3, 4}});
    manifest.regions.push_back({0x1000, 0x2010, 0x10, true, true, HostMemory, blob});
    manifest.writes.push_back({0x1800, 0x1900, blob});
    manifest.buffers.push_back({0, 1, CaptureBufferSource::Gds, 0, 0x10000, HostMemory, blob, blob});
    CaptureManifest::Image image;
    image.type = VK_IMAGE_TYPE_3D;
    image.format = VK_FORMAT_BC7_UNORM_BLOCK;
    image.width = 64;
    image.height = 32;
    image.depth = 8;
    image.levels = 3;
    image.layers = 1;
    image.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    image.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image.blob = blob;
    image.contents.push_back({VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 64, 128, 32, 16, 4});
    image.written.push_back({VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 0, 8, 1, 1, 1});
    manifest.images.push_back(image);
    CaptureManifest::View view;
    view.binding = 3;
    view.element = 1;
    view.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    view.viewType = VK_IMAGE_VIEW_TYPE_3D;
    view.format = VK_FORMAT_BC7_SRGB_BLOCK;
    view.components = {VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE};
    view.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    view.baseLevel = 1;
    view.levels = 2;
    view.layers = 1;
    view.minLod = 1.25f;
    view.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    view.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    manifest.views.push_back(view);
    CaptureManifest::Sampler sampler;
    sampler.binding = 48;
    sampler.element = 1;
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.mipLodBias = -0.5f;
    sampler.maxAnisotropy = 16.0f;
    sampler.maxLod = 1000.0f;
    sampler.compareEnable = 1;
    sampler.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    manifest.samplers.push_back(sampler);
    manifest.notes.push_back("an image could not be copied");
    const auto captureDirectory = directory.path / "capture";
    WriteCaptureManifest(captureDirectory, manifest);
    const auto read = ReadCaptureManifest(captureDirectory);
    Require(read.program == manifest.program && read.codeHash == manifest.codeHash && read.groups == manifest.groups && read.lanes == 2 && read.spirv == manifest.spirv && read.request == manifest.request && read.push == manifest.push, "the capture header did not round trip");
    Require(read.layout.size() == 2 && read.layout[1].binding == 48 && read.layout[1].type == VK_DESCRIPTOR_TYPE_SAMPLER && read.layout[1].count == 2, "the capture layout did not round trip");
    Require(read.descriptors.size() == 1 && read.descriptors[0].words == manifest.descriptors[0].words && read.descriptors[0].samplerDepthCompare == manifest.descriptors[0].samplerDepthCompare && read.descriptors[0].imageDepthCompare == 1, "the capture descriptors did not round trip");
    Require(read.regions.size() == 1 && read.regions[0].begin == 0x1000 && read.regions[0].end == 0x2010 && read.regions[0].padding == 0x10 && read.regions[0].writable && read.regions[0].imported && read.regions[0].memory == HostMemory && read.regions[0].blob == blob, "the capture regions did not round trip");
    Require(read.writes.size() == 1 && read.writes[0].begin == 0x1800 && read.writes[0].end == 0x1900 && read.buffers.size() == 1 && read.buffers[0].source == CaptureBufferSource::Gds && read.buffers[0].element == 1 && read.buffers[0].after == blob, "the capture writes or buffers did not round trip");
    Require(read.images.size() == 1 && read.images[0].format == VK_FORMAT_BC7_UNORM_BLOCK && read.images[0].depth == 8 && read.images[0].contents.size() == 1 && read.images[0].contents[0].offset == 64 && read.images[0].contents[0].depth == 4 && read.images[0].written.size() == 1 && read.images[0].flags == VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT, "the capture images did not round trip");
    Require(read.views.size() == 1 && read.views[0].components == view.components && read.views[0].minLod == 1.25f && read.views[0].baseLevel == 1 && read.views[0].format == VK_FORMAT_BC7_SRGB_BLOCK && read.views[0].element == 1, "the capture views did not round trip");
    Require(read.samplers.size() == 1 && read.samplers[0].mipLodBias == -0.5f && read.samplers[0].maxAnisotropy == 16.0f && read.samplers[0].compareOp == VK_COMPARE_OP_LESS_OR_EQUAL && read.samplers[0].maxLod == 1000.0f, "the capture samplers did not round trip");
    Require(read.notes.size() == 1 && read.notes[0] == manifest.notes[0], "the capture notes did not round trip");
    std::uint64_t total = 0;
    const std::array<std::array<std::uint32_t, 3>, 3> subresources{{{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0}, {VK_IMAGE_ASPECT_COLOR_BIT, 2, 0}, {VK_IMAGE_ASPECT_COLOR_BIT, 4, 0}}};
    const auto layout = CaptureLayoutSubresources(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_IMAGE_TYPE_2D, {20, 12, 1}, subresources, total);
    Require(layout.size() == 3 && layout[0].size == 5u * 3u * 8u && layout[1].width == 5 && layout[1].height == 3 && layout[1].size == 2u * 8u && layout[2].width == 1 && layout[2].height == 1 && layout[2].size == 8 && layout[1].offset % 32 == 0 && total == layout[2].offset + 8, "block-compressed capture subresources are sized wrongly");
    const auto depth = CaptureLayoutSubresources(VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_TYPE_2D, {3, 3, 1}, std::array<std::array<std::uint32_t, 3>, 2>{{{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0}, {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0}}}, total);
    Require(depth[0].size == 36 && depth[1].size == 9 && depth[1].offset % 4 == 0 && CaptureFormatAspects(VK_FORMAT_D32_SFLOAT_S8_UINT) == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT), "depth and stencil capture subresources are sized wrongly");
    Require(CaptureBlockOf(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)->bytes == 8 && CaptureBlockOf(VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_ASPECT_COLOR_BIT)->bytes == 4 && CaptureBlockOf(VK_FORMAT_R8G8B8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT)->bytes == 3 && !CaptureBlockOf(VK_FORMAT_UNDEFINED, VK_IMAGE_ASPECT_COLOR_BIT), "capture texel sizes are wrong");
}

void bufferDispatchReplays(const Context& context, ReplayDevice& device, const std::filesystem::path& root) {
    constexpr std::uint64_t input = 0x40010u;
    constexpr std::uint64_t output = 0x80020u;
    constexpr std::uint32_t threads = 64u;
    constexpr std::uint32_t groups = 3u;
    constexpr std::uint32_t total = threads * groups;
    constexpr std::uint32_t count = 150u;
    constexpr std::uint32_t threshold = 70u;
    const auto program = Synthetic::PerThreadProgram({threads, 1u, 1u}, count, threshold, input, output, total);
    const auto request = program.Request(64u, device.SubgroupSize(), false);
    const auto compiled = ShaderRecompiler::Recompile(request);
    std::vector<TestRegion> regions;
    regions.push_back(makeRegion(context, input, total * 4u, false, inputPattern));
    regions.push_back(makeRegion(context, output, total * 4u, true, [](std::size_t) { return Sentinel; }));
    DispatchBindings bindings;
    std::vector<HarnessBinding> harness;
    std::vector<std::unique_ptr<Buffer>> data;
    for (const auto& binding : compiled.bindings) {
        Require(binding.kind == ShaderRecompiler::DescriptorKind::StorageBuffer, "the synthetic capture program binds a descriptor other than a storage buffer");
        bindings.layout.push_back({binding.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, binding.count, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
        HarnessBinding item{binding.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, {}, {}};
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            if (binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers) {
                const auto* words = binding.guestDescriptor.data() + element * 4u;
                const std::uint64_t address = words[0] | (static_cast<std::uint64_t>(words[1] & 0xffffu) << 32u);
                const std::uint32_t stride = (words[1] >> 16u) & 0x3fffu;
                const std::uint64_t size = stride == 0u ? words[2] : static_cast<std::uint64_t>(stride) * words[2];
                const auto region = std::find_if(regions.begin(), regions.end(), [&](const TestRegion& candidate) { return address >= candidate.begin && address + size <= candidate.end; });
                Require(region != regions.end(), "a synthetic guest buffer lies outside the test regions");
                const auto residue = address % 256u;
                bindings.buffers.push_back({binding.binding, element, CaptureBufferSource::Guest, address, size, HostMemory, {}});
                item.buffers.push_back({region->buffer->Handle(), address - residue - (region->begin - region->padding), size + residue});
            } else {
                Require(binding.role == ShaderRecompiler::DescriptorRole::ShaderData || binding.role == ShaderRecompiler::DescriptorRole::FlattenedSrt, "the synthetic capture program binds an unexpected descriptor role");
                const auto bytes = binding.guestDescriptor.size() * sizeof(std::uint32_t);
                auto& buffer = data.emplace_back(std::make_unique<Buffer>(context, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
                std::memcpy(buffer->Bytes().data(), binding.guestDescriptor.data(), bytes);
                bindings.buffers.push_back({binding.binding, element, CaptureBufferSource::Data, 0, bytes, HostMemory, std::span<const std::byte>(buffer->Bytes().data(), bytes)});
                item.buffers.push_back({buffer->Handle(), 0, bytes});
            }
        }
        harness.push_back(std::move(item));
    }
    for (auto& region : regions) bindings.regions.push_back(boundRegion(region));
    bindings.writes.push_back({output, output + total * 4u});
    std::vector<std::byte> push;
    if (!compiled.pushConstants.empty()) {
        const std::array<CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &compiled, 0}}};
        const auto block = AssemblePushConstants(shaders);
        push.assign(block.begin(), block.end());
    }
    CaptureTarget target{root, Synthetic::CodeAddress, 0x5eedull, 1, ShaderRecompiler::RequestSerializer{}.Serialize(request)};
    DispatchRecorder recorder(context, target, compiled, {groups, 1u, 1u}, std::move(bindings));
    recorder.Before();
    runHarness(context, compiled.spirv.Words(), harness, push, {groups, 1u, 1u});
    recorder.After();
    const auto directory = recorder.Finish();
    const auto produced = regions[1].buffer->Bytes().subspan(static_cast<std::size_t>(regions[1].padding));
    for (std::uint32_t index = 0; index < total; ++index) {
        const auto expected = index < count ? Synthetic::PerThreadExpected(index, inputPattern(index), threshold) : Sentinel;
        Require(wordAt(produced, index * 4u) == expected, "the synthetic capture dispatch wrote " + std::to_string(wordAt(produced, index * 4u)) + " instead of " + std::to_string(expected) + " for thread " + std::to_string(index));
    }
    const auto manifest = ReadCaptureManifest(directory);
    Require(directory.parent_path() == root && directory.filename().string().starts_with("20000-3x1x1-"), "the capture directory is named wrongly: " + directory.string());
    Require(manifest.program == Synthetic::CodeAddress && manifest.codeHash == 0x5eedull && manifest.groups == std::array<std::uint32_t, 3>{groups, 1u, 1u} && manifest.request == CaptureRequestName && manifest.notes.empty(), "the synthetic capture header is wrong");
    Require(manifest.regions.size() == 2 && manifest.writes.size() == 1 && manifest.writes[0].begin == output && manifest.writes[0].end == output + total * 4u, "the synthetic capture regions or writes are wrong");
    const auto reference = ReadCaptureBlob(root, manifest.writes[0].blob);
    Require(reference.size() == total * 4u && std::equal(reference.begin(), reference.end(), produced.begin()), "the captured reference output differs from what the dispatch wrote");
    const auto inputs = ReadCaptureBlob(root, manifest.regions[0].blob);
    Require(inputs.size() == regions[0].padding + total * 4u && wordAt(inputs, static_cast<std::size_t>(regions[0].padding) + 8u) == inputPattern(2), "the captured input region differs from the guest input");
    const auto outputs = ReadCaptureBlob(root, manifest.regions[1].blob);
    Require(wordAt(outputs, static_cast<std::size_t>(regions[1].padding)) == Sentinel, "the captured output region does not hold its contents before the dispatch");
    ReplayOptions options;
    options.iterations = 3;
    options.warmup = 1;
    const auto recompiled = ReplayCapture(device, directory, options);
    Require(recompiled.mismatches.empty() && recompiled.compared == 1 && recompiled.spirvMatchesCapture && recompiled.milliseconds.size() == 3 && recompiled.minimum > 0.0 && recompiled.median >= recompiled.minimum, "replaying a recompiled synthetic capture did not reproduce its output:\n" + AgcDriver::Replay::DescribeReplay(recompiled));
    options.shader = ReplayShader::Captured;
    options.memory = ReplayMemory::DeviceLocal;
    const auto captured = ReplayCapture(device, directory, options);
    Require(captured.mismatches.empty() && captured.compared == 1, "replaying the captured SPIR-V in device-local memory did not reproduce the output:\n" + AgcDriver::Replay::DescribeReplay(captured));
    std::vector<std::byte> original;
    flipBlob(root, manifest.writes[0].blob, original);
    const auto corrupted = ReplayCapture(device, directory, options);
    writeFile(root / CaptureBlobDirectory / manifest.writes[0].blob, original);
    Require(corrupted.mismatches.size() == 1 && corrupted.mismatches[0].find("1 of ") != std::string::npos, "a replay did not report a corrupted reference output:\n" + AgcDriver::Replay::DescribeReplay(corrupted));
}

void bdaDispatchReplays(const Context& context, ReplayDevice& device, const std::filesystem::path& root) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    constexpr std::uint64_t mapped = 0x7fff12340010ull;
    constexpr std::uint64_t unmapped = 0x7fff12370000ull;
    constexpr std::uint64_t output = 0x7fff12380000ull;
    constexpr std::uint32_t mappedBytes = 256u;
    for (const bool faulting : {false, true}) {
        TestRegion region{mapped, mapped + mappedBytes, mapped % 256u, false, nullptr};
        region.buffer = std::make_unique<Buffer>(context, static_cast<std::size_t>(region.padding + mappedBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        for (std::size_t byte = 0; byte < region.buffer->Bytes().size(); ++byte) region.buffer->Bytes()[byte] = static_cast<std::byte>((byte * 29u + 5u) & 0xffu);
        auto results = makeRegion(context, output, 16u, true, [](std::size_t) { return Sentinel; });
        const Abi::Header header{Abi::Version, 1u, sizeof(Abi::Range), 0u};
        const Abi::Range range{region.begin, region.end, region.buffer->DeviceAddress() + region.padding, Abi::Read, 0u};
        Buffer table(context, sizeof(header) + sizeof(range), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        std::memcpy(table.Bytes().data(), &header, sizeof(header));
        std::memcpy(table.Bytes().data() + sizeof(header), &range, sizeof(range));
        Buffer fault(context, sizeof(Abi::Fault), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        std::memset(fault.Bytes().data(), 0, fault.Bytes().size());
        ShaderRecompiler::RecompileResult shader;
        shader.spirv = MakeBdaTestShader(faulting ? unmapped : mapped + 0x24u, 32u);
        shader.bdaAbiVersion = Abi::Version;
        const auto outputWords = Synthetic::BufferDescriptor(output, 4u, 4u);
        for (const auto& [binding, role, words] : {std::tuple{0u, ShaderRecompiler::DescriptorRole::BdaPagetable, std::vector<std::uint32_t>{}}, std::tuple{1u, ShaderRecompiler::DescriptorRole::FaultBuffer, std::vector<std::uint32_t>{}}, std::tuple{2u, ShaderRecompiler::DescriptorRole::GuestBuffers, std::vector<std::uint32_t>(outputWords.begin(), outputWords.end())}}) {
            ShaderRecompiler::DescriptorBinding descriptor;
            descriptor.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
            descriptor.role = role;
            descriptor.binding = binding;
            descriptor.count = 1;
            descriptor.guestDescriptor = words;
            shader.bindings.push_back(std::move(descriptor));
        }
        DispatchBindings bindings;
        for (std::uint32_t binding = 0; binding < 3u; ++binding) bindings.layout.push_back({binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
        bindings.regions.push_back(boundRegion(region));
        bindings.regions.push_back(boundRegion(results));
        bindings.writes.push_back({output, output + 16u});
        bindings.buffers.push_back({0, 0, CaptureBufferSource::Table, 0, table.Bytes().size(), HostMemory, std::span<const std::byte>(table.Bytes().data(), table.Bytes().size())});
        bindings.buffers.push_back({1, 0, CaptureBufferSource::Fault, 0, fault.Bytes().size(), HostMemory, std::span<const std::byte>(fault.Bytes().data(), fault.Bytes().size())});
        bindings.buffers.push_back({2, 0, CaptureBufferSource::Guest, output, 16u, HostMemory, {}});
        const std::vector<HarnessBinding> harness{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, {{table.Handle(), 0, table.Bytes().size()}}, {}},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, {{fault.Handle(), 0, fault.Bytes().size()}}, {}},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, {{results.buffer->Handle(), 0, 16u}}, {}},
        };
        CaptureTarget target{root, faulting ? 0x1400bda100ull : 0x1400bda000ull, 0xbdaull, 1, {}};
        DispatchRecorder recorder(context, target, shader, {1u, 1u, 1u}, std::move(bindings));
        recorder.Before();
        runHarness(context, shader.spirv.Words(), harness, {}, {1u, 1u, 1u});
        recorder.After();
        const auto directory = recorder.Finish();
        Abi::Fault report{};
        std::memcpy(&report, fault.Bytes().data(), sizeof(report));
        const auto produced = wordAt(results.buffer->Bytes(), 0);
        const auto expected = faulting ? Sentinel : wordAt(region.buffer->Bytes(), static_cast<std::size_t>(region.padding) + 0x24u);
        Require(produced == expected && (faulting ? report.state == Abi::FaultState::Ready && report.reason == Abi::FaultReason::Unmapped : report.state == Abi::FaultState::Empty), std::string("the BDA capture dispatch ") + (faulting ? "did not fault on an unmapped address" : "read the wrong guest dword"));
        const auto manifest = ReadCaptureManifest(directory);
        Require(manifest.buffers.size() == 3 && !manifest.buffers[0].blob.empty() && !manifest.buffers[1].after.empty() && manifest.regions.size() == 2, "the BDA capture misses its page table, fault record or regions");
        ReplayOptions options;
        options.shader = ReplayShader::Captured;
        options.iterations = 2;
        options.warmup = 1;
        for (const auto memory : {ReplayMemory::Captured, ReplayMemory::DeviceLocal}) {
            options.memory = memory;
            const auto replayed = ReplayCapture(device, directory, options);
            Require(replayed.mismatches.empty() && replayed.compared == 2, std::string("replaying a BDA capture ") + (faulting ? "with a fault" : "through the page table") + " did not reproduce its outputs:\n" + AgcDriver::Replay::DescribeReplay(replayed));
        }
    }
}

void imageDispatchReplays(const Context& context, ReplayDevice& device, const std::filesystem::path& root) {
    constexpr std::uint32_t size = 16u;
    constexpr std::uint32_t levels = 3u;
    constexpr std::uint64_t output = 0x700000u;
    constexpr std::uint32_t salt = 0x5bd1e995u;
    const auto resolver = CaptureObjects::Wrap(context.deviceProc);
    const auto createImage = reinterpret_cast<PFN_vkCreateImage>(resolver(context.device, "vkCreateImage"));
    const auto destroyImage = reinterpret_cast<PFN_vkDestroyImage>(resolver(context.device, "vkDestroyImage"));
    const auto createView = reinterpret_cast<PFN_vkCreateImageView>(resolver(context.device, "vkCreateImageView"));
    const auto destroyView = reinterpret_cast<PFN_vkDestroyImageView>(resolver(context.device, "vkDestroyImageView"));
    Require(createImage != nullptr && destroyImage != nullptr && createView != nullptr && destroyView != nullptr, "the capture registry did not wrap the image functions");
    std::array<VkImage, 2> images{};
    std::array<VkDeviceMemory, 2> memories{};
    std::array<VkImageView, 2> views{};
    VkSampler sampler = VK_NULL_HANDLE;
    const auto release = [&] {
        if (sampler) context.Function<PFN_vkDestroySampler>("vkDestroySampler")(context.device, sampler, nullptr);
        for (const auto view : views) {
            if (view) destroyView(context.device, view, nullptr);
        }
        for (std::size_t index = 0; index < images.size(); ++index) {
            if (images[index]) destroyImage(context.device, images[index], nullptr);
            if (memories[index]) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memories[index], nullptr);
        }
    };
    try {
        const std::array<VkFormat, 2> formats{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32_UINT};
        const std::array<VkImageUsageFlags, 2> usages{VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
        for (std::size_t index = 0; index < images.size(); ++index) {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = formats[index];
            info.extent = {size, size, 1};
            info.mipLevels = index == 0 ? levels : 1u;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = usages[index];
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            Check(createImage(context.device, &info, nullptr, &images[index]), "vkCreateImage capture test");
            const auto registered = CaptureObjects::Image(images[index]);
            Require(registered && registered->copyable && (registered->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0 && registered->levels == info.mipLevels && registered->format == info.format, "the capture registry did not record a copyable image");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, images[index], &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memories[index]), "vkAllocateMemory capture test");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, images[index], memories[index], 0), "vkBindImageMemory capture test");
        }
        std::uint64_t sourceBytes = 0;
        for (std::uint32_t level = 0; level < levels; ++level) sourceBytes += static_cast<std::uint64_t>(size >> level) * (size >> level) * 4u;
        const auto targetOffset = sourceBytes;
        Buffer staging(context, static_cast<std::size_t>(sourceBytes + size * size * 4u), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        auto stagingBytes = staging.Bytes();
        for (std::size_t byte = 0; byte < sourceBytes; ++byte) stagingBytes[byte] = static_cast<std::byte>((byte * 37u + 11u) & 0xffu);
        for (std::uint32_t texel = 0; texel < size * size; ++texel) {
            const auto value = texel * 0x01000193u;
            std::memcpy(stagingBytes.data() + targetOffset + texel * 4u, &value, sizeof(value));
        }
        {
            CommandBatch batch(context);
            const auto commands = batch.Handle();
            std::array<VkImageMemoryBarrier, 2> barriers{};
            for (std::size_t index = 0; index < images.size(); ++index) {
                barriers[index] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, images[index], {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1}};
            }
            const auto pipelineBarrier = context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
            pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers.data());
            std::vector<VkBufferImageCopy> copies;
            std::uint64_t offset = 0;
            for (std::uint32_t level = 0; level < levels; ++level) {
                VkBufferImageCopy copy{};
                copy.bufferOffset = offset;
                copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
                copy.imageExtent = {size >> level, size >> level, 1};
                copies.push_back(copy);
                offset += static_cast<std::uint64_t>(size >> level) * (size >> level) * 4u;
            }
            const auto copyToImage = context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage");
            copyToImage(commands, staging.Handle(), images[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(copies.size()), copies.data());
            VkBufferImageCopy targetCopy{};
            targetCopy.bufferOffset = targetOffset;
            targetCopy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            targetCopy.imageExtent = {size, size, 1};
            copyToImage(commands, staging.Handle(), images[1], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &targetCopy);
            for (std::size_t index = 0; index < images.size(); ++index) {
                barriers[index].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barriers[index].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                barriers[index].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barriers[index].newLayout = index == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
            }
            pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers.data());
            batch.SubmitAndWait();
        }
        VkImageViewCreateInfo sampledInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        sampledInfo.image = images[0];
        sampledInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        sampledInfo.format = formats[0];
        sampledInfo.components = {VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE};
        sampledInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 2, 0, 1};
        Check(createView(context.device, &sampledInfo, nullptr, &views[0]), "vkCreateImageView capture test sampled");
        VkImageViewUsageCreateInfo storageUsage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO, nullptr, VK_IMAGE_USAGE_STORAGE_BIT};
        VkImageViewCreateInfo storageInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, &storageUsage};
        storageInfo.image = images[1];
        storageInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        storageInfo.format = formats[1];
        storageInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        Check(createView(context.device, &storageInfo, nullptr, &views[1]), "vkCreateImageView capture test storage");
        const auto registeredView = CaptureObjects::View(views[0]);
        Require(registeredView && registeredView->image == images[0] && registeredView->range.levelCount == 2 && registeredView->components.r == VK_COMPONENT_SWIZZLE_B && CaptureObjects::View(views[1])->usage == VK_IMAGE_USAGE_STORAGE_BIT, "the capture registry did not record the image views");
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 16.0f;
        Check(context.Function<PFN_vkCreateSampler>("vkCreateSampler")(context.device, &samplerInfo, nullptr, &sampler), "vkCreateSampler capture test");
        auto results = makeRegion(context, output, size * size * 4u, true, [](std::size_t) { return Sentinel; });
        ShaderRecompiler::RecompileResult shader;
        shader.spirv = std::vector<std::uint32_t>(std::begin(CAPTURE_REPLAY_SPV), std::end(CAPTURE_REPLAY_SPV));
        shader.pushConstants.resize(sizeof(salt));
        std::memcpy(shader.pushConstants.data(), &salt, sizeof(salt));
        const auto descriptor = [](ShaderRecompiler::DescriptorKind kind, ShaderRecompiler::DescriptorRole role, std::uint32_t binding, std::vector<std::uint32_t> words) {
            ShaderRecompiler::DescriptorBinding result;
            result.kind = kind;
            result.role = role;
            result.descriptorSet = 0;
            result.binding = binding;
            result.count = 1;
            result.guestDescriptor = std::move(words);
            return result;
        };
        const auto outputWords = Synthetic::BufferDescriptor(output, 4u, size * size);
        shader.bindings.push_back(descriptor(ShaderRecompiler::DescriptorKind::SampledImage, ShaderRecompiler::DescriptorRole::GuestImages, 0, {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u}));
        shader.bindings.push_back(descriptor(ShaderRecompiler::DescriptorKind::Sampler, ShaderRecompiler::DescriptorRole::GuestSamplers, 1, {0x1u, 0x2u, 0x3u, 0x4u}));
        shader.bindings.back().samplerDepthCompare = {false};
        shader.bindings.push_back(descriptor(ShaderRecompiler::DescriptorKind::StorageImage, ShaderRecompiler::DescriptorRole::GuestImages, 2, {0x99u, 0xaau, 0xbbu, 0xccu, 0xddu, 0xeeu, 0xffu, 0x100u}));
        shader.bindings.push_back(descriptor(ShaderRecompiler::DescriptorKind::StorageBuffer, ShaderRecompiler::DescriptorRole::GuestBuffers, 3, {outputWords.begin(), outputWords.end()}));
        DispatchBindings bindings;
        bindings.layout = {{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
        bindings.regions.push_back(boundRegion(results));
        bindings.writes.push_back({output, output + size * size * 4u});
        bindings.buffers.push_back({3, 0, CaptureBufferSource::Guest, output, size * size * 4u, HostMemory, {}});
        bindings.images.push_back({0, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, views[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        bindings.images.push_back({2, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, views[1], VK_IMAGE_LAYOUT_GENERAL});
        bindings.samplers.push_back({1, 0, samplerInfo});
        const std::array<CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
        const auto pushBlock = AssemblePushConstants(shaders);
        const std::vector<HarnessBinding> harness{
            {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, {}, {{VK_NULL_HANDLE, views[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}},
            {1, VK_DESCRIPTOR_TYPE_SAMPLER, {}, {{sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED}}},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, {}, {{VK_NULL_HANDLE, views[1], VK_IMAGE_LAYOUT_GENERAL}}},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, {{results.buffer->Handle(), output - (output % 256u) - (results.begin - results.padding), size * size * 4u + output % 256u}}, {}},
        };
        CaptureTarget target{root, 0x1400abc000ull, 0x77ull, 1, {}};
        DispatchRecorder recorder(context, target, shader, {size / 8u, size / 8u, 1u}, std::move(bindings));
        recorder.Before();
        runHarness(context, shader.spirv.Words(), harness, pushBlock, {size / 8u, size / 8u, 1u});
        recorder.After();
        const auto directory = recorder.Finish();
        const auto manifest = ReadCaptureManifest(directory);
        Require(manifest.notes.empty() && manifest.images.size() == 2 && manifest.views.size() == 2 && manifest.samplers.size() == 1 && manifest.request.empty(), "the image capture is incomplete");
        Require(manifest.images[0].levels == levels && manifest.images[0].contents.size() == 2 && manifest.images[0].written.empty() && manifest.images[0].after.empty() && manifest.images[1].contents.size() == 1 && manifest.images[1].written.size() == 1 && !manifest.images[1].after.empty(), "the image capture holds the wrong subresources");
        const auto before = ReadCaptureBlob(root, manifest.images[1].blob);
        const auto after = ReadCaptureBlob(root, manifest.images[1].after);
        Require(before.size() == size * size * 4u && wordAt(before, 4u * 5u) == 5u * 0x01000193u && after.size() == before.size() && after != before, "the storage image was not captured before and after the dispatch");
        const auto sampled = ReadCaptureBlob(root, manifest.images[0].blob);
        Require(sampled.size() >= manifest.images[0].contents[1].offset + 8u * 8u * 4u && manifest.images[0].contents[1].level == 1 && sampled[static_cast<std::size_t>(manifest.images[0].contents[1].offset)] == stagingBytes[size * size * 4u], "the sampled image mip 1 was not captured");
        ReplayOptions options;
        options.shader = ReplayShader::Captured;
        options.iterations = 3;
        options.warmup = 1;
        const auto replayed = ReplayCapture(device, directory, options);
        Require(replayed.mismatches.empty() && replayed.compared == 2 && replayed.spirvMatchesCapture, "replaying the image capture did not reproduce its outputs:\n" + AgcDriver::Replay::DescribeReplay(replayed));
        options.shader = ReplayShader::Recompile;
        bool rejected = false;
        try {
            static_cast<void>(ReplayCapture(device, directory, options));
        } catch (const std::exception& error) {
            rejected = std::string(error.what()).find("recompile request") != std::string::npos;
        }
        Require(rejected, "a capture without a recompile request was recompiled");
        options.shader = ReplayShader::Captured;
        std::vector<std::byte> original;
        flipBlob(root, manifest.images[1].after, original);
        const auto corrupted = ReplayCapture(device, directory, options);
        writeFile(root / CaptureBlobDirectory / manifest.images[1].after, original);
        Require(corrupted.mismatches.size() == 1 && corrupted.mismatches[0].starts_with("storage image 1"), "a replay did not report a corrupted storage image result:\n" + AgcDriver::Replay::DescribeReplay(corrupted));
        const auto destroyed = images[1];
        destroyView(context.device, views[1], nullptr);
        views[1] = VK_NULL_HANDLE;
        destroyImage(context.device, images[1], nullptr);
        images[1] = VK_NULL_HANDLE;
        Require(!CaptureObjects::Image(destroyed), "the capture registry kept a destroyed image");
    } catch (...) {
        release();
        throw;
    }
    release();
}

}

void RunDispatchCaptureFormatTests() {
    selectionParses();
    manifestRoundTrips();
}

void RunDispatchCaptureTests(const AgcDriver::Graphics::Context& context, PFN_vkGetInstanceProcAddr instanceProc) {
    ReplayDevice device(instanceProc);
    TemporaryDirectory root("anyps5-capture-replay");
    bufferDispatchReplays(context, device, root.path);
    bdaDispatchReplays(context, device, root.path);
    imageDispatchReplays(context, device, root.path);
}

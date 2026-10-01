#include <string_view>
#include <algorithm>
#include "BdaShader.hpp"
#include "ColorTransferTests.hpp"
#include <fstream>
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include <SDL_loadso.h>
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

std::vector<std::uint32_t> depthReadShader(const std::vector<bool>& integerImages) {
    enum : std::uint32_t { OpMemoryModel = 14, OpEntryPoint = 15, OpExecutionMode = 16, OpCapability = 17, OpTypeVoid = 19, OpTypeInt = 21, OpTypeFloat = 22, OpTypeVector = 23, OpTypeImage = 25, OpTypeRuntimeArray = 29, OpTypeStruct = 30, OpTypePointer = 32, OpTypeFunction = 33, OpConstant = 43, OpConstantComposite = 44, OpFunction = 54, OpFunctionEnd = 56, OpVariable = 59, OpLoad = 61, OpStore = 62, OpAccessChain = 65, OpDecorate = 71, OpMemberDecorate = 72, OpCompositeExtract = 81, OpImageFetch = 95, OpBitcast = 124, OpLabel = 248, OpReturn = 253 };
    enum : std::uint32_t { UniformConstant = 0, Uniform = 2, Dim2D = 1, ImageOperandsLod = 2, DecorationBufferBlock = 3, DecorationArrayStride = 6, DecorationBinding = 33, DecorationDescriptorSet = 34, DecorationOffset = 35, ModelGlCompute = 5, ModeLocalSize = 17, NameMain = 0x6e69616du };
    std::vector<std::uint32_t> annotations;
    std::vector<std::uint32_t> declarations;
    std::vector<std::uint32_t> body;
    std::uint32_t next = 1;
    const auto id = [&next] { return next++; };
    const auto emit = [](std::vector<std::uint32_t>& out, std::uint32_t opcode, std::initializer_list<std::uint32_t> operands) {
        out.push_back((static_cast<std::uint32_t>(operands.size() + 1) << 16u) | opcode);
        out.insert(out.end(), operands);
    };
    const auto voidType = id();
    const auto functionType = id();
    const auto floatType = id();
    const auto uintType = id();
    const auto intType = id();
    const auto int2 = id();
    const auto float4 = id();
    const auto uint4 = id();
    const auto floatImage = id();
    const auto uintImage = id();
    const auto floatImagePointer = id();
    const auto uintImagePointer = id();
    const auto words = id();
    const auto block = id();
    const auto blockPointer = id();
    const auto wordPointer = id();
    const auto zero = id();
    const auto origin = id();
    const auto output = id();
    const auto main = id();
    emit(declarations, OpTypeVoid, {voidType});
    emit(declarations, OpTypeFunction, {functionType, voidType});
    emit(declarations, OpTypeFloat, {floatType, 32});
    emit(declarations, OpTypeInt, {uintType, 32, 0});
    emit(declarations, OpTypeInt, {intType, 32, 1});
    emit(declarations, OpTypeVector, {int2, intType, 2});
    emit(declarations, OpTypeVector, {float4, floatType, 4});
    emit(declarations, OpTypeVector, {uint4, uintType, 4});
    emit(declarations, OpTypeImage, {floatImage, floatType, Dim2D, 0, 0, 0, 1, 0});
    emit(declarations, OpTypeImage, {uintImage, uintType, Dim2D, 0, 0, 0, 1, 0});
    emit(declarations, OpTypePointer, {floatImagePointer, UniformConstant, floatImage});
    emit(declarations, OpTypePointer, {uintImagePointer, UniformConstant, uintImage});
    emit(declarations, OpTypeRuntimeArray, {words, uintType});
    emit(declarations, OpTypeStruct, {block, words});
    emit(declarations, OpTypePointer, {blockPointer, Uniform, block});
    emit(declarations, OpTypePointer, {wordPointer, Uniform, uintType});
    emit(declarations, OpConstant, {intType, zero, 0});
    emit(declarations, OpConstantComposite, {int2, origin, zero, zero});
    emit(declarations, OpVariable, {blockPointer, output, Uniform});
    emit(annotations, OpDecorate, {output, DecorationDescriptorSet, 0});
    emit(annotations, OpDecorate, {output, DecorationBinding, 0});
    emit(annotations, OpDecorate, {words, DecorationArrayStride, 4});
    emit(annotations, OpDecorate, {block, DecorationBufferBlock});
    emit(annotations, OpMemberDecorate, {block, 0, DecorationOffset, 0});
    std::uint32_t slot = 0;
    for (std::uint32_t binding = 1; binding <= integerImages.size(); ++binding) {
        const bool integer = integerImages[binding - 1];
        const auto variable = id();
        const auto image = id();
        const auto texel = id();
        emit(declarations, OpVariable, {integer ? uintImagePointer : floatImagePointer, variable, UniformConstant});
        emit(annotations, OpDecorate, {variable, DecorationDescriptorSet, 0});
        emit(annotations, OpDecorate, {variable, DecorationBinding, binding});
        emit(body, OpLoad, {integer ? uintImage : floatImage, image, variable});
        emit(body, OpImageFetch, {integer ? uint4 : float4, texel, image, origin, ImageOperandsLod, zero});
        for (std::uint32_t component = 0; component < 4; ++component) {
            auto value = id();
            emit(body, OpCompositeExtract, {integer ? uintType : floatType, value, texel, component});
            if (!integer) {
                const auto bits = id();
                emit(body, OpBitcast, {uintType, bits, value});
                value = bits;
            }
            const auto index = id();
            const auto pointer = id();
            emit(declarations, OpConstant, {intType, index, slot++});
            emit(body, OpAccessChain, {wordPointer, pointer, output, zero, index});
            emit(body, OpStore, {pointer, value});
        }
    }
    std::vector<std::uint32_t> module{0x07230203u, 0x00010000u, 0, 0, 0};
    emit(module, OpCapability, {1});
    emit(module, OpMemoryModel, {0, 1});
    emit(module, OpEntryPoint, {ModelGlCompute, main, NameMain, 0});
    emit(module, OpExecutionMode, {main, ModeLocalSize, 1, 1, 1});
    module.insert(module.end(), annotations.begin(), annotations.end());
    module.insert(module.end(), declarations.begin(), declarations.end());
    emit(module, OpFunction, {voidType, main, 0, functionType});
    emit(module, OpLabel, {id()});
    module.insert(module.end(), body.begin(), body.end());
    emit(module, OpReturn, {});
    emit(module, OpFunctionEnd, {});
    module[3] = next;
    return module;
}

struct SampledDepthView {
    VkImageView view;
    bool integer;
};

class DepthSampler {
public:
    DepthSampler(const Context& context, const DepthImage& depth, std::span<const SampledDepthView> views, const Buffer& output) : context(context) {
        try {
            const auto& target = depth.Description();
            VkAttachmentDescription attachment{};
            attachment.format = target.format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.pDepthStencilAttachment = &reference;
            VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            passInfo.attachmentCount = 1;
            passInfo.pAttachments = &attachment;
            passInfo.subpassCount = 1;
            passInfo.pSubpasses = &subpass;
            Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &renderPass), "vkCreateRenderPass");
            const auto attachmentView = depth.View();
            VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &attachmentView;
            framebufferInfo.width = target.extent.width;
            framebufferInfo.height = target.extent.height;
            framebufferInfo.layers = 1;
            Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
            std::vector<VkDescriptorSetLayoutBinding> bindings{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
            std::vector<VkDescriptorImageInfo> images;
            std::vector<bool> integers;
            for (const auto& sampled : views) {
                bindings.push_back({static_cast<std::uint32_t>(bindings.size()), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
                images.push_back({VK_NULL_HANDLE, sampled.view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL});
                integers.push_back(sampled.integer);
            }
            VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
            setInfo.pBindings = bindings.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
            const std::array<VkDescriptorPoolSize, 2> sizes{{{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, static_cast<std::uint32_t>(images.size())}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}}};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
            poolInfo.pPoolSizes = sizes.data();
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1, &setLayout};
            Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets");
            const VkDescriptorBufferInfo buffer{output.Handle(), 0, VK_WHOLE_SIZE};
            std::vector<VkWriteDescriptorSet> writes(bindings.size(), VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET});
            for (std::size_t i = 0; i < writes.size(); ++i) {
                writes[i].dstSet = set;
                writes[i].dstBinding = bindings[i].binding;
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = bindings[i].descriptorType;
                if (i == 0) writes[i].pBufferInfo = &buffer;
                else writes[i].pImageInfo = &images[i - 1];
            }
            context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &setLayout;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout");
            const auto code = depthReadShader(integers);
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            moduleInfo.codeSize = code.size() * sizeof(std::uint32_t);
            moduleInfo.pCode = code.data();
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
            pipelineInfo.layout = layout;
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
        } catch (...) { release(); throw; }
    }

    ~DepthSampler() { release(); }

    void Clear(VkCommandBuffer commands, VkExtent2D extent, float depthValue, std::uint32_t stencilValue) const {
        VkClearValue clear{};
        clear.depthStencil = {depthValue, stencilValue};
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = renderPass;
        begin.framebuffer = framebuffer;
        begin.renderArea = {{0, 0}, extent};
        begin.clearValueCount = 1;
        begin.pClearValues = &clear;
        context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
        context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
    }

    void Sample(VkCommandBuffer commands) const {
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, 1, 1, 1);
        VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
    }

private:
    void release() noexcept {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (setLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
        if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
        if (renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, renderPass, nullptr);
    }

    const Context& context;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

void checkDepthViews(const Context& context, bool graphicsQueue) {
    constexpr VkFormat format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    if (!graphicsQueue || (properties.optimalTilingFeatures & required) != required) return;
    constexpr VkExtent2D extent{16, 8};
    constexpr float depthValue = 0.625f;
    constexpr std::uint32_t stencilValue = 0x5a;
    DepthImage depth(context, {0x100000, extent, format, true, 0x200000});
    Require(depth.Sampled(), "a depth image of a format the device can sample was created without sampled usage");
    const VkComponentMapping identity{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    const VkComponentMapping swizzle{VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R};
    const auto depthView = depth.SampledView(false, false, identity);
    const auto stencilView = depth.SampledView(true, false, identity);
    const auto swizzledView = depth.SampledView(false, false, swizzle);
    Require(depthView != VK_NULL_HANDLE && stencilView != VK_NULL_HANDLE && swizzledView != VK_NULL_HANDLE && depthView != stencilView && swizzledView != depthView && depth.SampledView(false, false, identity) == depthView && depth.SampledView(false, true, identity) != VK_NULL_HANDLE, "the device rejected the depth, stencil, swizzled or array views of a sampled depth image");
    const std::array<SampledDepthView, 3> views{{{depthView, false}, {stencilView, true}, {swizzledView, false}}};
    Buffer output(context, 4 * views.size() * sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memset(output.Bytes().data(), 0xff, output.Bytes().size());
    {
        DepthSampler sampler(context, depth, views, output);
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        depth.Prepare(commands, true);
        sampler.Clear(commands, extent, depthValue, stencilValue);
        depth.Transition(commands, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        sampler.Sample(commands);
        depth.Prepare(commands, false);
        batch.SubmitAndWait();
    }
    std::array<std::uint32_t, 12> read{};
    std::memcpy(read.data(), output.Bytes().data(), sizeof(read));
    const auto one = std::bit_cast<std::uint32_t>(1.0f);
    const auto sampled = std::bit_cast<std::uint32_t>(depthValue);
    const std::array<std::uint32_t, 12> expected{sampled, 0, 0, one, stencilValue, 0, 0, 1, one, sampled, 0, sampled};
    if (read != expected) {
        std::string values;
        for (const auto value : read) values += " " + std::to_string(value);
        Require(false, "the depth, stencil and swizzled depth views of a read-only depth image sampled" + values + " instead of the cleared depth and stencil as (value, 0, 0, 1) under each view's swizzle");
    }
    Require(depth.Attached() && depth.Generation() == 1, "a depth pass that cannot write did not return the sampled depth image to the attachment layout or advanced its generation");
}

void checkGpuTimestamps(const Context& device) {
    if (!device.limits.timestampComputeAndGraphics) return;
    auto context = device;
    context.batchTimestamps = 8;
    constexpr VkDeviceSize slice = 4u << 20u;
    Buffer target(context, 4 * slice, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    const auto fill = context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer");
    CommandBatch batch(context);
    for (std::uint32_t region = 0; region < 4; ++region) {
        batch.BeginRegion(GpuWork::Dispatch, 0x1400000000ull + region, {region + 1, 1, 1});
        fill(batch.Handle(), target.Handle(), region * slice, slice, region);
        batch.EndRegion();
    }
    batch.SubmitAndWait();
    const auto regions = batch.Regions();
    Require(regions.size() == 3 && batch.DroppedRegions() == 1 && batch.gpuTime.count() > 0, "the device did not time a batch whose timestamp pool filled up");
    std::chrono::nanoseconds timed{};
    for (const auto& region : regions) timed += region.time;
    Require(timed.count() > 0 && timed <= batch.gpuTime, "the timed regions of a batch took no GPU time or more than the whole batch");
    CommandBatch oneOff(context, "resolve");
    fill(oneOff.Handle(), target.Handle(), 0, slice, 7);
    oneOff.SubmitAndWait();
    Require(oneOff.gpuTime.count() > 0 && oneOff.gpuTime <= std::chrono::seconds(1), "the device did not time a one-off batch");
}

class Device {
public:
    Device() {
        library = SDL_LoadObject(AgcDriver::ResolveVulkanLibrary_nid_no_patch());
        Require(library != nullptr, "cannot load Vulkan");
        try {
            instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
            Require(instanceProc != nullptr, "missing Vulkan instance resolver");
            VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            application.apiVersion = VK_API_VERSION_1_1;
            VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            info.pApplicationInfo = &application;
#if defined(__APPLE__)
            const char* portability = "VK_KHR_portability_enumeration";
            info.flags = 0x00000001;
            info.enabledExtensionCount = 1;
            info.ppEnabledExtensionNames = &portability;
#endif
            Check(function<PFN_vkCreateInstance>("vkCreateInstance")(&info, nullptr, &instance), "vkCreateInstance");
            std::uint32_t count = 0;
            const auto enumerate = function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
            Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
            Require(count != 0, "no Vulkan device");
            std::vector<VkPhysicalDevice> devices(count);
            Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
            context.physical = devices.front();
            const auto extensions = function<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            Check(extensions(context.physical, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> available(count);
            Check(extensions(context.physical, nullptr, &count, available.data()), "vkEnumerateDeviceExtensionProperties");
            auto bytes = AgcDriver::QueryBdaByteFeatures(context.physical, function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), available);
            auto address = AgcDriver::QueryBdaFeatures(context.physical, function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), available);
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) ++family;
            Require(family < count, "no Vulkan compute queue");
            graphicsQueue = (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            const float priority = 1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkPhysicalDeviceFeatures enabled{};
            enabled.shaderInt64 = VK_TRUE;
            address.pNext = &bytes;
            std::vector<const char*> extensionsEnabled{VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME};
            if (std::any_of(available.begin(), available.end(), [](const auto& extension) { return std::string_view(extension.extensionName) == "VK_KHR_portability_subset"; })) extensionsEnabled.push_back("VK_KHR_portability_subset");
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &address};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            device.enabledExtensionCount = static_cast<std::uint32_t>(extensionsEnabled.size());
            device.ppEnabledExtensionNames = extensionsEnabled.data();
            device.pEnabledFeatures = &enabled;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            context.deviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
            context.imageFormatProperties = function<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties");
            VkPhysicalDeviceProperties properties{};
            function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
            context.limits = properties.limits;
            context.bufferDeviceAddress = true;
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool.queueFamilyIndex = family;
            Check(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")(context.device, &pool, nullptr, &context.pool), "vkCreateCommandPool");
        } catch (...) {
            release();
            throw;
        }
    }

    ~Device() { release(); }
    const Context& GetContext() const { return context; }
    bool GraphicsQueue() const { return graphicsQueue; }

private:
    template<typename TFunction>
    TFunction function(const char* name) const {
        const auto result = reinterpret_cast<TFunction>(instanceProc(instance, name));
        Require(result != nullptr, name);
        return result;
    }

    void release() noexcept {
        if (context.pool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(context.device, context.pool, nullptr);
        context.bufferPool.reset();
        if (context.device != VK_NULL_HANDLE) function<PFN_vkDestroyDevice>("vkDestroyDevice")(context.device, nullptr);
        if (instance != VK_NULL_HANDLE) function<PFN_vkDestroyInstance>("vkDestroyInstance")(instance, nullptr);
        if (library != nullptr) SDL_UnloadObject(library);
    }

    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    Context context{};
    bool graphicsQueue = false;
};

}

int main(int argc, char** argv) {
    try {
        RunBdaContractTests();
        if (argc == 2) {
            const auto shader = MakeBdaTestShader(0x7fff12340000ULL, 32);
            std::ofstream file(argv[1], std::ios::binary);
            file.write(reinterpret_cast<const char*>(shader.data()), static_cast<std::streamsize>(shader.size() * sizeof(std::uint32_t)));
            Require(static_cast<bool>(file), "cannot save BDA test SPIR-V");
            return 0;
        }
        Device device;
        Buffer buffer(device.GetContext(), 256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        Require(buffer.DeviceAddress() != 0 && buffer.Bytes().size() == 256, "invalid real BDA buffer");
        buffer.Bytes()[255] = std::byte{0x5a};
        Require(buffer.Bytes()[255] == std::byte{0x5a}, "real BDA buffer mapping failed");
        RunBdaExecutionTests(device.GetContext());
        RunRecompiledShaderTests(device.GetContext());
        RunColorTransferTests(device.GetContext());
        checkDepthViews(device.GetContext(), device.GraphicsQueue());
        checkGpuTimestamps(device.GetContext());
        std::cout << "Vulkan BDA allocation and execution tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

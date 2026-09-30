#include "prx/libSceAgcDriver/Execution/include/GpuJournal.hpp"
#include "BdaAbi.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GraphicsPipelineCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationScaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/SwapchainState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReleaseQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DescriptorCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlowPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include <chrono>
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseSleep.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanLibrary.hpp"
#include <SDL_loadso.h>
#include <SDL_error.h>
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <vector>
#include <utility>
#include <unordered_map>

namespace AgcDriver {

constexpr std::size_t GdsBytes = 64 * 1024;

namespace {

constexpr const char* PortabilityEnumerationExtension = "VK_KHR_portability_enumeration";
constexpr VkInstanceCreateFlags PortabilityEnumerationFlag = 0x00000001;
constexpr const char* PortabilitySubsetExtension = "VK_KHR_portability_subset";

void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + ": Vulkan result " + std::to_string(result));
    }
}

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("Vulkan presentation: ") + reason);
}

}

struct VulkanDevice::State {
    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    PFN_vkGetDeviceProcAddr deviceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    void* window = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    SwapchainState swapchainState;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    VkFence acquireFence = VK_NULL_HANDLE;
    VkFence renderFence = VK_NULL_HANDLE;
    bool renderPending = false;
    std::shared_ptr<Graphics::ResidentColor> presentedTarget;
    mutable std::mutex hostRangesMutex;
    std::shared_ptr<const std::vector<std::pair<std::uint64_t, std::uint64_t>>> hostRanges = std::make_shared<const std::vector<std::pair<std::uint64_t, std::uint64_t>>>();
    std::vector<std::pair<std::uint64_t, std::uint64_t>> hostRangesScratch;
    void PublishHostRanges() {
        hostRangesScratch.clear();
        if (renderCache) renderCache->AppendColorRanges(hostRangesScratch);
        if (drawQueue) drawQueue->AppendWriteRanges(hostRangesScratch);
        if (hostRangesScratch == *hostRanges) return;
        auto ranges = std::make_shared<const std::vector<std::pair<std::uint64_t, std::uint64_t>>>(hostRangesScratch);
        std::lock_guard lock(hostRangesMutex);
        hostRanges = std::move(ranges);
    }
    std::mutex ticketMutex;
    std::uint64_t ticketsIssued = 0;
    std::uint64_t ticketsDone = 0;
    struct RetiredSwapchain {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        std::vector<VkSemaphore> rendered;
    };
    std::vector<VkSemaphore> rendered;
    std::vector<RetiredSwapchain> retiredSwapchains;
    VkCommandBuffer clearCommands = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    VkBuffer uploadBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uploadMemory = VK_NULL_HANDLE;
    void* uploadMapping = nullptr;
    VkDeviceSize uploadSize = 0;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    std::vector<std::uint32_t> capabilities{1};
    std::vector<std::string_view> spirvExtensions;
    bool tessellationShader = false;
    bool meshShader = false;
    bool fragmentShaderBarycentric = false;
    bool depthClipControl = false;
    bool depthClamp = false;
    bool depthRangeUnrestricted = false;
    bool hostPointerImport = false;
    VkDeviceSize hostPointerAlignment = 0;
    bool samplerAnisotropy = false;
    bool textureCompressionBC = false;
    bool storageImages = false;
    std::unique_ptr<Graphics::TextureDetiler> detiler;
    std::unique_ptr<Graphics::GpuColorTransfer> colorTransfer;
    std::shared_ptr<Graphics::BufferPool> bufferPool;
    std::shared_ptr<Graphics::ReleaseQueue> releaseQueue;
    std::unique_ptr<Graphics::GuestBufferCache> guestBufferCache;
    std::unique_ptr<Graphics::Buffer> gds;
    std::uint64_t idleSubmissions = ~0ull;
    std::shared_ptr<Graphics::DescriptorCache> descriptorCache;
    std::shared_ptr<Graphics::SamplerCache> samplerCache;
    std::unique_ptr<Graphics::TextureCache> textureCache;
    std::unique_ptr<Graphics::PipelineCache> pipelineCache;
    struct ComputePipeline {
        VkShaderModule module = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
    };
    std::unordered_map<std::string, ComputePipeline> computePipelines;
    void destroyComputePipelines() {
        const auto destroyModule = reinterpret_cast<PFN_vkDestroyShaderModule>(deviceProc(device, "vkDestroyShaderModule"));
        const auto destroyLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(deviceProc(device, "vkDestroyPipelineLayout"));
        const auto destroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(deviceProc(device, "vkDestroyPipeline"));
        for (auto& [key, entry] : computePipelines) {
            if (entry.pipeline) destroyPipeline(device, entry.pipeline, nullptr);
            if (entry.layout) destroyLayout(device, entry.layout, nullptr);
            if (entry.module) destroyModule(device, entry.module, nullptr);
        }
        computePipelines.clear();
    }
    std::unique_ptr<Graphics::DrawQueue> drawQueue;
    std::unique_ptr<Graphics::RenderCache> renderCache;
    std::unique_ptr<Graphics::GraphicsPipelineCache> graphicsPipelines;
    VkPhysicalDeviceMeshShaderPropertiesEXT meshLimits{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
    std::unique_ptr<PresentationScaler> scaler;
    std::unique_ptr<PresentationScaler> rgbaScaler;

    template<typename TFunction>
    TFunction InstanceFunction(const char* name) const {
        auto function = reinterpret_cast<TFunction>(instanceProc(instance, name));
        if (function == nullptr) {
            throw std::runtime_error(std::string("Vulkan instance function missing: ") + name);
        }
        return function;
    }

    template<typename TFunction>
    TFunction DeviceFunction(const char* name) const {
        auto function = reinterpret_cast<TFunction>(deviceProc(device, name));
        if (function == nullptr) {
            throw std::runtime_error(std::string("Vulkan device function missing: ") + name);
        }
        return function;
    }

    void Upload(std::span<const std::byte> pixels) {
        if (uploadSize < pixels.size()) {
            if (uploadBuffer || uploadMemory) {
                Graphics::Release(releaseQueue, [device = device, unmap = uploadMapping ? DeviceFunction<PFN_vkUnmapMemory>("vkUnmapMemory") : nullptr, destroyBuffer = DeviceFunction<PFN_vkDestroyBuffer>("vkDestroyBuffer"), freeMemory = DeviceFunction<PFN_vkFreeMemory>("vkFreeMemory"), buffer = uploadBuffer, memory = uploadMemory] {
                    if (unmap) unmap(device, memory);
                    if (buffer) destroyBuffer(device, buffer, nullptr);
                    if (memory) freeMemory(device, memory, nullptr);
                });
            }
            uploadMapping = nullptr;
            uploadBuffer = VK_NULL_HANDLE;
            uploadMemory = VK_NULL_HANDLE;
            uploadSize = 0;
            VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            buffer.size = pixels.size();
            buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            check(DeviceFunction<PFN_vkCreateBuffer>("vkCreateBuffer")(device, &buffer, nullptr, &uploadBuffer), "vkCreateBuffer display upload");
            VkMemoryRequirements requirements{};
            DeviceFunction<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(device, uploadBuffer, &requirements);
            std::uint32_t memoryType = memoryProperties.memoryTypeCount;
            const auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
                if ((requirements.memoryTypeBits & (1u << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags) {
                    memoryType = i;
                    break;
                }
            }
            require(memoryType < memoryProperties.memoryTypeCount, "coherent host upload memory is unavailable");
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = memoryType;
            check(DeviceFunction<PFN_vkAllocateMemory>("vkAllocateMemory")(device, &allocation, nullptr, &uploadMemory), "vkAllocateMemory display upload");
            check(DeviceFunction<PFN_vkBindBufferMemory>("vkBindBufferMemory")(device, uploadBuffer, uploadMemory, 0), "vkBindBufferMemory display upload");
            check(DeviceFunction<PFN_vkMapMemory>("vkMapMemory")(device, uploadMemory, 0, pixels.size(), 0, &uploadMapping), "vkMapMemory display upload");
            uploadSize = pixels.size();
        }
        std::memcpy(uploadMapping, pixels.data(), pixels.size());
    }

    void DestroyRetiredSwapchains() {
        if (retiredSwapchains.empty()) return;
        const auto destroySemaphore = DeviceFunction<PFN_vkDestroySemaphore>("vkDestroySemaphore");
        const auto destroySwapchain = DeviceFunction<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR");
        for (const auto& retired : retiredSwapchains) {
            for (auto semaphore : retired.rendered) {
                if (semaphore) destroySemaphore(device, semaphore, nullptr);
            }
            destroySwapchain(device, retired.swapchain, nullptr);
        }
        retiredSwapchains.clear();
    }

    ~State() {
        std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
        if (device != VK_NULL_HANDLE) {
            if (drawQueue) drawQueue->Wait();
            if (renderCache) renderCache->Flush();
            const auto idle = reinterpret_cast<PFN_vkDeviceWaitIdle>(deviceProc(device, "vkDeviceWaitIdle"))(device);
            if (idle != VK_SUCCESS && idle != VK_ERROR_DEVICE_LOST) std::terminate();
            drawQueue.reset();
            graphicsPipelines.reset();
            destroyComputePipelines();
            guestBufferCache.reset();
            gds.reset();
            presentedTarget.reset();
            renderCache.reset();
            textureCache.reset();
            detiler.reset();
            colorTransfer.reset();
            scaler.reset();
            rgbaScaler.reset();
            pipelineCache.reset();
            bufferPool.reset();
            descriptorCache.reset();
            samplerCache.reset();
            if (releaseQueue) releaseQueue->Close();
            releaseQueue.reset();
            const auto destroyFence = reinterpret_cast<PFN_vkDestroyFence>(deviceProc(device, "vkDestroyFence"));
            if (acquireFence) destroyFence(device, acquireFence, nullptr);
            if (renderFence) destroyFence(device, renderFence, nullptr);
            const auto destroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(deviceProc(device, "vkDestroySemaphore"));
            for (auto semaphore : rendered) {
                if (semaphore) destroySemaphore(device, semaphore, nullptr);
            }
            DestroyRetiredSwapchains();
            if (uploadMapping) reinterpret_cast<PFN_vkUnmapMemory>(deviceProc(device, "vkUnmapMemory"))(device, uploadMemory);
            if (uploadBuffer) reinterpret_cast<PFN_vkDestroyBuffer>(deviceProc(device, "vkDestroyBuffer"))(device, uploadBuffer, nullptr);
            if (uploadMemory) reinterpret_cast<PFN_vkFreeMemory>(deviceProc(device, "vkFreeMemory"))(device, uploadMemory, nullptr);
            if (swapchain) reinterpret_cast<PFN_vkDestroySwapchainKHR>(deviceProc(device, "vkDestroySwapchainKHR"))(device, swapchain, nullptr);
            const auto destroyPool = reinterpret_cast<PFN_vkDestroyCommandPool>(deviceProc(device, "vkDestroyCommandPool"));
            const auto destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(deviceProc(device, "vkDestroyDevice"));
            if (pool != VK_NULL_HANDLE) {
                destroyPool(device, pool, nullptr);
            }
            destroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            if (surface) reinterpret_cast<PFN_vkDestroySurfaceKHR>(instanceProc(instance, "vkDestroySurfaceKHR"))(instance, surface, nullptr);
            reinterpret_cast<PFN_vkDestroyInstance>(instanceProc(instance, "vkDestroyInstance"))(instance, nullptr);
        }
        if (library != nullptr) {
            SDL_UnloadObject(library);
        }
    }
};

VulkanDevice::VulkanDevice(const PresentationWindow* window) : state(std::make_unique<State>()) {
    state->library = SDL_LoadObject(ResolveVulkanLibrary_nid_no_patch());
    if (state->library == nullptr) {
        throw std::runtime_error(std::string("Vulkan loader: ") + SDL_GetError());
    }
    state->instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(state->library, "vkGetInstanceProcAddr"));
    if (state->instanceProc == nullptr) {
        throw std::runtime_error("Vulkan loader: vkGetInstanceProcAddr missing");
    }
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "AnyPS5 libSceAgcDriver";
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pApplicationInfo = &application;
    std::vector<const char*> instanceExtensions;
    std::uint32_t availableCount = 0;
    const auto enumerateExtensions = state->InstanceFunction<PFN_vkEnumerateInstanceExtensionProperties>("vkEnumerateInstanceExtensionProperties");
    check(enumerateExtensions(nullptr, &availableCount, nullptr), "vkEnumerateInstanceExtensionProperties");
    std::vector<VkExtensionProperties> availableInstanceExtensions(availableCount);
    check(enumerateExtensions(nullptr, &availableCount, availableInstanceExtensions.data()), "vkEnumerateInstanceExtensionProperties");
    const auto hasInstanceExtension = [&](const char* name) { return std::any_of(availableInstanceExtensions.begin(), availableInstanceExtensions.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; }); };
    if (window != nullptr) {
        require(window->context && window->createSurface && window->getDrawableSize && window->width && window->height, "invalid window descriptor");
        instanceExtensions.assign(window->extensions.begin(), window->extensions.end());
        for (const auto* name : instanceExtensions) {
            require(name != nullptr, "null instance extension");
            if (!hasInstanceExtension(name)) throw std::runtime_error(std::string("Vulkan presentation: required instance extension missing: ") + name);
        }
    }
    if (hasInstanceExtension(PortabilityEnumerationExtension)) {
        instanceExtensions.push_back(PortabilityEnumerationExtension);
        create.flags |= PortabilityEnumerationFlag;
    }
    const bool gpuLabels = std::getenv("ANYPS5_DEBUG_GPU_LABELS") != nullptr && hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (gpuLabels) instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    create.enabledExtensionCount = static_cast<std::uint32_t>(instanceExtensions.size());
    create.ppEnabledExtensionNames = instanceExtensions.data();
    check(state->InstanceFunction<PFN_vkCreateInstance>("vkCreateInstance")(&create, nullptr, &state->instance), "vkCreateInstance");
    if (gpuLabels) GpuJournal::CommandLabel = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(state->instanceProc(state->instance, "vkCmdInsertDebugUtilsLabelEXT"));
    if (window != nullptr) {
        state->surface = window->createSurface(window->context, state->instance);
        require(state->surface != VK_NULL_HANDLE, "window returned a null surface");
        state->window = window->context;
    }
    state->deviceProc = state->InstanceFunction<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
    const auto enumerate = state->InstanceFunction<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
    std::uint32_t count = 0;
    check(enumerate(state->instance, &count, nullptr), "vkEnumeratePhysicalDevices");
    std::vector<VkPhysicalDevice> devices(count);
    check(enumerate(state->instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
    devices.resize(count);
    VkPhysicalDevice selected = VK_NULL_HANDLE;
    std::uint32_t family = 0;
    const std::array<const char*, 1> presentationExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (auto physical : devices) {
        VkPhysicalDeviceProperties properties{};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(physical, &properties);
        if (properties.apiVersion < VK_API_VERSION_1_1) {
            continue;
        }
        if (window != nullptr) {
            std::uint32_t extensionCount = 0;
            auto enumerateExtensions = state->InstanceFunction<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            check(enumerateExtensions(physical, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> extensions(extensionCount);
            check(enumerateExtensions(physical, nullptr, &extensionCount, extensions.data()), "vkEnumerateDeviceExtensionProperties");
            const bool supported = std::all_of(presentationExtensions.begin(), presentationExtensions.end(), [&](const char* name) {
                return std::any_of(extensions.begin(), extensions.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; });
            });
            if (!supported) continue;
        }
        std::uint32_t families = 0;
        auto getFamilies = state->InstanceFunction<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        getFamilies(physical, &families, nullptr);
        std::vector<VkQueueFamilyProperties> queues(families);
        getFamilies(physical, &families, queues.data());
        for (std::uint32_t i = 0; i < families; ++i) {
            if (queues[i].queueCount != 0 && (queues[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                if (window != nullptr) {
                    VkBool32 supported = VK_FALSE;
                    check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>("vkGetPhysicalDeviceSurfaceSupportKHR")(physical, i, state->surface, &supported), "vkGetPhysicalDeviceSurfaceSupportKHR");
                    if (!supported) continue;
                }
                selected = physical;
                family = i;
                break;
            }
        }
        if (selected != VK_NULL_HANDLE) {
            break;
        }
    }
    if (selected == VK_NULL_HANDLE) {
        throw std::runtime_error(window ? "Vulkan: no Vulkan 1.1 device with graphics, compute and swapchain presentation" : "Vulkan: no Vulkan 1.1 graphics and compute queue");
    }
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    properties.pNext = &state->subgroup;
    state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
    state->properties = properties.properties;
    state->physical = selected;
    if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT) != 0) {
        state->capabilities.push_back(spv::CapabilityGroupNonUniform);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformBallot);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformShuffle);
    }
    state->InstanceFunction<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(selected, &state->memoryProperties);
    std::uint32_t extensionCount = 0;
    const auto enumerateDeviceExtensions = state->InstanceFunction<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
    check(enumerateDeviceExtensions(selected, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    check(enumerateDeviceExtensions(selected, nullptr, &extensionCount, availableExtensions.data()), "vkEnumerateDeviceExtensionProperties");
    const auto hasExtension = [&](const char* name) { return std::any_of(availableExtensions.begin(), availableExtensions.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; }); };
    auto byteFeatures = QueryBdaByteFeatures(selected, state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), availableExtensions);
    auto bdaFeatures = QueryBdaFeatures(selected, state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), availableExtensions);
    require(hasExtension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME), "VK_KHR_shader_float_controls is unavailable");
    VkPhysicalDeviceFloatControlsProperties floatControls{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES};
    VkPhysicalDeviceProperties2 floatProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &floatControls};
    state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &floatProperties);
    require(floatControls.shaderSignedZeroInfNanPreserveFloat32 == VK_TRUE, "shaderSignedZeroInfNanPreserveFloat32 is unavailable");
    const std::array<const char*, 2> meshExtensions{VK_EXT_MESH_SHADER_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME};
    const bool meshAvailable = std::all_of(meshExtensions.begin(), meshExtensions.end(), hasExtension);
    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    if (meshAvailable) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &meshFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->meshShader = meshFeatures.meshShader == VK_TRUE;
        VkPhysicalDeviceProperties2 meshProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &state->meshLimits};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &meshProperties);
    }
    meshFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    meshFeatures.meshShader = state->meshShader;
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentricFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR};
    if (hasExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &barycentricFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->fragmentShaderBarycentric = barycentricFeatures.fragmentShaderBarycentric == VK_TRUE;
    }
    std::vector<const char*> deviceExtensions;
    if (window != nullptr) deviceExtensions.assign(presentationExtensions.begin(), presentationExtensions.end());
    if (state->fragmentShaderBarycentric) {
        deviceExtensions.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityFragmentBarycentricKHR);
        state->spirvExtensions.push_back("SPV_KHR_fragment_shader_barycentric");
    }
    if (hasExtension(PortabilitySubsetExtension)) deviceExtensions.push_back(PortabilitySubsetExtension);
    deviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    state->capabilities.push_back(spv::CapabilitySignedZeroInfNanPreserve);
    state->capabilities.push_back(spv::CapabilitySampled1D);
    state->capabilities.push_back(spv::CapabilityImage1D);
    state->spirvExtensions.push_back("SPV_KHR_float_controls");
    deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
    state->capabilities.push_back(4448);
    state->spirvExtensions.push_back("SPV_KHR_8bit_storage");
    state->capabilities.push_back(11);
    state->capabilities.push_back(5347);
    state->spirvExtensions.push_back("SPV_KHR_physical_storage_buffer");
    state->depthRangeUnrestricted = hasExtension(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME);
    if (state->depthRangeUnrestricted) deviceExtensions.push_back(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME);
    if (hasExtension(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME) && std::getenv("ANYPS5_NO_HOST_IMPORT") == nullptr) {
        VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &hostProperties};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties2);
        if (hostProperties.minImportedHostPointerAlignment != 0 && (hostProperties.minImportedHostPointerAlignment & (hostProperties.minImportedHostPointerAlignment - 1)) == 0) {
            state->hostPointerImport = true;
            state->hostPointerAlignment = hostProperties.minImportedHostPointerAlignment;
            deviceExtensions.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
            if (hasExtension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME)) deviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
        }
    }
    VkPhysicalDeviceDepthClipControlFeaturesEXT depthClipFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT};
    if (hasExtension(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &depthClipFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->depthClipControl = depthClipFeatures.depthClipControl == VK_TRUE;
        if (state->depthClipControl) deviceExtensions.push_back(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);
    }
    if (state->meshShader) {
        deviceExtensions.insert(deviceExtensions.end(), meshExtensions.begin(), meshExtensions.end());
        state->capabilities.push_back(5283);
        state->spirvExtensions.push_back("SPV_EXT_mesh_shader");
    }
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = family;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    VkPhysicalDeviceFeatures available{};
    state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures>("vkGetPhysicalDeviceFeatures")(selected, &available);
    require(available.vertexPipelineStoresAndAtomics && available.fragmentStoresAndAtomics, "graphics shader buffer writes and atomics are unavailable");
    VkPhysicalDeviceFeatures enabled{};
    enabled.shaderInt64 = VK_TRUE;
    enabled.vertexPipelineStoresAndAtomics = VK_TRUE;
    enabled.fragmentStoresAndAtomics = VK_TRUE;
    enabled.tessellationShader = available.tessellationShader;
    state->tessellationShader = enabled.tessellationShader == VK_TRUE;
    if (state->tessellationShader) state->capabilities.push_back(3);
    require(available.samplerAnisotropy && available.textureCompressionBC, "device lacks sampler anisotropy or BC texture compression support required for texture sampling");
    enabled.samplerAnisotropy = VK_TRUE;
    enabled.textureCompressionBC = VK_TRUE;
    state->samplerAnisotropy = true;
    state->textureCompressionBC = true;
    enabled.dualSrcBlend = available.dualSrcBlend;
    enabled.depthClamp = available.depthClamp;
    state->depthClamp = enabled.depthClamp == VK_TRUE;
    enabled.shaderStorageImageWriteWithoutFormat = available.shaderStorageImageWriteWithoutFormat;
    enabled.shaderStorageImageReadWithoutFormat = available.shaderStorageImageReadWithoutFormat;
    state->storageImages = available.shaderStorageImageWriteWithoutFormat == VK_TRUE && available.shaderStorageImageReadWithoutFormat == VK_TRUE;
    if (state->storageImages) {
        state->capabilities.push_back(spv::CapabilityStorageImageReadWithoutFormat);
        state->capabilities.push_back(spv::CapabilityStorageImageWriteWithoutFormat);
    }
    deviceInfo.pEnabledFeatures = &enabled;
    deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if (state->meshShader) {
        meshFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &meshFeatures;
    }
    if (state->depthClipControl) {
        depthClipFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &depthClipFeatures;
    }
    byteFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
    if (state->fragmentShaderBarycentric) {
        barycentricFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &barycentricFeatures;
    }
    bdaFeatures.pNext = &byteFeatures;
    deviceInfo.pNext = &bdaFeatures;
    check(state->InstanceFunction<PFN_vkCreateDevice>("vkCreateDevice")(selected, &deviceInfo, nullptr, &state->device), "vkCreateDevice");
    RetainMetalCommandReferences();
    state->DeviceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(state->device, family, 0, &state->queue);
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = family;
    check(state->DeviceFunction<PFN_vkCreateCommandPool>("vkCreateCommandPool")(state->device, &poolInfo, nullptr, &state->pool), "vkCreateCommandPool");
    state->releaseQueue = std::make_shared<Graphics::ReleaseQueue>(graphicsContext());
    state->bufferPool = std::make_shared<Graphics::BufferPool>(graphicsContext());
    state->descriptorCache = std::make_shared<Graphics::DescriptorCache>();
    state->samplerCache = std::make_shared<Graphics::SamplerCache>();
    state->pipelineCache = std::make_unique<Graphics::PipelineCache>(graphicsContext());
    state->detiler = std::make_unique<Graphics::TextureDetiler>(graphicsContext());
    state->drawQueue = std::make_unique<Graphics::DrawQueue>();
    state->guestBufferCache = std::make_unique<Graphics::GuestBufferCache>(graphicsContext());
    state->gds = std::make_unique<Graphics::Buffer>(graphicsContext(), GdsBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::memset(state->gds->Bytes().data(), 0, GdsBytes);
    state->colorTransfer = std::make_unique<Graphics::GpuColorTransfer>(graphicsContext());
    state->renderCache = std::make_unique<Graphics::RenderCache>(graphicsContext());
    state->graphicsPipelines = std::make_unique<Graphics::GraphicsPipelineCache>(graphicsContext());
    state->textureCache = std::make_unique<Graphics::TextureCache>(graphicsContext());
    if (window != nullptr) {
        require(window->getDrawableSize != nullptr, "missing window drawable size query");
        std::uint32_t drawableWidth = 0;
        std::uint32_t drawableHeight = 0;
        window->getDrawableSize(window->context, &drawableWidth, &drawableHeight);
        require(drawableWidth != 0 && drawableHeight != 0, "window has a zero drawable size at creation");
        VkSurfaceCapabilitiesKHR surface{};
        check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR")(selected, state->surface, &surface), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        state->extent = {drawableWidth, drawableHeight};
        require(surface.currentExtent.width == std::numeric_limits<std::uint32_t>::max() || (surface.currentExtent.width == drawableWidth && surface.currentExtent.height == drawableHeight), "window extent differs from the real drawable size");
        require(drawableWidth >= surface.minImageExtent.width && drawableWidth <= surface.maxImageExtent.width && drawableHeight >= surface.minImageExtent.height && drawableHeight <= surface.maxImageExtent.height, "unsupported output extent");
        require((surface.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0, "surface does not support transfer destination images");
        require((surface.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0, "opaque composition is unavailable");
        require((surface.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0, "identity surface transform is unavailable");
        std::uint32_t formatCount = 0;
        auto getFormats = state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>("vkGetPhysicalDeviceSurfaceFormatsKHR");
        check(getFormats(selected, state->surface, &formatCount, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        check(getFormats(selected, state->surface, &formatCount, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");
        require(std::any_of(formats.begin(), formats.end(), [](const auto& format) { return format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; }), "BGRA8 sRGB-nonlinear surface format is unavailable");
        VkSwapchainCreateInfoKHR swapchain{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swapchain.surface = state->surface;
        swapchain.minImageCount = surface.minImageCount;
        swapchain.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        swapchain.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchain.imageExtent = state->extent;
        swapchain.imageArrayLayers = 1;
        swapchain.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchain.clipped = VK_FALSE;
        check(state->DeviceFunction<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(state->device, &swapchain, nullptr, &state->swapchain), "vkCreateSwapchainKHR");
        std::uint32_t imageCount = 0;
        auto getImages = state->DeviceFunction<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR");
        check(getImages(state->device, state->swapchain, &imageCount, nullptr), "vkGetSwapchainImagesKHR");
        state->images.resize(imageCount);
        check(getImages(state->device, state->swapchain, &imageCount, state->images.data()), "vkGetSwapchainImagesKHR");
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        for (auto* destination : {&state->acquireFence, &state->renderFence}) {
            check(state->DeviceFunction<PFN_vkCreateFence>("vkCreateFence")(state->device, &fence, nullptr, destination), "vkCreateFence");
        }
        state->images.resize(imageCount);
        state->rendered.resize(imageCount, VK_NULL_HANDLE);
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = state->pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        check(state->DeviceFunction<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(state->device, &allocation, &state->clearCommands), "vkAllocateCommandBuffers");
        state->scaler = std::make_unique<PresentationScaler>(graphicsContext(), VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM);
        state->rgbaScaler = std::make_unique<PresentationScaler>(graphicsContext(), VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM);
    }
    GuestAllocations::GuestAllocationsAddReleaseHook_nid_postfix(this, [](void* device) {
        auto& self = *static_cast<VulkanDevice*>(device);
        std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
        self.state->drawQueue->Wait();
        self.state->PublishHostRanges();
    });
}

VulkanDevice::~VulkanDevice() {
    GuestAllocations::GuestAllocationsRemoveReleaseHook_nid_postfix(this);
}

void VulkanDevice::WaitIdle() {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    PerformanceTimer timing("Vulkan.WaitIdle");
    state->drawQueue->Wait();
    state->PublishHostRanges();
    if (state->pipelineCache) state->pipelineCache->SaveIfDue();
    timing.Mark("draw_wait");
    const auto submissions = Graphics::QueueSubmissionCounter().load(std::memory_order_relaxed);
    if (submissions == state->idleSubmissions) return;
    VkResult idle = VK_SUCCESS;
    GpuJournal::Watched("vkDeviceWaitIdle", std::chrono::seconds(15), [&] { idle = state->DeviceFunction<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(state->device); });
    check(idle, "vkDeviceWaitIdle");
    state->idleSubmissions = Graphics::QueueSubmissionCounter().load(std::memory_order_relaxed);
    timing.Mark("device_wait");
}

void VulkanDevice::Defer(std::function<void()> action) {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    state->drawQueue->EnqueueCompletion(std::move(action));
}

bool VulkanDevice::HasPendingWork() {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    return state->drawQueue->HasPending();
}

void VulkanDevice::Collect() {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    state->drawQueue->Collect();
    state->PublishHostRanges();
}

void VulkanDevice::RecordBarrier() {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    if (state->drawQueue->HasPending()) state->drawQueue->RecordMemoryBarrier(graphicsContext());
}

void VulkanDevice::ResolveGpuWrites(std::uint64_t address, std::size_t bytes) {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    if (bytes != 0) state->drawQueue->Resolve(address, bytes);
}

void VulkanDevice::GdsTransfer(std::span<const std::uint32_t> packet) {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    PerformanceTimer timing("Vulkan.GdsTransfer");
    const auto bytes = static_cast<std::size_t>(packet[6] & 0x3ffffffu);
    const bool toGds = Pm4::DmaGdsDestination(packet);
    const bool fromGds = Pm4::DmaGdsSource(packet);
    require(toGds || fromGds, "DMA_DATA does not involve GDS");
    require(!(toGds && fromGds), "GDS to GDS DMA_DATA is not implemented");
    state->drawQueue->WaitGds();
    timing.Mark("draw_wait");
    auto gds = state->gds->Bytes();
    std::uint32_t previous = 0;
    if (toGds && packet[4] + 4 <= gds.size()) std::memcpy(&previous, gds.data() + packet[4], 4);
    if (toGds) {
        const auto offset = static_cast<std::size_t>(packet[4]);
        require(offset + bytes <= gds.size(), "DMA_DATA GDS destination exceeds the GDS size");
        if (Pm4::DmaImmediateSource(packet)) {
            const auto value = packet[2];
            for (std::size_t i = 0; i < bytes; ++i) gds[offset + i] = static_cast<std::byte>(value >> ((i % 4) * 8));
        } else {
            const auto source = static_cast<std::uint64_t>(packet[2]) | (static_cast<std::uint64_t>(packet[3]) << 32u);
            GuestMemory::Read(source, gds.subspan(offset, bytes), 1);
        }
    } else {
        const auto offset = static_cast<std::size_t>(packet[2]);
        require(offset + bytes <= gds.size(), "DMA_DATA GDS source exceeds the GDS size");
        const auto destination = static_cast<std::uint64_t>(packet[4]) | (static_cast<std::uint64_t>(packet[5]) << 32u);
        GuestMemory::Write(destination, gds.subspan(offset, bytes), 1);
    }
    static const bool trace = std::getenv("ANYPS5_TRACE_INDIRECT") != nullptr || std::getenv("ANYPS5_TRACE_WRITEBACK") != nullptr;
    if (trace) {
        std::string nonzero;
        std::size_t count = 0;
        for (std::size_t offset = 0; offset + 4 <= gds.size(); offset += 4) {
            std::uint32_t value;
            std::memcpy(&value, gds.data() + offset, 4);
            if (value == 0) continue;
            if (count++ < 12) { char item[32]; std::snprintf(item, sizeof(item), " %zx:%x", offset, value); nonzero += item; }
        }
        if (count != 0) std::fprintf(stderr, "[gds] %zu nonzero dwords:%s\n", count, nonzero.c_str());
        std::uint32_t first = 0;
        if (toGds) std::memcpy(&first, gds.data() + packet[4], std::min<std::size_t>(4, bytes));
        else std::memcpy(&first, gds.data() + packet[2], std::min<std::size_t>(4, bytes));
        std::fprintf(stderr, "[gds] %s offset 0x%x %zu bytes %s 0x%llx first dword 0x%08x previous 0x%08x\n", toGds ? "write" : "read", toGds ? packet[4] : packet[2], bytes, toGds ? "from" : "to", static_cast<unsigned long long>(toGds ? (static_cast<std::uint64_t>(packet[2]) | (static_cast<std::uint64_t>(packet[3]) << 32u)) : (static_cast<std::uint64_t>(packet[4]) | (static_cast<std::uint64_t>(packet[5]) << 32u))), first, previous);
    }
    timing.Mark("copy");
}

void VulkanDevice::WaitDraws() {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    PerformanceTimer timing("Vulkan.WaitDraws");
    state->drawQueue->Wait();
    state->PublishHostRanges();
}

void VulkanDevice::FlushDraws() {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    state->drawQueue->Flush();
}

std::uint64_t VulkanDevice::SubmitTicket() {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    std::uint64_t ticket = 0;
    {
        std::lock_guard lock(state->ticketMutex);
        ticket = ++state->ticketsIssued;
    }
    auto* shared = state.get();
    state->drawQueue->EnqueueCompletion([shared, ticket] {
        std::lock_guard lock(shared->ticketMutex);
        shared->ticketsDone = std::max(shared->ticketsDone, ticket);
    });
    state->drawQueue->Flush();
    return ticket;
}

void VulkanDevice::WaitTicket(std::uint64_t ticket) {
    PerformanceTimer timing("Vulkan.WaitTicket");
    const auto done = [&] {
        std::lock_guard lock(state->ticketMutex);
        return state->ticketsDone >= ticket;
    };
    while (!done()) {
        {
            std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
            state->drawQueue->Collect();
        }
        if (done()) break;
        PreciseSleepNanos_nid_no_patch(100000);
    }
}

void* VulkanDevice::Window() const {
    return state->window;
}

void VulkanDevice::Resize(std::uint32_t width, std::uint32_t height) {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    require(state->swapchain != VK_NULL_HANDLE, "cannot resize an unavailable swapchain");
    if (width == 0 || height == 0) {
        state->extent = {0, 0};
        return;
    }
    VkSurfaceCapabilitiesKHR surface{};
    check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR")(state->physical, state->surface, &surface), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR resize");
    if (surface.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        width = surface.currentExtent.width;
        height = surface.currentExtent.height;
    }
    if (width == 0 || height == 0) {
        state->extent = {0, 0};
        return;
    }
    if (!state->swapchainState.NeedsRecreation() && state->extent.width == width && state->extent.height == height) return;
    WaitIdle();
    require(width >= surface.minImageExtent.width && width <= surface.maxImageExtent.width && height >= surface.minImageExtent.height && height <= surface.maxImageExtent.height, "unsupported resized output extent");
    require((surface.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0 && (surface.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0 && (surface.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0, "resized surface capabilities are unsupported");
    VkSwapchainCreateInfoKHR create{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create.surface = state->surface;
    create.minImageCount = surface.minImageCount;
    create.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    create.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create.imageExtent = {width, height};
    create.imageArrayLayers = 1;
    create.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    create.oldSwapchain = state->swapchain;
    state->retiredSwapchains.reserve(state->retiredSwapchains.size() + 1);
    VkSwapchainKHR replacement = VK_NULL_HANDLE;
    check(state->DeviceFunction<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(state->device, &create, nullptr, &replacement), "vkCreateSwapchainKHR resize");
    state->retiredSwapchains.push_back({state->swapchain, std::move(state->rendered)});
    state->swapchain = replacement;
    state->extent = create.imageExtent;
    auto getImages = state->DeviceFunction<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR");
    std::uint32_t count = 0;
    check(getImages(state->device, replacement, &count, nullptr), "vkGetSwapchainImagesKHR resize");
    state->images.resize(count);
    check(getImages(state->device, replacement, &count, state->images.data()), "vkGetSwapchainImagesKHR resize");
    state->images.resize(count);
    state->rendered.assign(count, VK_NULL_HANDLE);
    state->swapchainState.Recreated();
}

bool VulkanDevice::Presentable() const {
    return state->extent.width != 0 && state->extent.height != 0;
}

void VulkanDevice::PresentClear(std::uint32_t width, std::uint32_t height, bool opaque) {
    present(width, height, opaque, {});
}

void VulkanDevice::PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) {
    require(width != 0 && height != 0 && width <= 16384 && height <= 16384, "invalid display image extent");
    require(pixels.size() == static_cast<std::uint64_t>(width) * height * 4, "invalid display pixel buffer size");
    present(width, height, true, pixels);
}

void VulkanDevice::PresentDisplayBuffer(const DisplayBuffer& buffer) {
    present(buffer.width, buffer.height, true, {}, &buffer);
}

void VulkanDevice::present(std::uint32_t width, std::uint32_t height, bool opaque, std::span<const std::byte> pixels, const DisplayBuffer* display) {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    PerformanceTimer timing("Vulkan.Present");
    if (AsyncFlips()) {
        state->drawQueue->Flush();
        state->drawQueue->Collect();
    } else {
        state->drawQueue->Wait();
    }
    state->PublishHostRanges();
    timing.Mark("draw_wait");
    if (state->renderPending) {
        check(state->DeviceFunction<PFN_vkWaitForFences>("vkWaitForFences")(state->device, 1, &state->renderFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()), "vkWaitForFences previous presentation");
        state->renderPending = false;
    }
    state->presentedTarget.reset();
    timing.Mark("previous_present_wait");
    require(state->swapchain != VK_NULL_HANDLE, "device has no swapchain");
    require(state->extent.width != 0 && state->extent.height != 0, "output window is minimized");
    std::shared_ptr<Graphics::ResidentColor> resident;
    if (display != nullptr) {
        const auto bytes = DisplayBufferSize(*display);
        resident = state->renderCache->Find(display->address);
        if (resident) {
            const auto& color = resident->Description();
            if (color.extent.width != width || color.extent.height != height || color.bytes != bytes || color.tileMode != Graphics::ColorTileMode::RenderTarget) resident.reset();
        }
        if (!resident) {
            ResolveMemory(display->address, bytes, false);
            state->colorTransfer->Upload(display->address, width, height, Graphics::ColorTileMode::RenderTarget);
        }
        if (static const char* targetsTrigger = std::getenv("ANYPS5_DUMP_TARGETS_TRIGGER"); targetsTrigger != nullptr) {
            std::error_code missing;
            if (std::filesystem::remove(targetsTrigger, missing)) {
                state->renderCache->DumpTargets(std::string(targetsTrigger) + "_");
                state->renderCache->DumpDepthTargets(std::string(targetsTrigger) + "_");
                GpuJournal::Dump("targets dumped on request: last GPU work, oldest first");
                APS5_LOG_CHARS_OUT("dumped the resident color targets on request");
            }
        }
        if (static const char* trigger = std::getenv("ANYPS5_DUMP_DISPLAY_TRIGGER"); trigger != nullptr) {
            std::error_code missing;
            if (std::filesystem::remove(trigger, missing)) {
                state->renderCache->DumpTargets(std::string(trigger) + "_", display->address);
                APS5_LOG_OUT("dumped the display 0x%llx on request", static_cast<unsigned long long>(display->address));
            }
        }
        if (static const char* displayAt = std::getenv("ANYPS5_DUMP_DISPLAY_AT"); displayAt != nullptr) {
            static const auto displayStart = std::chrono::steady_clock::now();
            static std::size_t nextTime = 0;
            static const auto times = [] {
                std::vector<double> result;
                for (const char* cursor = displayAt; *cursor != '\0' && *cursor != ':';) {
                    char* end = nullptr;
                    result.push_back(std::strtod(cursor, &end));
                    if (end == cursor) break;
                    cursor = *end == ',' ? end + 1 : end;
                }
                return result;
            }();
            const char* colon = std::strchr(displayAt, ':');
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - displayStart).count();
            if (colon != nullptr && nextTime < times.size() && elapsed >= times[nextTime]) {
                const auto prefix = std::string(colon + 1) + std::to_string(static_cast<int>(times[nextTime++])) + "s_";
                state->renderCache->DumpTargets(prefix, display->address);
                APS5_LOG_OUT("dumped the display 0x%llx to %s", static_cast<unsigned long long>(display->address), prefix.c_str());
            }
        }
        if (static const char* dumpAt = std::getenv("ANYPS5_DUMP_TARGETS_AT"); dumpAt != nullptr) {
            static const auto dumpStart = std::chrono::steady_clock::now();
            static bool dumped = false;
            const char* colon = std::strchr(dumpAt, ':');
            if (!dumped && colon != nullptr && std::chrono::duration<double>(std::chrono::steady_clock::now() - dumpStart).count() >= std::atof(dumpAt)) {
                dumped = true;
                state->renderCache->DumpTargets(colon + 1);
                state->textureCache->DumpTextures(colon + 1);
                GpuJournal::Dump("targets dumped: last GPU work, oldest first");
                APS5_LOG_OUT("dumped the resident color targets to %s", colon + 1);
            }
        }
        static const char* dumpPrefix = std::getenv("ANYPS5_DUMP_FRAME");
        static unsigned presented = 0;
        if (dumpPrefix != nullptr && presented++ % 30 == 0) {
            APS5_LOG_OUT("present %u: display 0x%llx %ux%u format 0x%llx resident %s; color targets:%s", presented, static_cast<unsigned long long>(display->address), width, height, static_cast<unsigned long long>(display->pixelFormat), resident ? "yes" : "no", state->renderCache->DescribeColorTargets().c_str());
            if (presented % 60 == 31) {
                state->renderCache->DumpTargets(std::string(dumpPrefix) + std::to_string(presented) + "_");
                state->textureCache->DumpTextures(std::string(dumpPrefix) + std::to_string(presented) + "_");
                state->renderCache->DumpDepthTargets(std::string(dumpPrefix) + std::to_string(presented) + "_");
                GpuJournal::Dump("frame dump: last GPU work");
            }
            if (!resident) {
                try {
                    const auto pixels = ReadDisplayBuffer(*display);
                    std::size_t nonzero = 0;
                    for (const auto byte : pixels) nonzero += byte != std::byte{0};
                    const std::string path = std::string(dumpPrefix) + std::to_string(presented) + ".bmp";
                    if (auto* file = std::fopen(path.c_str(), "wb")) {
                        const std::uint32_t rowBytes = width * 4, imageBytes = rowBytes * height;
                        const std::uint32_t fileSize = 54 + imageBytes;
                        unsigned char header[54] = {'B', 'M'};
                        auto put32 = [&](int at, std::uint32_t v) { header[at] = v & 0xff; header[at + 1] = (v >> 8) & 0xff; header[at + 2] = (v >> 16) & 0xff; header[at + 3] = (v >> 24) & 0xff; };
                        put32(2, fileSize); put32(10, 54); put32(14, 40); put32(18, width); put32(22, static_cast<std::uint32_t>(-static_cast<std::int32_t>(height))); header[26] = 1; header[28] = 32; put32(34, imageBytes);
                        std::fwrite(header, 1, 54, file);
                        std::vector<unsigned char> row(rowBytes);
                        for (std::uint32_t y = 0; y < height; ++y) {
                            const auto* line = reinterpret_cast<const unsigned char*>(pixels.data()) + static_cast<std::size_t>(y) * rowBytes;
                            for (std::uint32_t x = 0; x < width; ++x) { row[x * 4] = line[x * 4 + 2]; row[x * 4 + 1] = line[x * 4 + 1]; row[x * 4 + 2] = line[x * 4]; row[x * 4 + 3] = 255; }
                            std::fwrite(row.data(), 1, rowBytes, file);
                        }
                        std::fclose(file);
                    }
                    APS5_LOG_OUT("present %u: guest display buffer has %zu nonzero bytes of %zu", presented, nonzero, pixels.size());
                } catch (const std::exception& error) {
                    APS5_LOG_OUT("present %u: cannot dump the display buffer: %s", presented, error.what());
                }
            }
        }
    }
    static const bool debugPresentClear = std::getenv("ANYPS5_DEBUG_PRESENT_CLEAR") != nullptr;
    if (debugPresentClear && resident) {
        Graphics::CommandBatch batch(graphicsContext());
        resident->DebugClear(batch.Handle(), 1.0f, 0.0f, 1.0f);
        batch.SubmitAndWait();
    }
    auto* scaler = resident && DisplayFormatRgba(display->pixelFormat) ? state->rgbaScaler.get() : state->scaler.get();
    if (!pixels.empty()) state->Upload(pixels);
    timing.Mark("pixel_upload");
    auto wait = state->DeviceFunction<PFN_vkWaitForFences>("vkWaitForFences");
    auto reset = state->DeviceFunction<PFN_vkResetFences>("vkResetFences");
    const std::array<VkFence, 2> fences{state->acquireFence, state->renderFence};
    check(reset(state->device, static_cast<std::uint32_t>(fences.size()), fences.data()), "vkResetFences");
    std::uint32_t index = 0;
    timing.Mark("fence_reset");
    if (!state->swapchainState.ProcessResult(state->DeviceFunction<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR")(state->device, state->swapchain, 5'000'000'000ULL, VK_NULL_HANDLE, state->acquireFence, &index), "vkAcquireNextImageKHR")) return;
    timing.Mark("acquire_image");
    check(wait(state->device, 1, &state->acquireFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()), "vkWaitForFences acquire");
    timing.Mark("acquire_fence_wait");
    require(index < state->images.size() && index < state->rendered.size(), "acquired image index is out of range");
    auto& rendered = state->rendered[index];
    if (rendered != VK_NULL_HANDLE) {
        state->DestroyRetiredSwapchains();
    } else {
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(state->DeviceFunction<PFN_vkCreateSemaphore>("vkCreateSemaphore")(state->device, &semaphore, nullptr, &rendered), "vkCreateSemaphore presentation");
    }
    auto commands = state->clearCommands;
    timing.Mark("retired_swapchains");
    check(state->DeviceFunction<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(commands, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(state->DeviceFunction<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin), "vkBeginCommandBuffer");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = state->images[index];
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto pipelineBarrier = state->DeviceFunction<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    if (pixels.empty() && display == nullptr) {
        VkClearColorValue clear{};
        clear.float32[3] = opaque ? 1.0f : 0.0f;
        state->DeviceFunction<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &barrier.subresourceRange);
    } else {
        require(scaler != nullptr, "presentation scaler is unavailable");
        scaler->EnsureSourceImage(width, height);
        if (resident) {
            resident->Transition(commands, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            scaler->RecordImage(commands, resident->Target().Image());
        } else {
            if (display != nullptr) state->colorTransfer->Detile(commands, DisplayFormatRgba(display->pixelFormat));
            scaler->RecordUpload(commands, display != nullptr ? state->colorTransfer->LinearBuffer() : state->uploadBuffer);
        }
        VkClearColorValue letterbox{};
        letterbox.float32[3] = 1.0f;
        state->DeviceFunction<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &letterbox, 1, &barrier.subresourceRange);
        VkImageMemoryBarrier letterboxBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        letterboxBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        letterboxBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        letterboxBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        letterboxBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        letterboxBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        letterboxBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        letterboxBarrier.image = barrier.image;
        letterboxBarrier.subresourceRange = barrier.subresourceRange;
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &letterboxBarrier);
        scaler->RecordBlit(commands, barrier.image, state->extent.width, state->extent.height);
    }
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    check(state->DeviceFunction<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &rendered;
    timing.Mark("command_record_scale");
    check(state->DeviceFunction<PFN_vkQueueSubmit>("vkQueueSubmit")(state->queue, 1, &submit, state->renderFence), "vkQueueSubmit clear");
    Graphics::QueueSubmissionCounter().fetch_add(1, std::memory_order_relaxed);
    state->releaseQueue->Collect();
    timing.Mark("queue_submit");
    if (AsyncFlips()) {
        state->renderPending = true;
        state->presentedTarget = resident;
    } else {
        check(wait(state->device, 1, &state->renderFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()), "vkWaitForFences clear");
        timing.Mark("render_fence_wait");
    }
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &rendered;
    present.swapchainCount = 1;
    present.pSwapchains = &state->swapchain;
    present.pImageIndices = &index;
    state->swapchainState.ProcessResult(state->DeviceFunction<PFN_vkQueuePresentKHR>("vkQueuePresentKHR")(state->queue, &present), "vkQueuePresentKHR");
    timing.Mark("queue_present");
}

ShaderRecompiler::SpirvTarget VulkanDevice::Target() const {
    const auto& limits = state->properties.limits;
    ShaderRecompiler::SpirvTarget target{VK_API_VERSION_1_1, state->meshShader ? 0x00010400u : 0x00010300u, state->subgroup.subgroupSize, ShaderRecompiler::BdaAbi::Version, state->capabilities, state->spirvExtensions, false, {limits.maxComputeWorkGroupSize[0], limits.maxComputeWorkGroupSize[1], limits.maxComputeWorkGroupSize[2]}, limits.maxComputeWorkGroupInvocations, limits.maxComputeSharedMemorySize, {}, {}};
    if (state->meshShader) {
        const auto& mesh = state->meshLimits;
        target.mesh = ShaderRecompiler::MeshTargetLimits{{mesh.maxMeshWorkGroupSize[0], mesh.maxMeshWorkGroupSize[1], mesh.maxMeshWorkGroupSize[2]}, mesh.maxMeshWorkGroupInvocations, std::min(mesh.maxMeshSharedMemorySize, mesh.maxMeshPayloadAndSharedMemorySize), mesh.maxMeshOutputVertices, mesh.maxMeshOutputPrimitives, mesh.maxMeshOutputComponents, std::min(mesh.maxMeshOutputMemorySize, mesh.maxMeshPayloadAndOutputMemorySize), mesh.meshOutputPerVertexGranularity, mesh.meshOutputPerPrimitiveGranularity};
    }
    {
        const auto stages = state->subgroup.supportedStages;
        std::uint32_t mask = 0;
        if (stages & VK_SHADER_STAGE_COMPUTE_BIT) mask |= (1u << 0u) | (1u << 6u) | (1u << 7u);
        if (stages & VK_SHADER_STAGE_VERTEX_BIT) mask |= 1u << 1u;
        if (stages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) mask |= 1u << 2u;
        if (stages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) mask |= 1u << 3u;
        if (stages & VK_SHADER_STAGE_GEOMETRY_BIT) mask |= 1u << 4u;
        if (stages & VK_SHADER_STAGE_FRAGMENT_BIT) mask |= 1u << 5u;
        if (state->meshShader && !(stages & VK_SHADER_STAGE_MESH_BIT_EXT)) mask &= ~(1u << 7u);
        target.subgroupStageMask = mask;
    }
    if (state->tessellationShader) target.tessellation = ShaderRecompiler::TessellationTargetLimits{limits.maxTessellationPatchSize, limits.maxTessellationControlPerVertexInputComponents, limits.maxTessellationControlPerVertexOutputComponents, limits.maxTessellationControlPerPatchOutputComponents, limits.maxTessellationControlTotalOutputComponents, limits.maxTessellationEvaluationInputComponents, limits.maxTessellationEvaluationOutputComponents};
    return target;
}

Graphics::Context VulkanDevice::graphicsContext() const {
    auto context = Graphics::Context{
        state->device,
        state->physical,
        state->queue,
        state->pool,
        state->deviceProc,
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties"),
        state->InstanceFunction<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties"),
        state->memoryProperties,
        state->properties.limits,
        state->tessellationShader,
        state->meshShader,
        state->meshLimits,
        state->depthClipControl,
        state->depthRangeUnrestricted,
        true,
        state->subgroup,
        state->fragmentShaderBarycentric,
        state->samplerAnisotropy,
        state->textureCompressionBC,
        state->storageImages,
        state->detiler.get(),
        state->colorTransfer.get(),
        state->bufferPool,
        state->textureCache.get(),
        state->pipelineCache ? state->pipelineCache->Handle() : VK_NULL_HANDLE,
        state->renderCache.get(),
        state->drawQueue.get(),
        state->graphicsPipelines.get(),
        state->descriptorCache,
        state->samplerCache
    };
    context.pipelineCacheOwner = state->pipelineCache.get();
    static const bool noMirrors = std::getenv("ANYPS5_DEBUG_NO_MIRRORS") != nullptr;
    context.guestBufferCache = noMirrors ? nullptr : state->guestBufferCache.get();
    context.gds = state->gds.get();
    context.depthClamp = state->depthClamp;
    context.hostPointerImport = state->hostPointerImport;
    context.hostPointerAlignment = state->hostPointerAlignment;
    context.releaseQueue = state->releaseQueue;
    return context;
}

void VulkanDevice::ResolveMemory(std::uint64_t address, std::size_t bytes, bool writable) {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    state->drawQueue->Resolve(address, bytes);
    state->renderCache->Resolve(address, bytes, writable);
    state->PublishHostRanges();
}

bool VulkanDevice::NeedsResolve(std::uint64_t address, std::size_t bytes) const {
    std::shared_ptr<const std::vector<std::pair<std::uint64_t, std::uint64_t>>> ranges;
    {
        std::lock_guard lock(state->hostRangesMutex);
        ranges = state->hostRanges;
    }
    const auto end = address + bytes;
    return std::any_of(ranges->begin(), ranges->end(), [&](const auto& range) { return range.first < end && address < range.second; });
}

void VulkanDevice::Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots) {
    EnqueueDraw(graphics, draw, shaders, snapshots);
    WaitIdle();
}

void VulkanDevice::ValidateDraw(const Graphics::State& graphics, std::span<const Graphics::CompiledShader> shaders) const {
    const auto context = graphicsContext();
    Graphics::ValidateShaders(shaders, graphics, context.subgroup, context.fragmentShaderBarycentric);
}

void VulkanDevice::EnqueueDraw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots) {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    const GuestMemory::MemoryAccessScope memoryScope(this, [](void* context, std::uint64_t address, std::size_t bytes, bool writable) {
        static_cast<VulkanDevice*>(context)->ResolveMemory(address, bytes, writable);
    });
    const auto context = graphicsContext();
    static const char* slowGpu = std::getenv("ANYPS5_DEBUG_SLOW_GPU");
    if (slowGpu != nullptr) {
        state->drawQueue->Flush();
        state->drawQueue->Wait();
    }
    Graphics::Draw(context, graphics, draw, shaders, snapshots);
    if (slowGpu != nullptr) {
        state->drawQueue->Flush();
        const auto start = std::chrono::steady_clock::now();
        state->drawQueue->Wait();
        const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (milliseconds >= std::atof(slowGpu)) APS5_LOG_OUT("[slow-gpu] draw program 0x%llx %u indices x%u into 0x%llx %ux%u took %.1f ms", static_cast<unsigned long long>(GpuJournal::CurrentProgram), draw.indexCount, draw.instanceCount, static_cast<unsigned long long>(graphics.hasColorTarget ? graphics.color.address : 0), graphics.renderExtent.width, graphics.renderExtent.height, milliseconds);
    }
    state->PublishHostRanges();
}

void VulkanDevice::Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots) {
    std::lock_guard memoryLock(GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix());
    PerformanceTimer timing("Vulkan.Dispatch");
    const GuestMemory::MemoryAccessScope memoryScope(this, [](void* context, std::uint64_t address, std::size_t bytes, bool writable) {
        static_cast<VulkanDevice*>(context)->ResolveMemory(address, bytes, writable);
    });
    if (shader.spirv.size() < 5 || shader.spirv[0] != 0x07230203u) {
        throw std::runtime_error("Vulkan dispatch: invalid SPIR-V");
    }
    const std::array<Graphics::CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
    const auto pushStages = Graphics::PushConstantStages(shaders);
    if (pushStages != 0 && state->properties.limits.maxPushConstantsSize < Graphics::PipelinePushConstantBytes) {
        throw std::runtime_error("Vulkan dispatch: compute push constant range exceeds device limit");
    }
    const auto pushBytes = Graphics::AssemblePushConstants(shaders);
    const auto context = graphicsContext();
    const auto* limit = state->properties.limits.maxComputeWorkGroupCount;
    if (x > limit[0] || y > limit[1] || z > limit[2]) {
        throw std::runtime_error("Vulkan dispatch: workgroup count exceeds device limits");
    }
    static const char* slowGpu = std::getenv("ANYPS5_DEBUG_SLOW_GPU");
    if (slowGpu != nullptr) {
        state->drawQueue->Flush();
        state->drawQueue->Wait();
    }
    try {
        auto resources = std::make_shared<Graphics::ShaderResources>(context, shaders[0], snapshots);
        timing.Mark("shader_resources");
        static const bool journalAllTextures = std::getenv("ANYPS5_JOURNAL_TEXTURES") != nullptr;
        if (journalAllTextures) GpuJournal::Record("  dispatch textures:" + resources->DescribeTextures());
        static const char* debugGdsInputsValue = std::getenv("ANYPS5_DEBUG_GDS_INPUTS");
        static const auto debugStart = std::chrono::steady_clock::now();
        const bool debugGdsInputs = debugGdsInputsValue != nullptr && std::chrono::duration<double>(std::chrono::steady_clock::now() - debugStart).count() >= std::atof(debugGdsInputsValue);
        if (debugGdsInputs) {
            bool gds = false;
            for (const auto& binding : shader.bindings) gds = gds || binding.role == ShaderRecompiler::DescriptorRole::Gds;
            bool postChain = false;
            for (const auto& texture : resources->Textures()) postChain = postChain || (texture->Extent().width >= 240 && texture->GuestFormat() == VK_FORMAT_B10G11R11_UFLOAT_PACK32);
            if (gds || postChain || (x == 8 && y == 8 && z == 8)) {
                state->drawQueue->Flush();
                state->drawQueue->Wait();
                std::string dumped;
                if (const char* dumpDirectory = std::getenv("ANYPS5_DUMP_GDS_SHADERS")) {
                    std::uint64_t hash = 1469598103934665603ull;
                    for (const auto word : shader.spirv) hash = (hash ^ word) * 1099511628211ull;
                    char name[64];
                    std::snprintf(name, sizeof(name), "/cs_%016llx.spv", static_cast<unsigned long long>(hash));
                    dumped = std::string(dumpDirectory) + name;
                    if (FILE* file = std::fopen(dumped.c_str(), "wb")) { std::fwrite(shader.spirv.data(), sizeof(std::uint32_t), shader.spirv.size(), file); std::fclose(file); }
                }
                APS5_LOG_OUT("[gds-input] dispatch %ux%ux%u%s %s", x, y, z, gds ? " (binds GDS)" : "", dumped.c_str());
                for (std::size_t i = 0; i < resources->Textures().size(); ++i) {
                    const auto& texture = resources->Textures()[i];
                    const auto [bindingIndex, element] = resources->TextureBindings().at(i);
                    APS5_LOG_OUT("[gds-input]   b%u[%u] 0x%llx %ux%u vk%u%s%s:%s", bindingIndex, element, static_cast<unsigned long long>(texture->GuestAddress()), texture->Extent().width, texture->Extent().height, static_cast<unsigned>(texture->GuestFormat()), texture->IsDirectView() ? " direct" : texture->SharesImage() ? " view" : "", texture->Stored() ? " store" : "", state->textureCache->DescribeContents(*texture).c_str());
                }
            }
        }
        std::uint64_t hash = 1469598103934665603ull;
        for (const auto word : shader.spirv) hash = (hash ^ word) * 1099511628211ull;
        std::string key(reinterpret_cast<const char*>(&hash), sizeof(hash));
        const auto spirvWords = static_cast<std::uint64_t>(shader.spirv.size());
        key.append(reinterpret_cast<const char*>(&spirvWords), sizeof(spirvWords));
        key.push_back(pushStages != 0 ? '\1' : '\0');
        const auto& layoutKey = resources->LayoutKey();
        key.append(reinterpret_cast<const char*>(layoutKey.data()), layoutKey.size() * sizeof(std::uint32_t));
        auto found = state->computePipelines.find(key);
        if (found == state->computePipelines.end()) {
            if (state->computePipelines.size() >= 1024) {
                state->drawQueue->Wait();
                check(state->DeviceFunction<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(state->device), "vkDeviceWaitIdle before compute pipeline eviction");
                state->destroyComputePipelines();
            }
            State::ComputePipeline entry;
            const auto destroyModule = state->DeviceFunction<PFN_vkDestroyShaderModule>("vkDestroyShaderModule");
            const auto destroyLayout = state->DeviceFunction<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout");
            try {
                VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                moduleInfo.codeSize = shader.spirv.size() * sizeof(std::uint32_t);
                moduleInfo.pCode = shader.spirv.data();
                check(state->DeviceFunction<PFN_vkCreateShaderModule>("vkCreateShaderModule")(state->device, &moduleInfo, nullptr, &entry.module), "vkCreateShaderModule");
                const auto setLayout = resources->Layout();
                const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, Graphics::PipelinePushConstantBytes};
                VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
                layoutInfo.setLayoutCount = 1;
                layoutInfo.pSetLayouts = &setLayout;
                layoutInfo.pushConstantRangeCount = pushStages != 0 ? 1 : 0;
                layoutInfo.pPushConstantRanges = pushStages != 0 ? &push : nullptr;
                check(state->DeviceFunction<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(state->device, &layoutInfo, nullptr, &entry.layout), "vkCreatePipelineLayout");
                VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
                pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                pipelineInfo.stage.module = entry.module;
                pipelineInfo.stage.pName = "main";
                pipelineInfo.layout = entry.layout;
                const auto creationStart = std::chrono::steady_clock::now();
                check(state->DeviceFunction<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(state->device, context.pipelineCache, 1, &pipelineInfo, nullptr, &entry.pipeline), "vkCreateComputePipelines");
                timing.Mark("pipeline_create");
                Graphics::ReportSlowPipeline("compute", creationStart, shaders);
                if (state->pipelineCache) state->pipelineCache->NoteCreated();
                if (state->pipelineCache && std::chrono::steady_clock::now() - creationStart > std::chrono::milliseconds(100)) state->pipelineCache->Save();
            } catch (...) {
                if (entry.layout) destroyLayout(state->device, entry.layout, nullptr);
                if (entry.module) destroyModule(state->device, entry.module, nullptr);
                throw;
            }
            found = state->computePipelines.emplace(key, entry).first;
        } else {
            timing.Mark("pipeline_hit");
        }
        const auto layout = found->second.layout;
        const auto pipeline = found->second.pipeline;
        if (static const bool traceWrites = std::getenv("ANYPS5_TRACE_WAITS") != nullptr; traceWrites && resources->Writes()) {
            static int reported = 0;
            if (reported++ < 400) {
                std::string ranges;
                for (const auto& [begin, end] : resources->WriteRanges()) {
                    char item[64];
                    std::snprintf(item, sizeof(item), " 0x%llx+0x%llx", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin));
                    ranges += item;
                }
                std::string textures;
                for (const auto& texture : resources->Textures()) {
                    char item[80];
                    std::snprintf(item, sizeof(item), " 0x%llx(%ux%u vk%u%s)", static_cast<unsigned long long>(texture->GuestAddress()), texture->Extent().width, texture->Extent().height, static_cast<unsigned>(texture->GuestFormat()), texture->IsDirectView() ? " direct" : "");
                    textures += item;
                }
                std::uint64_t hash = 1469598103934665603ull;
                for (const auto word : shader.spirv) hash = (hash ^ word) * 1099511628211ull;
                std::fprintf(stderr, "[dispatch-writes] %ux%ux%u shader %016llx, writes%s textures%s\n", x, y, z, static_cast<unsigned long long>(hash), ranges.c_str(), textures.c_str());
            }
        }
        const auto commands = state->drawQueue->Begin(context);
        VkMemoryBarrier upload{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        upload.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        upload.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        state->DeviceFunction<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &upload, 0, nullptr, 0, nullptr);
        state->DeviceFunction<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        resources->Bind(commands, VK_PIPELINE_BIND_POINT_COMPUTE, layout);
        if (pushStages != 0) {
            state->DeviceFunction<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, Graphics::PipelinePushConstantBytes, pushBytes.data());
        }
        state->DeviceFunction<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, x, y, z);
        if (GpuJournal::CommandLabel != nullptr) {
            char text[80];
            std::snprintf(text, sizeof(text), "dispatch 0x%llx %ux%ux%u", static_cast<unsigned long long>(GpuJournal::CurrentProgram), x, y, z);
            GpuJournal::Label(commands, text);
        }
        VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        download.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        download.dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;
        state->DeviceFunction<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
        timing.Mark("command_record");
        bool debugPost = false;
        for (const auto& texture : resources->Textures()) debugPost = debugPost || (texture->Extent().width >= 240 && texture->GuestFormat() == VK_FORMAT_B10G11R11_UFLOAT_PACK32);
        const auto debugResources = debugGdsInputs && (debugPost || (x == 8 && y == 8 && z == 8)) ? resources : nullptr;
        state->drawQueue->Enqueue(std::move(resources), std::make_shared<int>(0));
        if (slowGpu != nullptr) {
            state->drawQueue->Flush();
            const auto start = std::chrono::steady_clock::now();
            state->drawQueue->Wait();
            const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if (milliseconds >= std::atof(slowGpu)) APS5_LOG_OUT("[slow-gpu] dispatch program 0x%llx %ux%ux%u took %.1f ms (SPIR-V %zu words)", static_cast<unsigned long long>(GpuJournal::CurrentProgram), x, y, z, milliseconds, shader.spirv.size());
        }
        if (debugResources) {
            state->drawQueue->Flush();
            state->drawQueue->Wait();
            for (const auto& texture : debugResources->Textures()) {
                if (texture->Extent().width * texture->Extent().height < 32 * 32) continue;
                APS5_LOG_OUT("[gds-output]  0x%llx %ux%u vk%u%s:%s", static_cast<unsigned long long>(texture->GuestAddress()), texture->Extent().width, texture->Extent().height, static_cast<unsigned>(texture->GuestFormat()), texture->IsDirectView() ? " direct" : "", state->textureCache->DescribeContents(*texture).c_str());
            }
        }
        timing.Mark("enqueue");
        state->PublishHostRanges();
    } catch (...) {
        throw;
    }
}

}

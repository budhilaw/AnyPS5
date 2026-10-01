#include "prx/libSceAgcDriver/Replay/include/DispatchReplay.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CaptureFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "BdaAbi.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#if defined(_WIN32)
#include <malloc.h>
#endif

namespace AgcDriver::Replay {

using Graphics::CaptureBufferSource;
using Graphics::CaptureManifest;
using ShaderRecompiler::DescriptorRole;

namespace {

constexpr std::uint32_t PushConstantBytes = 128;
constexpr std::uint64_t FenceTimeout = 60'000'000'000ull;
constexpr VkMemoryPropertyFlags HostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
constexpr std::uint64_t ViewAlignment = 256;

void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(operation) + " failed with Vulkan result " + std::to_string(static_cast<int>(result)));
}

std::string hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

std::uint64_t alignUp(std::uint64_t value, std::uint64_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

void* allocateHost(std::size_t bytes, std::size_t alignment) {
#if defined(_WIN32)
    return _aligned_malloc(bytes, alignment);
#else
    void* pointer = nullptr;
    return posix_memalign(&pointer, alignment, bytes) == 0 ? pointer : nullptr;
#endif
}

void freeHost(void* pointer) {
#if defined(_WIN32)
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}

bool hasExtension(std::span<const VkExtensionProperties> extensions, const char* name) {
    return std::any_of(extensions.begin(), extensions.end(), [&](const VkExtensionProperties& extension) { return std::strcmp(extension.extensionName, name) == 0; });
}

std::vector<std::uint32_t> readWords(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("cannot open " + path.string());
    const auto size = static_cast<std::size_t>(file.tellg());
    if (size == 0 || size % sizeof(std::uint32_t) != 0) throw std::runtime_error(path.string() + " is not a SPIR-V module");
    std::vector<std::uint32_t> words(size / sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(size));
    if (!file || words.size() < 5 || words[0] != 0x07230203u) throw std::runtime_error(path.string() + " is not a SPIR-V module");
    return words;
}

std::string readRequestPayload(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open " + path.string());
    std::string line;
    bool header = false;
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!header) {
            if (line != Graphics::CaptureRequestHeader) throw std::runtime_error(path.string() + " is not a shader request file");
            header = true;
            continue;
        }
        if (!line.empty()) return line;
    }
    throw std::runtime_error(path.string() + " holds no recompile request");
}

std::span<const std::uint32_t> elementWords(std::span<const std::uint32_t> words, std::uint32_t element, std::uint32_t stride) {
    const auto offset = static_cast<std::size_t>(element) * stride;
    if (offset + stride > words.size()) return {};
    return words.subspan(offset, stride);
}

bool sameWords(std::span<const std::uint32_t> left, std::span<const std::uint32_t> right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

}

struct ReplayDevice::State {
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    PFN_vkGetDeviceProcAddr deviceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t family = 0;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memory{};
    std::uint32_t subgroupSize = 0;
    std::uint32_t timestampBits = 0;
    bool hostImport = false;
    VkDeviceSize hostAlignment = 0;
    bool minLod = false;
    bool anisotropy = false;
    mutable std::unordered_map<std::string, PFN_vkVoidFunction> functions;

    template<typename TFunction>
    TFunction Instance(const char* name) const {
        const auto function = reinterpret_cast<TFunction>(instanceProc(instance, name));
        if (function == nullptr) throw std::runtime_error(std::string("Vulkan instance function missing: ") + name);
        return function;
    }

    template<typename TFunction>
    TFunction Device(const char* name) const {
        auto& function = functions[name];
        if (function == nullptr) function = deviceProc(device, name);
        if (function == nullptr) throw std::runtime_error(std::string("Vulkan device function missing: ") + name);
        return reinterpret_cast<TFunction>(function);
    }

    std::uint32_t MemoryType(std::uint32_t bits, VkMemoryPropertyFlags flags) const {
        for (std::uint32_t index = 0; index < memory.memoryTypeCount; ++index) {
            if ((bits & (1u << index)) != 0 && (memory.memoryTypes[index].propertyFlags & flags) == flags) return index;
        }
        throw std::runtime_error("no Vulkan memory type has the properties " + hex(flags));
    }

    ~State() {
        if (device != VK_NULL_HANDLE && deviceProc != nullptr) {
            if (const auto wait = reinterpret_cast<PFN_vkDeviceWaitIdle>(deviceProc(device, "vkDeviceWaitIdle"))) wait(device);
            if (const auto destroyPool = reinterpret_cast<PFN_vkDestroyCommandPool>(deviceProc(device, "vkDestroyCommandPool")); destroyPool != nullptr && pool != VK_NULL_HANDLE) destroyPool(device, pool, nullptr);
            if (const auto destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(deviceProc(device, "vkDestroyDevice"))) destroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE && instanceProc != nullptr) {
            if (const auto destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(instanceProc(instance, "vkDestroyInstance"))) destroyInstance(instance, nullptr);
        }
    }
};

ReplayDevice::ReplayDevice(PFN_vkGetInstanceProcAddr instanceProc) : state(std::make_unique<State>()) {
    auto& vk = *state;
    if (instanceProc == nullptr) throw std::runtime_error("Vulkan loader: vkGetInstanceProcAddr is unavailable");
    vk.instanceProc = instanceProc;
    const auto enumerateInstance = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(instanceProc(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
    const auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(instanceProc(VK_NULL_HANDLE, "vkCreateInstance"));
    if (enumerateInstance == nullptr || createInstance == nullptr) throw std::runtime_error("Vulkan loader: instance creation functions are unavailable");
    std::uint32_t count = 0;
    check(enumerateInstance(nullptr, &count, nullptr), "vkEnumerateInstanceExtensionProperties");
    std::vector<VkExtensionProperties> instanceExtensions(count);
    check(enumerateInstance(nullptr, &count, instanceExtensions.data()), "vkEnumerateInstanceExtensionProperties");
    instanceExtensions.resize(count);
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "AnyPS5 dispatch replay";
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &application;
    std::vector<const char*> enabledInstance;
    if (hasExtension(instanceExtensions, "VK_KHR_portability_enumeration")) {
        enabledInstance.push_back("VK_KHR_portability_enumeration");
        instanceInfo.flags |= 0x00000001;
    }
    instanceInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabledInstance.size());
    instanceInfo.ppEnabledExtensionNames = enabledInstance.data();
    check(createInstance(&instanceInfo, nullptr, &vk.instance), "vkCreateInstance");
    vk.deviceProc = vk.Instance<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
    const auto enumerate = vk.Instance<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
    check(enumerate(vk.instance, &count, nullptr), "vkEnumeratePhysicalDevices");
    std::vector<VkPhysicalDevice> devices(count);
    check(enumerate(vk.instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
    devices.resize(count);
    const auto getProperties = vk.Instance<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
    const auto getFamilies = vk.Instance<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
    for (const auto physical : devices) {
        VkPhysicalDeviceProperties properties{};
        getProperties(physical, &properties);
        if (properties.apiVersion < VK_API_VERSION_1_1) continue;
        std::uint32_t families = 0;
        getFamilies(physical, &families, nullptr);
        std::vector<VkQueueFamilyProperties> queues(families);
        getFamilies(physical, &families, queues.data());
        for (std::uint32_t index = 0; index < families; ++index) {
            if (queues[index].queueCount != 0 && (queues[index].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                vk.physical = physical;
                vk.family = index;
                vk.timestampBits = queues[index].timestampValidBits;
                break;
            }
        }
        if (vk.physical != VK_NULL_HANDLE) break;
    }
    if (vk.physical == VK_NULL_HANDLE) throw std::runtime_error("Vulkan: no Vulkan 1.1 device with a graphics and compute queue");
    VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &subgroup};
    vk.Instance<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(vk.physical, &properties);
    vk.properties = properties.properties;
    vk.subgroupSize = subgroup.subgroupSize;
    vk.Instance<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(vk.physical, &vk.memory);
    const auto enumerateDevice = vk.Instance<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
    check(enumerateDevice(vk.physical, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties");
    std::vector<VkExtensionProperties> extensions(count);
    check(enumerateDevice(vk.physical, nullptr, &count, extensions.data()), "vkEnumerateDeviceExtensionProperties");
    extensions.resize(count);
    VkPhysicalDeviceFeatures available{};
    vk.Instance<PFN_vkGetPhysicalDeviceFeatures>("vkGetPhysicalDeviceFeatures")(vk.physical, &available);
    if (available.shaderInt64 != VK_TRUE) throw std::runtime_error("Vulkan: the device lacks shaderInt64");
    if (!hasExtension(extensions, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME)) throw std::runtime_error("Vulkan: the device lacks VK_KHR_shader_float_controls");
    const auto features2 = vk.Instance<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2");
    auto byteFeatures = QueryBdaByteFeatures(vk.physical, features2, extensions);
    auto addressFeatures = QueryBdaFeatures(vk.physical, features2, extensions);
    std::vector<const char*> enabled{VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};
    if (hasExtension(extensions, VK_KHR_SPIRV_1_4_EXTENSION_NAME)) enabled.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    if (hasExtension(extensions, "VK_KHR_portability_subset")) enabled.push_back("VK_KHR_portability_subset");
    VkPhysicalDeviceImageViewMinLodFeaturesEXT minLodFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT};
    if (hasExtension(extensions, VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &minLodFeatures};
        features2(vk.physical, &query);
        vk.minLod = minLodFeatures.minLod == VK_TRUE;
        if (vk.minLod) enabled.push_back(VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME);
    }
    minLodFeatures.pNext = nullptr;
    if (hasExtension(extensions, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME)) {
        VkPhysicalDeviceExternalMemoryHostPropertiesEXT host{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 hostProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &host};
        vk.Instance<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(vk.physical, &hostProperties);
        if (host.minImportedHostPointerAlignment != 0 && (host.minImportedHostPointerAlignment & (host.minImportedHostPointerAlignment - 1)) == 0) {
            vk.hostImport = true;
            vk.hostAlignment = host.minImportedHostPointerAlignment;
            enabled.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
            if (hasExtension(extensions, VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME)) enabled.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
        }
    }
    VkPhysicalDeviceFeatures features{};
    features.shaderInt64 = VK_TRUE;
    features.samplerAnisotropy = available.samplerAnisotropy;
    features.textureCompressionBC = available.textureCompressionBC;
    features.imageCubeArray = available.imageCubeArray;
    features.shaderStorageImageReadWithoutFormat = available.shaderStorageImageReadWithoutFormat;
    features.shaderStorageImageWriteWithoutFormat = available.shaderStorageImageWriteWithoutFormat;
    features.fragmentStoresAndAtomics = available.fragmentStoresAndAtomics;
    features.vertexPipelineStoresAndAtomics = available.vertexPipelineStoresAndAtomics;
    features.independentBlend = available.independentBlend;
    features.dualSrcBlend = available.dualSrcBlend;
    features.depthClamp = available.depthClamp;
    features.tessellationShader = available.tessellationShader;
    vk.anisotropy = available.samplerAnisotropy == VK_TRUE;
    addressFeatures.pNext = &byteFeatures;
    byteFeatures.pNext = vk.minLod ? &minLodFeatures : nullptr;
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = vk.family;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &addressFeatures};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabled.size());
    deviceInfo.ppEnabledExtensionNames = enabled.data();
    deviceInfo.pEnabledFeatures = &features;
    check(vk.Instance<PFN_vkCreateDevice>("vkCreateDevice")(vk.physical, &deviceInfo, nullptr, &vk.device), "vkCreateDevice");
    vk.Device<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(vk.device, vk.family, 0, &vk.queue);
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = vk.family;
    check(vk.Device<PFN_vkCreateCommandPool>("vkCreateCommandPool")(vk.device, &poolInfo, nullptr, &vk.pool), "vkCreateCommandPool");
}

ReplayDevice::~ReplayDevice() = default;

std::uint32_t ReplayDevice::SubgroupSize() const {
    return state->subgroupSize;
}

std::string ReplayDevice::Describe() const {
    const auto& vk = *state;
    char text[512];
    std::snprintf(text, sizeof(text), "%s, Vulkan %u.%u.%u, subgroup size %u, %s, host memory import %s", vk.properties.deviceName, VK_API_VERSION_MAJOR(vk.properties.apiVersion), VK_API_VERSION_MINOR(vk.properties.apiVersion), VK_API_VERSION_PATCH(vk.properties.apiVersion), vk.subgroupSize, vk.timestampBits != 0 ? "GPU timestamps" : "no GPU timestamps (CPU wall time instead)", vk.hostImport ? "available" : "unavailable");
    return text;
}

namespace {

class Session {
public:
    struct Buffer {
        VkBuffer handle = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        std::byte* mapping = nullptr;
        void* host = nullptr;
        VkDeviceSize size = 0;
        VkDeviceAddress address = 0;
    };
    struct Image {
        VkImage handle = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    explicit Session(ReplayDevice::State& vk) : vk(vk) {
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        check(vk.Device<PFN_vkCreateFence>("vkCreateFence")(vk.device, &fenceInfo, nullptr, &fence), "vkCreateFence");
    }

    ~Session() {
        try {
            vk.Device<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(vk.device);
            const auto destroyPipeline = vk.Device<PFN_vkDestroyPipeline>("vkDestroyPipeline");
            if (pipeline) destroyPipeline(vk.device, pipeline, nullptr);
            if (module) vk.Device<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(vk.device, module, nullptr);
            if (pipelineLayout) vk.Device<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(vk.device, pipelineLayout, nullptr);
            if (descriptorPool) vk.Device<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(vk.device, descriptorPool, nullptr);
            if (setLayout) vk.Device<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(vk.device, setLayout, nullptr);
            if (queries) vk.Device<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(vk.device, queries, nullptr);
            for (const auto sampler : samplers) vk.Device<PFN_vkDestroySampler>("vkDestroySampler")(vk.device, sampler, nullptr);
            for (const auto view : views) vk.Device<PFN_vkDestroyImageView>("vkDestroyImageView")(vk.device, view, nullptr);
            for (auto& image : images) {
                if (image.handle) vk.Device<PFN_vkDestroyImage>("vkDestroyImage")(vk.device, image.handle, nullptr);
                if (image.memory) vk.Device<PFN_vkFreeMemory>("vkFreeMemory")(vk.device, image.memory, nullptr);
            }
            for (auto& buffer : buffers) Destroy(buffer);
            if (fence) vk.Device<PFN_vkDestroyFence>("vkDestroyFence")(vk.device, fence, nullptr);
        } catch (...) {
        }
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    Buffer& CreateBuffer(VkDeviceSize size, VkMemoryPropertyFlags flags, bool import, bool addressable) {
        auto& buffer = buffers.emplace_back();
        buffer.size = std::max<VkDeviceSize>(size, 4);
        const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | (addressable ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
        const bool imported = import && vk.hostImport;
        const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT};
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, imported ? &external : nullptr};
        info.size = buffer.size;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vk.Device<PFN_vkCreateBuffer>("vkCreateBuffer")(vk.device, &info, nullptr, &buffer.handle), "vkCreateBuffer");
        VkMemoryRequirements requirements{};
        vk.Device<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(vk.device, buffer.handle, &requirements);
        const VkMemoryAllocateFlagsInfo addressFlags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0};
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, addressable ? &addressFlags : nullptr};
        VkImportMemoryHostPointerInfoEXT hostImport{VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT, addressable ? &addressFlags : nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, nullptr};
        if (imported) {
            const auto bytes = alignUp(std::max(buffer.size, requirements.size), vk.hostAlignment);
            buffer.host = allocateHost(static_cast<std::size_t>(bytes), static_cast<std::size_t>(vk.hostAlignment));
            if (buffer.host == nullptr) throw std::runtime_error("cannot allocate " + std::to_string(bytes) + " bytes of host memory to import");
            VkMemoryHostPointerPropertiesEXT pointer{VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
            check(vk.Device<PFN_vkGetMemoryHostPointerPropertiesEXT>("vkGetMemoryHostPointerPropertiesEXT")(vk.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, buffer.host, &pointer), "vkGetMemoryHostPointerPropertiesEXT");
            hostImport.pHostPointer = buffer.host;
            allocation.pNext = &hostImport;
            allocation.allocationSize = bytes;
            allocation.memoryTypeIndex = vk.MemoryType(pointer.memoryTypeBits & requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        } else {
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = vk.MemoryType(requirements.memoryTypeBits, flags);
        }
        check(vk.Device<PFN_vkAllocateMemory>("vkAllocateMemory")(vk.device, &allocation, nullptr, &buffer.memory), "vkAllocateMemory buffer");
        check(vk.Device<PFN_vkBindBufferMemory>("vkBindBufferMemory")(vk.device, buffer.handle, buffer.memory, 0), "vkBindBufferMemory");
        if (imported) buffer.mapping = static_cast<std::byte*>(buffer.host);
        else if ((vk.memory.memoryTypes[allocation.memoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
            void* mapping = nullptr;
            check(vk.Device<PFN_vkMapMemory>("vkMapMemory")(vk.device, buffer.memory, 0, VK_WHOLE_SIZE, 0, &mapping), "vkMapMemory");
            buffer.mapping = static_cast<std::byte*>(mapping);
        }
        if (addressable) {
            const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, buffer.handle};
            buffer.address = vk.Device<PFN_vkGetBufferDeviceAddressKHR>("vkGetBufferDeviceAddressKHR")(vk.device, &addressInfo);
            if (buffer.address == 0) throw std::runtime_error("vkGetBufferDeviceAddressKHR returned a null address");
        }
        return buffer;
    }

    Buffer& CreateStaging(VkDeviceSize size, bool readback) {
        if (readback) {
            try {
                return CreateBuffer(size, HostMemory | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, false, false);
            } catch (const std::exception&) {
                if (!buffers.empty() && buffers.back().mapping == nullptr) Destroy(buffers.back());
            }
        }
        return CreateBuffer(size, HostMemory, false, false);
    }

    void Destroy(Buffer& buffer) noexcept {
        try {
            if (buffer.mapping != nullptr && buffer.host == nullptr) vk.Device<PFN_vkUnmapMemory>("vkUnmapMemory")(vk.device, buffer.memory);
            if (buffer.handle) vk.Device<PFN_vkDestroyBuffer>("vkDestroyBuffer")(vk.device, buffer.handle, nullptr);
            if (buffer.memory) vk.Device<PFN_vkFreeMemory>("vkFreeMemory")(vk.device, buffer.memory, nullptr);
        } catch (...) {
        }
        if (buffer.host != nullptr) freeHost(buffer.host);
        buffer = Buffer{};
    }

    Image& CreateImage(const VkImageCreateInfo& info) {
        auto& image = images.emplace_back();
        check(vk.Device<PFN_vkCreateImage>("vkCreateImage")(vk.device, &info, nullptr, &image.handle), "vkCreateImage");
        VkMemoryRequirements requirements{};
        vk.Device<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(vk.device, image.handle, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = vk.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vk.Device<PFN_vkAllocateMemory>("vkAllocateMemory")(vk.device, &allocation, nullptr, &image.memory), "vkAllocateMemory image");
        check(vk.Device<PFN_vkBindImageMemory>("vkBindImageMemory")(vk.device, image.handle, image.memory, 0), "vkBindImageMemory");
        return image;
    }

    VkImageView CreateView(const VkImageViewCreateInfo& info) {
        VkImageView view = VK_NULL_HANDLE;
        check(vk.Device<PFN_vkCreateImageView>("vkCreateImageView")(vk.device, &info, nullptr, &view), "vkCreateImageView");
        views.push_back(view);
        return view;
    }

    VkSampler CreateSampler(const VkSamplerCreateInfo& info) {
        VkSampler sampler = VK_NULL_HANDLE;
        check(vk.Device<PFN_vkCreateSampler>("vkCreateSampler")(vk.device, &info, nullptr, &sampler), "vkCreateSampler");
        samplers.push_back(sampler);
        return sampler;
    }

    VkCommandBuffer Begin() {
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = vk.pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        check(vk.Device<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(vk.device, &allocation, &commands), "vkAllocateCommandBuffers");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vk.Device<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin), "vkBeginCommandBuffer");
        return commands;
    }

    std::chrono::nanoseconds Submit(VkCommandBuffer commands) {
        check(vk.Device<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer");
        check(vk.Device<PFN_vkResetFences>("vkResetFences")(vk.device, 1, &fence), "vkResetFences");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commands;
        const auto start = std::chrono::steady_clock::now();
        check(vk.Device<PFN_vkQueueSubmit>("vkQueueSubmit")(vk.queue, 1, &submit, fence), "vkQueueSubmit");
        const auto waited = vk.Device<PFN_vkWaitForFences>("vkWaitForFences")(vk.device, 1, &fence, VK_TRUE, FenceTimeout);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        vk.Device<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(vk.device, vk.pool, 1, &commands);
        check(waited, "vkWaitForFences");
        return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
    }

    void Barrier(VkCommandBuffer commands, VkPipelineStageFlags source, VkAccessFlags sourceAccess, VkPipelineStageFlags destination, VkAccessFlags destinationAccess, std::span<const VkImageMemoryBarrier> images = {}) {
        const VkMemoryBarrier memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, sourceAccess, destinationAccess};
        vk.Device<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, source, destination, 0, 1, &memory, 0, nullptr, static_cast<std::uint32_t>(images.size()), images.data());
    }

    void Upload(Buffer& target, VkDeviceSize offset, std::span<const std::byte> bytes) {
        if (bytes.empty()) return;
        if (offset + bytes.size() > target.size) throw std::runtime_error("replay upload exceeds its buffer");
        if (target.mapping != nullptr) {
            std::memcpy(target.mapping + offset, bytes.data(), bytes.size());
            return;
        }
        auto& staging = CreateStaging(bytes.size(), false);
        std::memcpy(staging.mapping, bytes.data(), bytes.size());
        const auto commands = Begin();
        const VkBufferCopy copy{0, offset, bytes.size()};
        vk.Device<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, staging.handle, target.handle, 1, &copy);
        Barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        Submit(commands);
        Destroy(staging);
    }

    std::vector<std::byte> Download(const Buffer& source, VkDeviceSize offset, VkDeviceSize size) {
        if (offset + size > source.size) throw std::runtime_error("replay download exceeds its buffer");
        if (size == 0) return {};
        auto& staging = CreateStaging(size, true);
        const auto commands = Begin();
        Barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        const VkBufferCopy copy{offset, 0, size};
        vk.Device<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, source.handle, staging.handle, 1, &copy);
        Barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
        Submit(commands);
        std::vector<std::byte> bytes(staging.mapping, staging.mapping + size);
        Destroy(staging);
        return bytes;
    }

    ReplayDevice::State& vk;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkQueryPool queries = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

private:
    std::deque<Buffer> buffers;
    std::deque<Image> images;
    std::vector<VkImageView> views;
    std::vector<VkSampler> samplers;
};

struct Required {
    std::uint32_t binding = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    std::uint32_t count = 1;
    std::optional<DescriptorRole> role;
    std::vector<std::uint32_t> words;
    std::vector<std::uint32_t> samplerCompare;
};

struct Program {
    std::vector<std::uint32_t> spirv;
    std::vector<Required> bindings;
    std::vector<std::byte> push;
    std::uint32_t lanes = 1;
};

VkDescriptorType descriptorTypeOf(const ShaderRecompiler::DescriptorBinding& binding) {
    if (binding.role == DescriptorRole::GuestImages) return binding.kind == ShaderRecompiler::DescriptorKind::StorageImage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    if (binding.role == DescriptorRole::GuestSamplers) return VK_DESCRIPTOR_TYPE_SAMPLER;
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
}

Program prepareProgram(const CaptureManifest& manifest, const std::filesystem::path& directory, const ReplayOptions& options, const std::vector<std::uint32_t>& captured, ReplayResult& result) {
    Program program;
    if (options.shader == ReplayShader::Recompile) {
        if (manifest.request.empty()) throw std::runtime_error("the capture holds no recompile request; replay it with --captured-spirv");
        const auto payload = readRequestPayload(directory / manifest.request);
        auto deserialized = ShaderRecompiler::RequestSerializer{}.Deserialize(payload);
        auto& request = deserialized.request;
        request.shader.codeHash = manifest.codeHash;
        request.useCache = false;
        const auto start = std::chrono::steady_clock::now();
        const auto compiled = ShaderRecompiler::Recompile(request);
        result.compileMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        program.spirv = compiled.spirv.Words();
        program.lanes = compiled.lanesPerInvocation;
        for (const auto& binding : compiled.bindings) {
            Required required;
            required.binding = binding.binding;
            required.type = descriptorTypeOf(binding);
            required.count = binding.count;
            required.role = binding.role;
            required.words = binding.guestDescriptor;
            for (const bool compare : binding.samplerDepthCompare) required.samplerCompare.push_back(compare ? 1u : 0u);
            program.bindings.push_back(std::move(required));
        }
        if (!compiled.pushConstants.empty()) {
            if (compiled.pushConstants.size() > PushConstantBytes) throw std::runtime_error("the recompiled program's push constants exceed the pipeline push constant block");
            program.push.assign(PushConstantBytes, std::byte{0});
            std::memcpy(program.push.data(), compiled.pushConstants.data(), compiled.pushConstants.size());
        }
        return program;
    }
    program.spirv = options.shader == ReplayShader::File ? readWords(options.spirvFile) : captured;
    program.lanes = manifest.lanes;
    program.push = manifest.push;
    for (const auto& layout : manifest.layout) {
        Required required;
        required.binding = layout.binding;
        required.type = static_cast<VkDescriptorType>(layout.type);
        required.count = layout.count;
        const auto descriptor = std::find_if(manifest.descriptors.begin(), manifest.descriptors.end(), [&](const CaptureManifest::Descriptor& candidate) { return candidate.binding == layout.binding; });
        if (descriptor != manifest.descriptors.end()) {
            required.role = static_cast<DescriptorRole>(descriptor->role);
            required.words = descriptor->words;
            required.samplerCompare = descriptor->samplerDepthCompare;
        }
        program.bindings.push_back(std::move(required));
    }
    return program;
}

class Replayer {
public:
    Replayer(ReplayDevice::State& vk, const CaptureManifest& manifest, const std::filesystem::path& root, const ReplayOptions& options, ReplayResult& result) : vk(vk), manifest(manifest), root(root), options(options), result(result), session(vk) {}

    void Run(const Program& program) {
        createRegions();
        createBindings(program);
        createPipeline(program);
        prepareRestores();
        const auto runs = options.warmup + std::max(options.iterations, 1u);
        for (std::uint32_t run = 0; run < runs; ++run) {
            restore();
            const auto time = dispatch(program);
            if (run >= options.warmup) result.milliseconds.push_back(time);
        }
        auto sorted = result.milliseconds;
        std::sort(sorted.begin(), sorted.end());
        result.minimum = sorted.front();
        result.median = sorted.size() % 2 == 1 ? sorted[sorted.size() / 2] : (sorted[sorted.size() / 2 - 1] + sorted[sorted.size() / 2]) / 2.0;
        compareOutputs();
    }

private:
    struct ReplayRegion {
        const CaptureManifest::Region* captured = nullptr;
        Session::Buffer* buffer = nullptr;
        std::vector<std::byte> before;
    };
    struct ReplayImage {
        Session::Image* image = nullptr;
        Session::Buffer* pristine = nullptr;
    };
    struct BufferRestore {
        Session::Buffer* target = nullptr;
        VkDeviceSize offset = 0;
        Session::Buffer* pristine = nullptr;
        VkDeviceSize size = 0;
    };
    struct Output {
        std::string label;
        Session::Buffer* buffer = nullptr;
        VkDeviceSize offset = 0;
        VkDeviceSize size = 0;
        std::string blob;
        bool faultRecord = false;
    };

    VkMemoryPropertyFlags memoryFor(std::uint32_t captured) const {
        if (options.memory == ReplayMemory::DeviceLocal) return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        return captured != 0 ? captured : HostMemory;
    }

    const ReplayRegion* regionFor(std::uint64_t begin, std::uint64_t end) const {
        for (const auto& region : regions) {
            if (begin >= region.captured->begin && end <= region.captured->end) return &region;
        }
        return nullptr;
    }

    std::span<const std::uint32_t> capturedWords(std::uint32_t binding, std::uint32_t element, std::uint32_t stride) const {
        for (const auto& descriptor : manifest.descriptors) {
            if (descriptor.binding == binding) return elementWords(descriptor.words, element, stride);
        }
        return {};
    }

    std::optional<DescriptorRole> capturedRole(std::uint32_t binding) const {
        for (const auto& descriptor : manifest.descriptors) {
            if (descriptor.binding == binding) return static_cast<DescriptorRole>(descriptor.role);
        }
        return std::nullopt;
    }

    void createRegions() {
        for (const auto& region : manifest.regions) {
            auto bytes = Graphics::ReadCaptureBlob(root, region.blob);
            if (bytes.size() != region.padding + (region.end - region.begin)) throw std::runtime_error("region " + hex(region.begin) + " blob holds " + std::to_string(bytes.size()) + " bytes instead of " + std::to_string(region.padding + region.end - region.begin));
            auto& buffer = session.CreateBuffer(bytes.size(), memoryFor(region.memory), options.memory == ReplayMemory::Captured && region.imported, true);
            session.Upload(buffer, 0, bytes);
            ReplayRegion replay{&region, &buffer, {}};
            const bool written = std::any_of(manifest.writes.begin(), manifest.writes.end(), [&](const CaptureManifest::Write& write) { return write.begin >= region.begin && write.end <= region.end; });
            if (written) replay.before = std::move(bytes);
            regions.push_back(std::move(replay));
        }
        if (std::any_of(manifest.regions.begin(), manifest.regions.end(), [](const CaptureManifest::Region& region) { return region.imported; }) && options.memory == ReplayMemory::Captured && !vk.hostImport) {
            result.notes.push_back("the device cannot import host memory; regions the driver imported live in host-visible device memory instead");
        }
    }

    const CaptureManifest::Buffer* findBuffer(const Required& required, std::uint32_t element, std::optional<DescriptorRole> role) const {
        const auto words = elementWords(required.words, element, 4);
        for (const auto& buffer : manifest.buffers) {
            if (buffer.binding == required.binding && buffer.element == element && (role != DescriptorRole::GuestBuffers || words.empty() || sameWords(capturedWords(buffer.binding, buffer.element, 4), words))) return &buffer;
        }
        for (const auto& buffer : manifest.buffers) {
            const auto bufferRole = capturedRole(buffer.binding);
            if (role == DescriptorRole::GuestBuffers && bufferRole == DescriptorRole::GuestBuffers && !words.empty() && sameWords(capturedWords(buffer.binding, buffer.element, 4), words)) return &buffer;
            if (role.has_value() && role != DescriptorRole::GuestBuffers && bufferRole == role) return &buffer;
        }
        return nullptr;
    }

    VkDescriptorBufferInfo resolveBuffer(const Required& required, std::uint32_t element) {
        const auto role = required.role.has_value() ? required.role : capturedRole(required.binding);
        const auto* captured = findBuffer(required, element, role);
        const auto label = "binding " + std::to_string(required.binding) + " element " + std::to_string(element);
        auto source = captured != nullptr ? std::optional<CaptureBufferSource>(captured->source) : std::nullopt;
        if (role == DescriptorRole::FaultBuffer) source = CaptureBufferSource::Fault;
        if (role == DescriptorRole::BdaPagetable) source = CaptureBufferSource::Table;
        if ((role == DescriptorRole::FlattenedSrt || role == DescriptorRole::ShaderData) && !required.words.empty()) source = CaptureBufferSource::Data;
        if (!source) throw std::runtime_error(label + ": the program binds a buffer the capture lacks");
        switch (*source) {
            case CaptureBufferSource::Guest: {
                const auto* region = regionFor(captured->address, captured->address + captured->size);
                if (region == nullptr) throw std::runtime_error(label + ": guest buffer " + hex(captured->address) + " lies outside every captured region");
                const auto residue = captured->address % ViewAlignment;
                const auto offset = captured->address - residue - (region->captured->begin - region->captured->padding);
                return {region->buffer->handle, offset, captured->size + residue};
            }
            case CaptureBufferSource::Zero: {
                auto& buffer = session.CreateBuffer(captured->size, memoryFor(captured->memory), false, false);
                session.Upload(buffer, 0, std::vector<std::byte>(static_cast<std::size_t>(buffer.size)));
                return {buffer.handle, 0, captured->size};
            }
            case CaptureBufferSource::Data: {
                std::vector<std::byte> bytes;
                if (!required.words.empty()) bytes.assign(reinterpret_cast<const std::byte*>(required.words.data()), reinterpret_cast<const std::byte*>(required.words.data() + required.words.size()));
                else bytes = Graphics::ReadCaptureBlob(root, captured->blob);
                auto& buffer = session.CreateBuffer(bytes.size(), memoryFor(captured != nullptr ? captured->memory : 0), false, false);
                session.Upload(buffer, 0, bytes);
                return {buffer.handle, 0, bytes.size()};
            }
            case CaptureBufferSource::Gds: {
                const auto bytes = Graphics::ReadCaptureBlob(root, captured->blob);
                auto& buffer = session.CreateBuffer(bytes.size(), memoryFor(captured->memory), false, false);
                session.Upload(buffer, 0, bytes);
                auto& pristine = session.CreateStaging(bytes.size(), false);
                std::memcpy(pristine.mapping, bytes.data(), bytes.size());
                bufferRestores.push_back({&buffer, 0, &pristine, bytes.size()});
                if (!captured->after.empty()) outputs.push_back({"GDS", &buffer, 0, bytes.size(), captured->after, false});
                return {buffer.handle, 0, bytes.size()};
            }
            case CaptureBufferSource::Table: {
                if (captured == nullptr) throw std::runtime_error(label + ": the capture holds no BDA page table");
                auto bytes = Graphics::ReadCaptureBlob(root, captured->blob);
                patchTable(bytes);
                auto& buffer = session.CreateBuffer(bytes.size(), memoryFor(captured->memory), false, false);
                session.Upload(buffer, 0, bytes);
                return {buffer.handle, 0, bytes.size()};
            }
            case CaptureBufferSource::Fault: {
                constexpr VkDeviceSize faultBytes = sizeof(ShaderRecompiler::BdaAbi::Fault);
                auto& buffer = session.CreateBuffer(faultBytes, memoryFor(captured != nullptr ? captured->memory : 0), false, false);
                session.Upload(buffer, 0, std::vector<std::byte>(static_cast<std::size_t>(faultBytes)));
                faultBuffers.push_back(&buffer);
                if (captured != nullptr && !captured->after.empty()) outputs.push_back({"BDA fault record", &buffer, 0, faultBytes, captured->after, true});
                return {buffer.handle, 0, faultBytes};
            }
        }
        throw std::runtime_error(label + ": unknown buffer source");
    }

    void patchTable(std::vector<std::byte>& bytes) const {
        namespace Abi = ShaderRecompiler::BdaAbi;
        if (bytes.size() < sizeof(Abi::Header)) throw std::runtime_error("the captured BDA page table is truncated");
        Abi::Header header{};
        std::memcpy(&header, bytes.data(), sizeof(header));
        if (bytes.size() != sizeof(Abi::Header) + static_cast<std::size_t>(header.count) * sizeof(Abi::Range)) throw std::runtime_error("the captured BDA page table size disagrees with its range count");
        for (std::uint32_t index = 0; index < header.count; ++index) {
            Abi::Range range{};
            const auto offset = sizeof(Abi::Header) + index * sizeof(Abi::Range);
            std::memcpy(&range, bytes.data() + offset, sizeof(range));
            const auto* region = regionFor(range.begin, range.end);
            if (region == nullptr) throw std::runtime_error("BDA range " + hex(range.begin) + "-" + hex(range.end) + " has no captured region");
            range.deviceAddress = region->buffer->address + region->captured->padding + (range.begin - region->captured->begin);
            std::memcpy(bytes.data() + offset, &range, sizeof(range));
        }
    }

    const CaptureManifest::View* findView(const Required& required, std::uint32_t element) const {
        const auto words = elementWords(required.words, element, 8);
        for (const auto& view : manifest.views) {
            if (view.binding == required.binding && view.element == element && view.type == static_cast<std::uint32_t>(required.type) && (words.empty() || sameWords(capturedWords(view.binding, view.element, 8), words))) return &view;
        }
        if (words.empty()) return nullptr;
        for (const auto& view : manifest.views) {
            if (view.type == static_cast<std::uint32_t>(required.type) && sameWords(capturedWords(view.binding, view.element, 8), words)) return &view;
        }
        return nullptr;
    }

    const CaptureManifest::Sampler* findSampler(const Required& required, std::uint32_t element) const {
        const auto words = elementWords(required.words, element, 4);
        const std::optional<std::uint32_t> compare = element < required.samplerCompare.size() ? std::optional<std::uint32_t>(required.samplerCompare[element]) : std::nullopt;
        for (const auto& sampler : manifest.samplers) {
            if (sampler.binding == required.binding && sampler.element == element && (words.empty() || sameWords(capturedWords(sampler.binding, sampler.element, 4), words))) return &sampler;
        }
        if (words.empty()) return nullptr;
        for (const auto& sampler : manifest.samplers) {
            if (sameWords(capturedWords(sampler.binding, sampler.element, 4), words) && (!compare || sampler.compareEnable == *compare)) return &sampler;
        }
        return nullptr;
    }

    std::vector<VkBufferImageCopy> copiesOf(std::span<const CaptureManifest::Subresource> subresources) const {
        std::vector<VkBufferImageCopy> copies;
        for (const auto& subresource : subresources) {
            VkBufferImageCopy copy{};
            copy.bufferOffset = subresource.offset;
            copy.imageSubresource = {subresource.aspect, subresource.level, subresource.layer, 1};
            copy.imageExtent = {subresource.width, subresource.height, subresource.depth};
            copies.push_back(copy);
        }
        return copies;
    }

    VkImageMemoryBarrier imageBarrier(VkImage image, VkFormat format, VkImageLayout from, VkImageLayout to, VkAccessFlags source, VkAccessFlags destination) const {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = source;
        barrier.dstAccessMask = destination;
        barrier.oldLayout = from;
        barrier.newLayout = to;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {Graphics::CaptureFormatAspects(format), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
        return barrier;
    }

    ReplayImage& imageFor(std::uint32_t index) {
        if (index >= manifest.images.size()) throw std::runtime_error("a view refers to an unknown image");
        auto& replay = images[index];
        if (replay.image != nullptr) return replay;
        const auto& captured = manifest.images[index];
        if (captured.format == 0 || captured.levels == 0 || captured.layers == 0 || captured.width == 0) throw std::runtime_error("image " + std::to_string(index) + " parameters were not captured");
        const auto format = static_cast<VkFormat>(captured.format);
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.flags = captured.flags;
        info.imageType = static_cast<VkImageType>(captured.type);
        info.format = format;
        info.extent = {captured.width, std::max(captured.height, 1u), std::max(captured.depth, 1u)};
        info.mipLevels = captured.levels;
        info.arrayLayers = captured.layers;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = captured.usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        replay.image = &session.CreateImage(info);
        std::vector<std::byte> before;
        if (!captured.blob.empty()) before = Graphics::ReadCaptureBlob(root, captured.blob);
        else result.notes.push_back("image " + std::to_string(index) + " contents were not captured; it starts cleared to zero");
        for (const auto& subresource : captured.contents) {
            if (subresource.offset + subresource.size > before.size() && !before.empty()) throw std::runtime_error("image " + std::to_string(index) + " blob is shorter than its subresources");
        }
        Session::Buffer* staging = nullptr;
        if (!before.empty()) {
            staging = &session.CreateStaging(before.size(), false);
            std::memcpy(staging->mapping, before.data(), before.size());
        }
        const auto layout = layoutOf(captured);
        const auto commands = session.Begin();
        const auto toTransfer = imageBarrier(replay.image->handle, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        session.Barrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, std::span(&toTransfer, 1));
        const auto aspects = Graphics::CaptureFormatAspects(format);
        const auto block = Graphics::CaptureBlockOf(format, VK_IMAGE_ASPECT_COLOR_BIT);
        const VkImageSubresourceRange everything{aspects, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
        if (aspects == VK_IMAGE_ASPECT_COLOR_BIT && block && block->width == 1) {
            const VkClearColorValue clear{};
            vk.Device<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, replay.image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &everything);
        } else if (aspects != VK_IMAGE_ASPECT_COLOR_BIT) {
            const VkClearDepthStencilValue clear{0.0f, 0};
            vk.Device<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, replay.image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &everything);
        }
        if (staging != nullptr) {
            session.Barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            const auto copies = copiesOf(captured.contents);
            vk.Device<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, staging->handle, replay.image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(copies.size()), copies.data());
        }
        const auto toLayout = imageBarrier(replay.image->handle, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        session.Barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, std::span(&toLayout, 1));
        session.Submit(commands);
        if (staging != nullptr && !captured.written.empty()) replay.pristine = staging;
        else if (staging != nullptr) session.Destroy(*staging);
        return replay;
    }

    VkDescriptorImageInfo resolveImage(const Required& required, std::uint32_t element) {
        const auto* view = findView(required, element);
        if (view == nullptr) throw std::runtime_error("binding " + std::to_string(required.binding) + " element " + std::to_string(element) + ": the program binds an image the capture lacks");
        if (view->format == 0) throw std::runtime_error("binding " + std::to_string(required.binding) + " element " + std::to_string(element) + ": the view parameters were not captured");
        auto& replay = imageFor(view->image);
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.image = replay.image->handle;
        info.viewType = static_cast<VkImageViewType>(view->viewType);
        info.format = static_cast<VkFormat>(view->format);
        info.components = {static_cast<VkComponentSwizzle>(view->components[0]), static_cast<VkComponentSwizzle>(view->components[1]), static_cast<VkComponentSwizzle>(view->components[2]), static_cast<VkComponentSwizzle>(view->components[3])};
        info.subresourceRange = {view->aspect, view->baseLevel, view->levels, view->baseLayer, view->layers};
        VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
        usage.usage = view->usage;
        VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
        minLod.minLod = view->minLod;
        const void* chain = nullptr;
        if (view->minLod > 0.0f && vk.minLod) {
            minLod.pNext = chain;
            chain = &minLod;
        }
        if (view->usage != 0) {
            usage.pNext = chain;
            chain = &usage;
        }
        info.pNext = chain;
        const auto& captured = manifest.images[view->image];
        if (view->type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && !captured.written.empty()) storageImages.insert(view->image);
        return {VK_NULL_HANDLE, session.CreateView(info), static_cast<VkImageLayout>(view->layout)};
    }

    VkDescriptorImageInfo resolveSampler(const Required& required, std::uint32_t element) {
        const auto* sampler = findSampler(required, element);
        if (sampler == nullptr) throw std::runtime_error("binding " + std::to_string(required.binding) + " element " + std::to_string(element) + ": the program binds a sampler the capture lacks");
        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        info.magFilter = static_cast<VkFilter>(sampler->magFilter);
        info.minFilter = static_cast<VkFilter>(sampler->minFilter);
        info.mipmapMode = static_cast<VkSamplerMipmapMode>(sampler->mipmapMode);
        info.addressModeU = static_cast<VkSamplerAddressMode>(sampler->addressU);
        info.addressModeV = static_cast<VkSamplerAddressMode>(sampler->addressV);
        info.addressModeW = static_cast<VkSamplerAddressMode>(sampler->addressW);
        info.mipLodBias = sampler->mipLodBias;
        info.anisotropyEnable = sampler->anisotropyEnable != 0 && vk.anisotropy ? VK_TRUE : VK_FALSE;
        info.maxAnisotropy = sampler->maxAnisotropy;
        info.compareEnable = sampler->compareEnable;
        info.compareOp = static_cast<VkCompareOp>(sampler->compareOp);
        info.minLod = sampler->minLod;
        info.maxLod = sampler->maxLod;
        info.borderColor = static_cast<VkBorderColor>(sampler->borderColor);
        info.unnormalizedCoordinates = sampler->unnormalized;
        if (sampler->anisotropyEnable != 0 && !vk.anisotropy) result.notes.push_back("the device lacks anisotropic filtering; a sampler filters without it");
        return {session.CreateSampler(info), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
    }

    void createBindings(const Program& program) {
        std::vector<VkDescriptorSetLayoutBinding> layout;
        std::map<VkDescriptorType, std::uint32_t> counts;
        for (const auto& required : program.bindings) {
            if (required.count == 0) throw std::runtime_error("binding " + std::to_string(required.binding) + " has no descriptors");
            layout.push_back({required.binding, required.type, required.count, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
            counts[required.type] += required.count;
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = static_cast<std::uint32_t>(layout.size());
        layoutInfo.pBindings = layout.data();
        check(vk.Device<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(vk.device, &layoutInfo, nullptr, &session.setLayout), "vkCreateDescriptorSetLayout");
        std::vector<VkDescriptorPoolSize> sizes;
        for (const auto& [type, count] : counts) sizes.push_back({type, count});
        if (sizes.empty()) sizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1});
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        poolInfo.pPoolSizes = sizes.data();
        check(vk.Device<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(vk.device, &poolInfo, nullptr, &session.descriptorPool), "vkCreateDescriptorPool");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, session.descriptorPool, 1, &session.setLayout};
        check(vk.Device<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(vk.device, &allocation, &session.set), "vkAllocateDescriptorSets");
        std::deque<std::vector<VkDescriptorBufferInfo>> bufferInfos;
        std::deque<std::vector<VkDescriptorImageInfo>> imageInfos;
        std::vector<VkWriteDescriptorSet> writes;
        for (const auto& required : program.bindings) {
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = session.set;
            write.dstBinding = required.binding;
            write.descriptorCount = required.count;
            write.descriptorType = required.type;
            if (required.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                auto& infos = bufferInfos.emplace_back();
                for (std::uint32_t element = 0; element < required.count; ++element) infos.push_back(resolveBuffer(required, element));
                write.pBufferInfo = infos.data();
            } else {
                auto& infos = imageInfos.emplace_back();
                for (std::uint32_t element = 0; element < required.count; ++element) infos.push_back(required.type == VK_DESCRIPTOR_TYPE_SAMPLER ? resolveSampler(required, element) : resolveImage(required, element));
                write.pImageInfo = infos.data();
            }
            writes.push_back(write);
        }
        if (!writes.empty()) vk.Device<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(vk.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void createPipeline(const Program& program) {
        const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, PushConstantBytes};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &session.setLayout;
        layoutInfo.pushConstantRangeCount = program.push.empty() ? 0u : 1u;
        layoutInfo.pPushConstantRanges = program.push.empty() ? nullptr : &push;
        if (!program.push.empty() && (program.push.size() != PushConstantBytes || vk.properties.limits.maxPushConstantsSize < PushConstantBytes)) throw std::runtime_error("the push constant block does not fit the device");
        check(vk.Device<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(vk.device, &layoutInfo, nullptr, &session.pipelineLayout), "vkCreatePipelineLayout");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = program.spirv.size() * sizeof(std::uint32_t);
        moduleInfo.pCode = program.spirv.data();
        check(vk.Device<PFN_vkCreateShaderModule>("vkCreateShaderModule")(vk.device, &moduleInfo, nullptr, &session.module), "vkCreateShaderModule");
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, session.module, "main", nullptr};
        pipelineInfo.layout = session.pipelineLayout;
        check(vk.Device<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &session.pipeline), "vkCreateComputePipelines");
        if (vk.timestampBits != 0) {
            VkQueryPoolCreateInfo queryInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryInfo.queryCount = 2;
            check(vk.Device<PFN_vkCreateQueryPool>("vkCreateQueryPool")(vk.device, &queryInfo, nullptr, &session.queries), "vkCreateQueryPool");
        }
        result.timestamps = session.queries != VK_NULL_HANDLE;
    }

    void prepareRestores() {
        for (const auto& write : manifest.writes) {
            const auto* region = regionFor(write.begin, write.end);
            if (region == nullptr) throw std::runtime_error("written range " + hex(write.begin) + "-" + hex(write.end) + " has no captured region");
            const auto offset = region->captured->padding + (write.begin - region->captured->begin);
            const auto size = write.end - write.begin;
            auto& pristine = session.CreateStaging(size, false);
            std::memcpy(pristine.mapping, region->before.data() + offset, static_cast<std::size_t>(size));
            bufferRestores.push_back({region->buffer, offset, &pristine, size});
            outputs.push_back({"write " + hex(write.begin) + "-" + hex(write.end), region->buffer, offset, size, write.blob, false});
        }
        for (const auto index : storageImages) {
            if (images[index].pristine == nullptr) result.notes.push_back("image " + std::to_string(index) + " has no captured contents to restore between runs");
        }
    }

    static VkImageLayout layoutOf(const CaptureManifest::Image& image) {
        return image.layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_GENERAL : static_cast<VkImageLayout>(image.layout);
    }

    void restore() {
        const auto commands = session.Begin();
        session.Barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        for (const auto& entry : bufferRestores) {
            const VkBufferCopy copy{0, entry.offset, entry.size};
            vk.Device<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, entry.pristine->handle, entry.target->handle, 1, &copy);
        }
        for (auto* fault : faultBuffers) vk.Device<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, fault->handle, 0, VK_WHOLE_SIZE, 0);
        for (const auto index : storageImages) {
            const auto& replay = images[index];
            if (replay.pristine == nullptr) continue;
            const auto& captured = manifest.images[index];
            const auto format = static_cast<VkFormat>(captured.format);
            const auto layout = layoutOf(captured);
            std::vector<CaptureManifest::Subresource> sources;
            for (const auto& written : captured.written) {
                const auto found = std::find_if(captured.contents.begin(), captured.contents.end(), [&](const CaptureManifest::Subresource& content) { return content.aspect == written.aspect && content.level == written.level && content.layer == written.layer; });
                if (found != captured.contents.end()) sources.push_back(*found);
            }
            if (sources.empty()) continue;
            const auto copies = copiesOf(sources);
            const bool general = layout == VK_IMAGE_LAYOUT_GENERAL;
            if (!general) {
                const auto toTransfer = imageBarrier(replay.image->handle, format, layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
                session.Barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, std::span(&toTransfer, 1));
            }
            vk.Device<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, replay.pristine->handle, replay.image->handle, general ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(copies.size()), copies.data());
            if (!general) {
                const auto back = imageBarrier(replay.image->handle, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                session.Barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, std::span(&back, 1));
            }
        }
        session.Barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        session.Submit(commands);
    }

    double dispatch(const Program& program) {
        const auto commands = session.Begin();
        if (session.queries) vk.Device<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(commands, session.queries, 0, 2);
        session.Barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        if (session.queries) vk.Device<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, session.queries, 0);
        vk.Device<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, session.pipeline);
        vk.Device<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, session.pipelineLayout, 0, 1, &session.set, 0, nullptr);
        if (!program.push.empty()) vk.Device<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, session.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, PushConstantBytes, program.push.data());
        vk.Device<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, manifest.groups[0], manifest.groups[1], manifest.groups[2]);
        if (session.queries) vk.Device<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, session.queries, 1);
        session.Barrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT);
        const auto wall = session.Submit(commands);
        if (!session.queries) return std::chrono::duration<double, std::milli>(wall).count();
        std::array<std::uint64_t, 2> stamps{};
        check(vk.Device<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(vk.device, session.queries, 0, 2, sizeof(stamps), stamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "vkGetQueryPoolResults");
        const auto mask = vk.timestampBits >= 64 ? ~0ull : (1ull << vk.timestampBits) - 1ull;
        const auto ticks = (stamps[1] - stamps[0]) & mask;
        return static_cast<double>(ticks) * static_cast<double>(vk.properties.limits.timestampPeriod) / 1.0e6;
    }

    std::vector<std::byte> downloadImage(std::uint32_t index) {
        const auto& captured = manifest.images[index];
        const auto& replay = images[index];
        const auto format = static_cast<VkFormat>(captured.format);
        const auto layout = layoutOf(captured);
        std::uint64_t total = 0;
        for (const auto& written : captured.written) total = std::max(total, written.offset + written.size);
        auto& staging = session.CreateStaging(total, true);
        const auto commands = session.Begin();
        const bool general = layout == VK_IMAGE_LAYOUT_GENERAL;
        if (!general) {
            const auto toSource = imageBarrier(replay.image->handle, format, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            session.Barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, std::span(&toSource, 1));
        } else {
            session.Barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        }
        const auto copies = copiesOf(captured.written);
        vk.Device<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, replay.image->handle, general ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.handle, static_cast<std::uint32_t>(copies.size()), copies.data());
        if (!general) {
            const auto back = imageBarrier(replay.image->handle, format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            session.Barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_HOST_READ_BIT, std::span(&back, 1));
        } else {
            session.Barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
        }
        session.Submit(commands);
        std::vector<std::byte> bytes(staging.mapping, staging.mapping + total);
        session.Destroy(staging);
        return bytes;
    }

    void compare(const std::string& label, std::span<const std::byte> actual, std::span<const std::byte> expected) {
        ++result.compared;
        result.comparedBytes += expected.size();
        if (actual.size() != expected.size()) {
            result.mismatches.push_back(label + ": the replay produced " + std::to_string(actual.size()) + " bytes and the capture holds " + std::to_string(expected.size()));
            return;
        }
        std::size_t differing = 0;
        std::size_t first = 0;
        for (std::size_t index = 0; index < actual.size(); ++index) {
            if (actual[index] == expected[index]) continue;
            if (differing++ == 0) first = index;
        }
        if (differing == 0) return;
        char detail[160];
        std::snprintf(detail, sizeof(detail), ": %zu of %zu bytes differ, first at +0x%zx (replay 0x%02x, capture 0x%02x)", differing, actual.size(), first, static_cast<unsigned>(actual[first]), static_cast<unsigned>(expected[first]));
        result.mismatches.push_back(label + detail);
    }

    void compareOutputs() {
        for (const auto& output : outputs) {
            auto actual = session.Download(*output.buffer, output.offset, output.size);
            auto expected = Graphics::ReadCaptureBlob(root, output.blob);
            if (output.faultRecord) {
                actual.resize(std::min<std::size_t>(actual.size(), 8));
                expected.resize(std::min<std::size_t>(expected.size(), 8));
            }
            compare(output.label, actual, expected);
        }
        for (const auto index : storageImages) {
            const auto& captured = manifest.images[index];
            if (captured.after.empty() || images[index].image == nullptr) {
                result.notes.push_back("storage image " + std::to_string(index) + " has no captured results to compare");
                continue;
            }
            compare("storage image " + std::to_string(index), downloadImage(index), Graphics::ReadCaptureBlob(root, captured.after));
        }
    }

    ReplayDevice::State& vk;
    const CaptureManifest& manifest;
    const std::filesystem::path& root;
    const ReplayOptions& options;
    ReplayResult& result;
    Session session;
    std::vector<ReplayRegion> regions;
    std::map<std::uint32_t, ReplayImage> images;
    std::set<std::uint32_t> storageImages;
    std::vector<BufferRestore> bufferRestores;
    std::vector<Session::Buffer*> faultBuffers;
    std::vector<Output> outputs;
};

}

ReplayResult ReplayCapture(ReplayDevice& device, const std::filesystem::path& directory, const ReplayOptions& options) {
    auto& vk = device.Internal();
    const auto manifest = Graphics::ReadCaptureManifest(directory);
    const auto root = Graphics::CaptureBlobRoot(directory);
    ReplayResult result;
    result.directory = directory;
    result.program = manifest.program;
    result.codeHash = manifest.codeHash;
    result.groups = manifest.groups;
    result.notes = manifest.notes;
    for (auto& note : result.notes) note = "capture: " + note;
    const auto captured = readWords(directory / manifest.spirv);
    const auto program = prepareProgram(manifest, directory, options, captured, result);
    result.spirvWords = program.spirv.size();
    result.capturedSpirvWords = captured.size();
    result.spirvMatchesCapture = program.spirv == captured;
    result.lanes = program.lanes;
    if (!options.writeSpirv.empty()) {
        std::filesystem::create_directories(options.writeSpirv);
        const auto name = std::filesystem::absolute(directory).lexically_normal();
        std::ofstream file(options.writeSpirv / ((name.has_filename() ? name.filename() : name.parent_path().filename()).string() + ".spv"), std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(program.spirv.data()), static_cast<std::streamsize>(program.spirv.size() * sizeof(std::uint32_t)));
        if (!file) throw std::runtime_error("cannot write the replayed SPIR-V into " + options.writeSpirv.string());
    }
    if (manifest.groups[0] == 0 || manifest.groups[1] == 0 || manifest.groups[2] == 0) throw std::runtime_error("the capture dispatches no thread groups");
    Replayer replayer(vk, manifest, root, options, result);
    replayer.Run(program);
    return result;
}

std::vector<std::filesystem::path> FindCaptures(const std::filesystem::path& path) {
    std::error_code error;
    if (std::filesystem::is_regular_file(path / Graphics::CaptureManifestName, error)) return {path};
    if (!std::filesystem::is_directory(path, error)) throw std::runtime_error(path.string() + " is neither a dispatch capture nor a directory of captures");
    std::vector<std::filesystem::path> captures;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_directory(error) && std::filesystem::is_regular_file(entry.path() / Graphics::CaptureManifestName, error)) captures.push_back(entry.path());
    }
    std::sort(captures.begin(), captures.end());
    if (captures.empty()) throw std::runtime_error(path.string() + " holds no dispatch captures");
    return captures;
}

std::string DescribeReplay(const ReplayResult& result) {
    const auto name = std::filesystem::absolute(result.directory).lexically_normal();
    char line[512];
    std::snprintf(line, sizeof(line), "%s: program 0x%llx (code hash 0x%016llx) %ux%ux%u groups, %u guest lane(s) per invocation, %zu SPIR-V words (captured %zu, %s)", (name.has_filename() ? name.filename() : name.parent_path().filename()).string().c_str(), static_cast<unsigned long long>(result.program), static_cast<unsigned long long>(result.codeHash), result.groups[0], result.groups[1], result.groups[2], result.lanes, result.spirvWords, result.capturedSpirvWords, result.spirvMatchesCapture ? "identical" : "different");
    std::string text = line;
    if (result.compileMilliseconds > 0.0) {
        std::snprintf(line, sizeof(line), ", recompiled in %.1f ms", result.compileMilliseconds);
        text += line;
    }
    std::snprintf(line, sizeof(line), "\n  GPU time over %zu runs: min %.3f ms, median %.3f ms (%s)", result.milliseconds.size(), result.minimum, result.median, result.timestamps ? "GPU timestamps" : "CPU wall time");
    text += line;
    std::snprintf(line, sizeof(line), "\n  outputs: %zu compared (%llu bytes): %s", result.compared, static_cast<unsigned long long>(result.comparedBytes), result.mismatches.empty() ? (result.compared == 0 ? "nothing written to compare" : "identical to the capture") : "MISMATCH");
    text += line;
    for (const auto& mismatch : result.mismatches) text += "\n  mismatch: " + mismatch;
    for (const auto& note : result.notes) text += "\n  note: " + note;
    return text;
}

}

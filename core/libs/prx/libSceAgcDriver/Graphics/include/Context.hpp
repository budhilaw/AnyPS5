#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_CONTEXT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_CONTEXT_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <atomic>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {

class PipelineCache;
class GuestBufferCache;
class Buffer;

inline std::atomic<std::uint64_t>& QueueSubmissionCounter() {
    static std::atomic<std::uint64_t> counter{0};
    return counter;
}

class TextureDetiler;
class GpuColorTransfer;
class BufferPool;
class TextureCache;
class RenderCache;
class DrawQueue;
class GraphicsPipelineCache;
class DescriptorCache;
class SamplerCache;
class ReleaseQueue;

inline void Require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("AGC graphics: " + reason);
}

inline void Check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string("AGC graphics: ") + operation + ": Vulkan result " + std::to_string(result));
}

inline void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("AGC graphics: ") + reason);
}

struct Context {
    VkDevice device;
    VkPhysicalDevice physical;
    VkQueue queue;
    VkCommandPool pool;
    PFN_vkGetDeviceProcAddr deviceProc;
    PFN_vkGetPhysicalDeviceFormatProperties formatProperties;
    PFN_vkGetPhysicalDeviceImageFormatProperties imageFormatProperties;
    VkPhysicalDeviceMemoryProperties memory;
    VkPhysicalDeviceLimits limits;
    bool tessellationShader = false;
    bool meshShader = false;
    VkPhysicalDeviceMeshShaderPropertiesEXT meshLimits{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
    bool depthClipControl = false;
    bool depthRangeUnrestricted = false;
    bool bufferDeviceAddress = false;
    VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    bool fragmentShaderBarycentric = false;
    bool samplerAnisotropy = false;
    bool textureCompressionBC = false;
    bool storageImages = false;
    TextureDetiler* detiler = nullptr;
    GpuColorTransfer* colorTransfer = nullptr;
    mutable std::shared_ptr<BufferPool> bufferPool;
    TextureCache* textureCache = nullptr;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    RenderCache* renderCache = nullptr;
    DrawQueue* drawQueue = nullptr;
    GraphicsPipelineCache* graphicsPipelines = nullptr;
    mutable std::shared_ptr<DescriptorCache> descriptorCache;
    mutable std::shared_ptr<SamplerCache> samplerCache;

    template<typename TFunction>
    TFunction Function(const char* name) const {
        struct Cached {
            VkDevice device;
            const char* name;
            TFunction function;
        };
        thread_local Cached cached{};
        if (cached.function != nullptr && cached.device == device && cached.name == name) return cached.function;
        Require(deviceProc != nullptr, "missing Vulkan device function resolver");
        const auto function = reinterpret_cast<TFunction>(deviceProc(device, name));
        if (function == nullptr) throw std::runtime_error(std::string("AGC graphics: missing Vulkan function: ") + name);
        cached = {device, name, function};
        return function;
    }

    std::uint32_t MemoryType(std::uint32_t mask, VkMemoryPropertyFlags flags) const {
        for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
            if ((mask & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
        }
        throw std::runtime_error("AGC graphics: required Vulkan memory type is unavailable");
    }
    PipelineCache* pipelineCacheOwner = nullptr;
    GuestBufferCache* guestBufferCache = nullptr;
    Buffer* gds = nullptr;
    bool depthClamp = false;
    bool hostPointerImport = false;
    VkDeviceSize hostPointerAlignment = 0;
    std::shared_ptr<ReleaseQueue> releaseQueue;
    bool imageViewMinLod = false;
};

}

#endif

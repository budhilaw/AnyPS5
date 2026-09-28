#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RESOURCES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RESOURCES_HPP

#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <span>

namespace AgcDriver::Graphics {

class Buffer {
public:
    Buffer(const Context& context, std::size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    // A buffer over imported host memory (VK_EXT_external_memory_host): `importBytes` bytes at
    // `host` (both aligned to Context::hostPointerAlignment) back it; the buffer covers `size`
    // bytes from `offset` into them. Bytes() is that host memory itself.
    struct HostImport {};
    Buffer(const Context& context, HostImport, void* host, std::size_t importBytes, std::size_t offset, std::size_t size, VkBufferUsageFlags usage);
    ~Buffer();
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    VkBuffer Handle() const;
    VkDeviceAddress DeviceAddress() const;
    std::span<std::byte> Bytes();
    VkBufferUsageFlags Usage() const { return usage; }
    void Invalidate();

private:
    void initializeAddress(VkBufferUsageFlags usage);
    void release() noexcept;
    Context context;
    VkDeviceAddress deviceAddress = 0;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapping = nullptr;
    std::size_t size;
    VkDeviceSize allocationBytes = 0;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags properties;
    bool reusable = false;
    bool imported = false;
    std::shared_ptr<BufferPool> cache;
};

// The linear (non-sRGB) counterpart of a color format.
VkFormat StorageFormat(VkFormat format);

class RenderTarget {
public:
    RenderTarget(const Context& context, const ColorTarget& target, bool blending);
    ~RenderTarget();
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    VkImage Image() const;
    VkImageView View() const;
    // Shader stores through a storage view are possible (the format supports storage images).
    bool StorageCapable() const { return storageCapable; }

private:
    void release() noexcept;
    Context context;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    bool storageCapable = false;
};

// A GPU-only depth/stencil attachment image.
class DepthImage {
public:
    DepthImage(const Context& context, const DepthTarget& target);
    ~DepthImage();
    DepthImage(const DepthImage&) = delete;
    DepthImage& operator=(const DepthImage&) = delete;
    VkImage Image() const { return image; }
    VkImageView View() const { return view; }
    VkImageAspectFlags Aspects() const { return aspects; }
    const DepthTarget& Description() const { return target; }
    // Moves the image into the attachment layout (first use starts undefined); every draw
    // prepares it, so this also counts the generations that sampled copies compare against.
    void Prepare(VkCommandBuffer commands);
    // Moves the image into another layout (copies for sampling).
    void Transition(VkCommandBuffer commands, VkImageLayout newLayout);
    std::uint64_t Generation() const { return generation; }

private:
    void release() noexcept;
    Context context;
    DepthTarget target;
    std::uint64_t generation = 0;
    VkImageAspectFlags aspects = VK_IMAGE_ASPECT_DEPTH_BIT;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

class CommandBatch {
public:
    explicit CommandBatch(const Context& context);
    ~CommandBatch();
    CommandBatch(const CommandBatch&) = delete;
    CommandBatch& operator=(const CommandBatch&) = delete;
    VkCommandBuffer Handle() const;
    void SubmitAndWait();
    void Submit();
    void Wait();
    bool IsComplete();
    void Reset();

private:
    void release() noexcept;
    Context context;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool pending = false;
    bool submitted = false;
};

}

#endif

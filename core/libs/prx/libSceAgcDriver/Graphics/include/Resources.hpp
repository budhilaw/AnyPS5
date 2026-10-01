#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RESOURCES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RESOURCES_HPP

#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuTimestamps.hpp"
#include <array>
#include <chrono>
#include <span>

namespace AgcDriver::Graphics {

class Buffer {
public:
    Buffer(const Context& context, std::size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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

VkFormat StorageFormat(VkFormat format);
void ReleaseImage(const Context& context, VkImageView view, VkImage image, VkDeviceMemory memory);

class RenderTarget {
public:
    RenderTarget(const Context& context, const ColorTarget& target, bool blending);
    ~RenderTarget();
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    VkImage Image() const;
    VkImageView View() const;
    bool StorageCapable() const { return storageCapable; }

private:
    void release() noexcept;
    Context context;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    bool storageCapable = false;
};

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
    void Prepare(VkCommandBuffer commands, bool writes);
    bool Attached() const { return layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; }
    bool ReadOnly() const { return layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; }
    void Continue(bool writes) { if (writes) ++generation; }
    void Transition(VkCommandBuffer commands, VkImageLayout newLayout);
    std::uint64_t Generation() const { return generation; }
    bool Sampled() const { return sampled; }
    bool Filterable() const { return filterable; }
    VkImageView SampledView(bool stencil, bool arrayed, VkComponentMapping components);

private:
    struct CachedView {
        bool stencil = false;
        bool arrayed = false;
        VkComponentMapping components{};
        VkImageView view = VK_NULL_HANDLE;
    };
    void release() noexcept;
    Context context;
    DepthTarget target;
    std::uint64_t generation = 0;
    VkImageAspectFlags aspects = VK_IMAGE_ASPECT_DEPTH_BIT;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    bool sampled = false;
    bool filterable = false;
    std::array<CachedView, 4> sampledViews{};
    std::uint32_t sampledViewCount = 0;
};

class CommandBatch {
public:
    explicit CommandBatch(const Context& context, const char* name = nullptr);
    ~CommandBatch();
    CommandBatch(const CommandBatch&) = delete;
    CommandBatch& operator=(const CommandBatch&) = delete;
    VkCommandBuffer Handle() const;
    void SubmitAndWait();
    void Submit();
    void Wait();
    bool IsComplete();
    void Reset();
    void BeginRegion(GpuWork work, std::uint64_t address, std::array<std::uint32_t, 3> size) { timestamps.BeginRegion(commands, work, address, size); }
    void EndRegion() { timestamps.EndRegion(commands); }
    std::span<const GpuRegion> Regions() const { return timestamps.Regions(); }
    std::uint32_t DroppedRegions() const { return timestamps.Dropped(); }

private:
    void release() noexcept;
    void readTimestamps();
    Context context;
    const char* name;
    GpuTimestamps timestamps;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool pending = false;
    bool submitted = false;
public:
    std::chrono::nanoseconds submittedAt{};
    std::chrono::nanoseconds completedAt{};
    std::chrono::nanoseconds gpuTime{};
private:
};

}

#endif

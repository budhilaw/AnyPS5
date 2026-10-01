#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DISPATCHBINDINGS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DISPATCHBINDINGS_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

enum class CaptureBufferSource : std::uint32_t { Guest, Zero, Data, Gds, Table, Fault };

struct DispatchBindings {
    struct BoundRegion {
        std::uint64_t begin = 0;
        std::uint64_t end = 0;
        std::uint64_t padding = 0;
        bool writable = false;
        bool imported = false;
        bool immutable = false;
        VkMemoryPropertyFlags memory = 0;
        std::span<const std::byte> bytes;
        std::shared_ptr<const void> owner;
    };
    struct BoundBuffer {
        std::uint32_t binding = 0;
        std::uint32_t element = 0;
        CaptureBufferSource source = CaptureBufferSource::Zero;
        std::uint64_t address = 0;
        std::uint64_t size = 0;
        VkMemoryPropertyFlags memory = 0;
        std::span<const std::byte> bytes;
    };
    struct BoundImage {
        std::uint32_t binding = 0;
        std::uint32_t element = 0;
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        VkImageView view = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };
    struct BoundSampler {
        std::uint32_t binding = 0;
        std::uint32_t element = 0;
        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    };
    std::vector<VkDescriptorSetLayoutBinding> layout;
    std::vector<BoundRegion> regions;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
    std::vector<BoundBuffer> buffers;
    std::vector<BoundImage> images;
    std::vector<BoundSampler> samplers;
};

}

#endif

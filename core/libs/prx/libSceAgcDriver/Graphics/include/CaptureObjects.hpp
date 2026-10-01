#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_CAPTUREOBJECTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_CAPTUREOBJECTS_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <optional>

namespace AgcDriver::Graphics {

struct CapturedImage {
    VkImageType type = VK_IMAGE_TYPE_2D;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent{};
    std::uint32_t levels = 1;
    std::uint32_t layers = 1;
    VkImageCreateFlags flags = 0;
    VkImageUsageFlags usage = 0;
    bool copyable = false;
};

struct CapturedView {
    VkImage image = VK_NULL_HANDLE;
    VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkComponentMapping components{};
    VkImageSubresourceRange range{};
    float minLod = 0.0f;
    VkImageUsageFlags usage = 0;
};

class CaptureObjects {
public:
    static PFN_vkGetDeviceProcAddr Wrap(PFN_vkGetDeviceProcAddr resolver);
    static std::optional<CapturedImage> Image(VkImage image);
    static std::optional<CapturedView> View(VkImageView view);
};

}

#endif

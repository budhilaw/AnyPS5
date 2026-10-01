#include "prx/libSceAgcDriver/Graphics/include/CaptureObjects.hpp"
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace AgcDriver::Graphics {

namespace {

struct DeviceFunctions {
    PFN_vkCreateImage createImage = nullptr;
    PFN_vkDestroyImage destroyImage = nullptr;
    PFN_vkCreateImageView createImageView = nullptr;
    PFN_vkDestroyImageView destroyImageView = nullptr;
};

struct Registry {
    std::mutex mutex;
    PFN_vkGetDeviceProcAddr resolver = nullptr;
    std::unordered_map<VkDevice, DeviceFunctions> devices;
    std::unordered_map<VkImage, CapturedImage> images;
    std::unordered_map<VkImageView, CapturedView> views;
};

Registry& registry() {
    static auto* instance = new Registry();
    return *instance;
}

DeviceFunctions functionsOf(VkDevice device) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.devices.find(device);
    return found != state.devices.end() ? found->second : DeviceFunctions{};
}

VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice device, const VkImageCreateInfo* info, const VkAllocationCallbacks* allocator, VkImage* image) {
    const auto real = functionsOf(device).createImage;
    if (real == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    auto extended = *info;
    extended.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    auto usage = extended.usage;
    auto result = real(device, &extended, allocator, image);
    if (result != VK_SUCCESS && extended.usage != info->usage) {
        usage = info->usage;
        result = real(device, info, allocator, image);
    }
    if (result == VK_SUCCESS) {
        auto& state = registry();
        std::lock_guard lock(state.mutex);
        state.images[*image] = {info->imageType, info->format, info->extent, info->mipLevels, info->arrayLayers, info->flags, usage, (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0};
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* allocator) {
    {
        auto& state = registry();
        std::lock_guard lock(state.mutex);
        state.images.erase(image);
    }
    if (const auto real = functionsOf(device).destroyImage; real != nullptr) real(device, image, allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL createImageView(VkDevice device, const VkImageViewCreateInfo* info, const VkAllocationCallbacks* allocator, VkImageView* view) {
    const auto real = functionsOf(device).createImageView;
    if (real == nullptr) return VK_ERROR_INITIALIZATION_FAILED;
    const auto result = real(device, info, allocator, view);
    if (result == VK_SUCCESS) {
        CapturedView captured{info->image, info->viewType, info->format, info->components, info->subresourceRange, 0.0f, 0};
        for (auto* next = static_cast<const VkBaseInStructure*>(info->pNext); next != nullptr; next = next->pNext) {
            if (next->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO) captured.usage = reinterpret_cast<const VkImageViewUsageCreateInfo*>(next)->usage;
            else if (next->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT) captured.minLod = reinterpret_cast<const VkImageViewMinLodCreateInfoEXT*>(next)->minLod;
        }
        auto& state = registry();
        std::lock_guard lock(state.mutex);
        state.views[*view] = captured;
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL destroyImageView(VkDevice device, VkImageView view, const VkAllocationCallbacks* allocator) {
    {
        auto& state = registry();
        std::lock_guard lock(state.mutex);
        state.views.erase(view);
    }
    if (const auto real = functionsOf(device).destroyImageView; real != nullptr) real(device, view, allocator);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL resolve(VkDevice device, const char* name) {
    auto& state = registry();
    PFN_vkGetDeviceProcAddr real = nullptr;
    {
        std::lock_guard lock(state.mutex);
        real = state.resolver;
    }
    if (real == nullptr) return nullptr;
    const auto function = real(device, name);
    if (function == nullptr || device == VK_NULL_HANDLE || name == nullptr) return function;
    std::lock_guard lock(state.mutex);
    auto& functions = state.devices[device];
    if (std::strcmp(name, "vkCreateImage") == 0) {
        functions.createImage = reinterpret_cast<PFN_vkCreateImage>(function);
        return reinterpret_cast<PFN_vkVoidFunction>(&createImage);
    }
    if (std::strcmp(name, "vkDestroyImage") == 0) {
        functions.destroyImage = reinterpret_cast<PFN_vkDestroyImage>(function);
        return reinterpret_cast<PFN_vkVoidFunction>(&destroyImage);
    }
    if (std::strcmp(name, "vkCreateImageView") == 0) {
        functions.createImageView = reinterpret_cast<PFN_vkCreateImageView>(function);
        return reinterpret_cast<PFN_vkVoidFunction>(&createImageView);
    }
    if (std::strcmp(name, "vkDestroyImageView") == 0) {
        functions.destroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(function);
        return reinterpret_cast<PFN_vkVoidFunction>(&destroyImageView);
    }
    return function;
}

}

PFN_vkGetDeviceProcAddr CaptureObjects::Wrap(PFN_vkGetDeviceProcAddr resolver) {
    if (resolver == nullptr || resolver == &resolve) return resolver;
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    state.resolver = resolver;
    return &resolve;
}

std::optional<CapturedImage> CaptureObjects::Image(VkImage image) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.images.find(image);
    if (found == state.images.end()) return std::nullopt;
    return found->second;
}

std::optional<CapturedView> CaptureObjects::View(VkImageView view) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.views.find(view);
    if (found == state.views.end()) return std::nullopt;
    return found->second;
}

}

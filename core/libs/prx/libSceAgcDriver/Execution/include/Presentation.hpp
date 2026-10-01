#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTATION_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <span>
#include <memory>
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"

namespace AgcDriver {

class FrameTiming;

enum class PresentModeRequest : std::uint8_t { Fifo, Mailbox, Immediate, Relaxed };

struct PresentationWindow {
    void* context;
    std::span<const char* const> extensions;
    VkSurfaceKHR (*createSurface)(void* context, VkInstance instance);
    void (*getDrawableSize)(void* context, std::uint32_t* width, std::uint32_t* height);
    std::uint32_t width;
    std::uint32_t height;
    std::shared_ptr<FrameTiming> timing;
    PresentModeRequest presentMode = PresentModeRequest::Fifo;
    VkPresentModeKHR* chosenPresentMode = nullptr;
};

inline const char* PresentModeRequestName(PresentModeRequest request) {
    switch (request) {
        case PresentModeRequest::Mailbox: return "mailbox";
        case PresentModeRequest::Immediate: return "immediate";
        case PresentModeRequest::Relaxed: return "relaxed";
        default: return "fifo";
    }
}

inline VkPresentModeKHR ChoosePresentMode(PresentModeRequest request, std::span<const VkPresentModeKHR> available) {
    const auto offered = [&](VkPresentModeKHR mode) { return std::find(available.begin(), available.end(), mode) != available.end(); };
    if (request == PresentModeRequest::Immediate && offered(VK_PRESENT_MODE_IMMEDIATE_KHR)) return VK_PRESENT_MODE_IMMEDIATE_KHR;
    if ((request == PresentModeRequest::Immediate || request == PresentModeRequest::Mailbox) && offered(VK_PRESENT_MODE_MAILBOX_KHR)) return VK_PRESENT_MODE_MAILBOX_KHR;
    if (request == PresentModeRequest::Relaxed && offered(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    return VK_PRESENT_MODE_FIFO_KHR;
}

inline std::uint32_t SwapchainImageCount(std::uint32_t minImageCount, std::uint32_t maxImageCount, VkPresentModeKHR mode) {
    const bool queued = mode == VK_PRESENT_MODE_FIFO_KHR || mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    const auto count = std::max(queued ? minImageCount + 1 : minImageCount, std::uint32_t{3});
    return maxImageCount != 0 ? std::min(count, maxImageCount) : count;
}

}

extern "C" void AgcDriverPresentClear_nid_postfix(const AgcDriver::PresentationWindow& window, bool opaque, void (*gpuReady)(void*), void* context);
extern "C" void AgcDriverPresentBuffer_nid_postfix(const AgcDriver::PresentationWindow& window, const AgcDriver::DisplayBuffer& buffer, void (*gpuReady)(void*), void* context);
extern "C" void AgcDriverReleaseWindow_nid_postfix(void* window);
extern "C" void AgcDriverReportFailure_nid_postfix(std::exception_ptr error);

#endif

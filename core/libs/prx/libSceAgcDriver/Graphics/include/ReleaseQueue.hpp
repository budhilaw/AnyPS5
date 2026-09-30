#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RELEASEQUEUE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RELEASEQUEUE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace AgcDriver::Graphics {

class ReleaseQueue {
public:
    explicit ReleaseQueue(const Context& context);
    ~ReleaseQueue();
    ReleaseQueue(const ReleaseQueue&) = delete;
    ReleaseQueue& operator=(const ReleaseQueue&) = delete;
    void Defer(std::function<void()> release);
    void Collect();
    void Close();

private:
    VkDevice device;
    VkQueue queue;
    PFN_vkQueueSubmit submit;
    PFN_vkGetFenceStatus fenceStatus;
    PFN_vkResetFences resetFences;
    PFN_vkDestroyFence destroyFence;
    VkFence fence = VK_NULL_HANDLE;
    std::mutex mutex;
    std::vector<std::function<void()>> waiting;
    std::vector<std::function<void()>> covered;
    bool marked = false;
    bool closed = false;
};

void Release(const std::shared_ptr<ReleaseQueue>& queue, std::function<void()> release);

}

#endif

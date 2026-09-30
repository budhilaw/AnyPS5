#include "prx/libSceAgcDriver/Graphics/include/ReleaseQueue.hpp"
#include <iterator>
#include <utility>

namespace AgcDriver::Graphics {

ReleaseQueue::ReleaseQueue(const Context& context) : device(context.device), queue(context.queue), submit(context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")), fenceStatus(context.Function<PFN_vkGetFenceStatus>("vkGetFenceStatus")), resetFences(context.Function<PFN_vkResetFences>("vkResetFences")), destroyFence(context.Function<PFN_vkDestroyFence>("vkDestroyFence")) {
    VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    Check(context.Function<PFN_vkCreateFence>("vkCreateFence")(device, &info, nullptr, &fence), "vkCreateFence release queue");
}

ReleaseQueue::~ReleaseQueue() {
    Close();
}

void ReleaseQueue::Defer(std::function<void()> release) {
    {
        std::lock_guard lock(mutex);
        if (!closed) {
            waiting.push_back(std::move(release));
            return;
        }
    }
    release();
}

void ReleaseQueue::Collect() {
    std::vector<std::function<void()>> finished;
    {
        std::lock_guard lock(mutex);
        if (closed) return;
        if (marked) {
            const auto status = fenceStatus(device, fence);
            if (status == VK_NOT_READY) return;
            Check(status, "vkGetFenceStatus release queue");
            Check(resetFences(device, 1, &fence), "vkResetFences release queue");
            finished.swap(covered);
            marked = false;
        }
        if (!waiting.empty()) {
            Check(submit(queue, 0, nullptr, fence), "vkQueueSubmit release queue");
            covered.swap(waiting);
            marked = true;
        }
    }
    for (auto& release : finished) release();
}

void ReleaseQueue::Close() {
    std::vector<std::function<void()>> finished;
    {
        std::lock_guard lock(mutex);
        if (closed) return;
        closed = true;
        finished.swap(covered);
        finished.insert(finished.end(), std::make_move_iterator(waiting.begin()), std::make_move_iterator(waiting.end()));
        waiting.clear();
        marked = false;
    }
    for (auto& release : finished) release();
    destroyFence(device, fence, nullptr);
    fence = VK_NULL_HANDLE;
}

void Release(const std::shared_ptr<ReleaseQueue>& queue, std::function<void()> release) {
    if (queue) queue->Defer(std::move(release));
    else release();
}

}

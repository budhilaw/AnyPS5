#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"

namespace AgcDriver::Graphics {

void TextureDetiler::BeginBatch() {
    if (currentPool != VK_NULL_HANDLE) fullPools.push_back(currentPool);
    currentPool = VK_NULL_HANDLE;
    for (const auto pool : fullPools) {
        Check(context.Function<PFN_vkResetDescriptorPool>("vkResetDescriptorPool")(context.device, pool, 0), "vkResetDescriptorPool texture detiler");
        freePools.push_back(pool);
    }
    fullPools.clear();
}

void TextureDetiler::Retire(DrawQueue& queue) {
    if (fullPools.empty()) return;
    queue.EnqueueCompletion([this, pools = std::move(fullPools)] {
        for (const auto pool : pools) {
            Check(context.Function<PFN_vkResetDescriptorPool>("vkResetDescriptorPool")(context.device, pool, 0), "vkResetDescriptorPool texture detiler");
            freePools.push_back(pool);
        }
    });
    fullPools.clear();
}

VkDescriptorSet TextureDetiler::allocateSet() {
    constexpr std::uint32_t setsPerPool = 64;
    if (currentPool == VK_NULL_HANDLE || currentSets == setsPerPool) {
        if (currentPool != VK_NULL_HANDLE) fullPools.push_back(currentPool);
        if (!freePools.empty()) {
            currentPool = freePools.back();
            freePools.pop_back();
        } else {
            const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, setsPerPool * 2};
            VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            info.maxSets = setsPerPool;
            info.poolSizeCount = 1;
            info.pPoolSizes = &size;
            descriptorPools.reserve(descriptorPools.size() + 1);
            VkDescriptorPool pool = VK_NULL_HANDLE;
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &info, nullptr, &pool), "vkCreateDescriptorPool texture detiler");
            descriptorPools.push_back(pool);
            currentPool = pool;
        }
        currentSets = 0;
    }
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = currentPool;
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &descriptorLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets texture detiler");
    ++currentSets;
    return set;
}

}

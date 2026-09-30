#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DESCRIPTORCACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DESCRIPTORCACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

namespace AgcDriver::Graphics {

class DescriptorCache {
public:
    static constexpr std::uint32_t PoolSets = 128;
    struct Entry {
        std::vector<std::uint32_t> key;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        std::vector<VkDescriptorPoolSize> sizes;
        std::vector<VkDescriptorPool> pools;
        std::uint32_t poolSets = 0;
        std::vector<VkDescriptorSet> available;
    };
    struct Allocation {
        Entry* entry = nullptr;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
    };
    DescriptorCache() = default;
    ~DescriptorCache();
    DescriptorCache(const DescriptorCache&) = delete;
    DescriptorCache& operator=(const DescriptorCache&) = delete;
    Allocation Take(const Context& context, std::span<const std::uint32_t> key, std::span<const VkDescriptorSetLayoutBinding> bindings, std::span<const VkDescriptorPoolSize> sizes);
    void Put(const Allocation& allocation) noexcept;
    std::size_t Layouts() const;

private:
    void bindDevice(const Context& context);
    Entry& find(const Context& context, std::span<const std::uint32_t> key, std::span<const VkDescriptorSetLayoutBinding> bindings, std::span<const VkDescriptorPoolSize> sizes);
    void addPool(Entry& entry);
    mutable std::mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkCreateDescriptorPool createPool = nullptr;
    PFN_vkAllocateDescriptorSets allocateSets = nullptr;
    PFN_vkDestroyDescriptorPool destroyPool = nullptr;
    PFN_vkDestroyDescriptorSetLayout destroyLayout = nullptr;
    std::shared_ptr<ReleaseQueue> releases;
    std::unordered_multimap<std::uint64_t, std::unique_ptr<Entry>> entries;
};

}

#endif

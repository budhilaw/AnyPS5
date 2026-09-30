#include "prx/libSceAgcDriver/Graphics/include/DescriptorCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/KeyHash.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReleaseQueue.hpp"
#include <algorithm>
#include <limits>
#include <utility>

namespace AgcDriver::Graphics {
namespace {

std::uint64_t hashKey(std::span<const std::uint32_t> key) {
    auto hash = MixKeyHash(KeyHashSeed, key.size());
    for (const auto word : key) hash = MixKeyHash(hash, word);
    return FinishKeyHash(hash);
}

}

DescriptorCache::~DescriptorCache() {
    for (auto& [hash, entry] : entries) {
        Release(releases, [device = device, destroyPool = destroyPool, destroyLayout = destroyLayout, pools = std::move(entry->pools), layout = entry->layout] {
            for (const auto pool : pools) destroyPool(device, pool, nullptr);
            if (layout) destroyLayout(device, layout, nullptr);
        });
    }
}

void DescriptorCache::bindDevice(const Context& context) {
    if (destroyLayout != nullptr) return;
    const auto create = context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool");
    const auto allocate = context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets");
    const auto destroy = context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool");
    const auto destroyLayouts = context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout");
    device = context.device;
    createPool = create;
    allocateSets = allocate;
    destroyPool = destroy;
    releases = context.releaseQueue;
    destroyLayout = destroyLayouts;
}

DescriptorCache::Allocation DescriptorCache::Take(const Context& context, std::span<const std::uint32_t> key, std::span<const VkDescriptorSetLayoutBinding> bindings, std::span<const VkDescriptorPoolSize> sizes) {
    std::lock_guard lock(mutex);
    bindDevice(context);
    auto& entry = find(context, key, bindings, sizes);
    if (entry.sizes.empty()) return {&entry, entry.layout, VK_NULL_HANDLE};
    if (!entry.available.empty()) {
        const auto set = entry.available.back();
        entry.available.pop_back();
        return {&entry, entry.layout, set};
    }
    if (entry.pools.empty() || entry.poolSets == PoolSets) addPool(entry);
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = entry.pools.back();
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &entry.layout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    Check(allocateSets(device, &allocation, &set), "vkAllocateDescriptorSets");
    ++entry.poolSets;
    return {&entry, entry.layout, set};
}

void DescriptorCache::Put(const Allocation& allocation) noexcept {
    if (allocation.entry == nullptr || allocation.set == VK_NULL_HANDLE) return;
    std::lock_guard lock(mutex);
    allocation.entry->available.push_back(allocation.set);
}

std::size_t DescriptorCache::Layouts() const {
    std::lock_guard lock(mutex);
    return entries.size();
}

DescriptorCache::Entry& DescriptorCache::find(const Context& context, std::span<const std::uint32_t> key, std::span<const VkDescriptorSetLayoutBinding> bindings, std::span<const VkDescriptorPoolSize> sizes) {
    const auto hash = hashKey(key);
    const auto [first, last] = entries.equal_range(hash);
    for (auto it = first; it != last; ++it) {
        if (std::equal(it->second->key.begin(), it->second->key.end(), key.begin(), key.end())) return *it->second;
    }
    Require(bindings.empty() == sizes.empty(), "descriptor pool sizes disagree with the set layout bindings");
    auto entry = std::make_unique<Entry>();
    entry->key.assign(key.begin(), key.end());
    entry->sizes.assign(sizes.begin(), sizes.end());
    for (auto& size : entry->sizes) {
        Require(size.descriptorCount != 0 && size.descriptorCount <= std::numeric_limits<std::uint32_t>::max() / PoolSets, "descriptor pool size overflows a pool of sets");
        size.descriptorCount *= PoolSets;
    }
    VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.bindingCount = static_cast<std::uint32_t>(bindings.size());
    info.pBindings = bindings.data();
    Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(device, &info, nullptr, &entry->layout), "vkCreateDescriptorSetLayout");
    const auto layout = entry->layout;
    try {
        return *entries.emplace(hash, std::move(entry))->second;
    } catch (...) {
        destroyLayout(device, layout, nullptr);
        throw;
    }
}

void DescriptorCache::addPool(Entry& entry) {
    VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    info.maxSets = PoolSets;
    info.poolSizeCount = static_cast<std::uint32_t>(entry.sizes.size());
    info.pPoolSizes = entry.sizes.data();
    VkDescriptorPool pool = VK_NULL_HANDLE;
    Check(createPool(device, &info, nullptr, &pool), "vkCreateDescriptorPool");
    try {
        entry.available.reserve((entry.pools.size() + 1) * PoolSets);
        entry.pools.push_back(pool);
    } catch (...) {
        destroyPool(device, pool, nullptr);
        throw;
    }
    entry.poolSets = 0;
}

}

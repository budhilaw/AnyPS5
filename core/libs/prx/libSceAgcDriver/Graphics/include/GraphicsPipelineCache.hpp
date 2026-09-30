#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GRAPHICSPIPELINECACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GRAPHICSPIPELINECACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/LruCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace AgcDriver::Graphics {

class GraphicsPipelineCache {
public:
    static constexpr std::size_t MaxExtraTargets = 7;
    explicit GraphicsPipelineCache(const Context& context) : context(context) {}
    std::shared_ptr<Pipeline> Get(const State& state, const std::shared_ptr<ResidentColor>& target, std::span<const std::shared_ptr<ResidentColor>> extraTargets, const std::shared_ptr<DepthImage>& depth, const ShaderResources& resources, std::span<const CompiledShader> shaders);
    static std::uint64_t Key(const Context& context, const State& state, VkImageView target, std::span<const VkImageView> extraTargets, VkImageView depth, std::span<const std::uint32_t> layoutKey, std::span<const CompiledShader> shaders, std::vector<std::uint64_t>& words);

private:
    static constexpr std::size_t PipelineLimit = 4096;
    struct Entry {
        std::vector<std::uint64_t> key;
        std::shared_ptr<ResidentColor> target;
        std::vector<std::shared_ptr<ResidentColor>> extraTargets;
        std::shared_ptr<DepthImage> depth;
        std::shared_ptr<Pipeline> pipeline;
    };
    using Entries = LruCache<std::uint64_t, Entry>;
    Context context;
    Entries entries{PipelineLimit};
    std::vector<std::uint64_t> key;
    Entries::Handle memo{};
    bool memoized = false;
};

}

#endif

#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GRAPHICSPIPELINECACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GRAPHICSPIPELINECACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/LruCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include <vector>
#include <string>

namespace AgcDriver::Graphics {

class GraphicsPipelineCache {
public:
    explicit GraphicsPipelineCache(const Context& context) : context(context) {}
    std::shared_ptr<Pipeline> Get(const State& state, const std::shared_ptr<ResidentColor>& target, std::span<const std::shared_ptr<ResidentColor>> extraTargets, const std::shared_ptr<DepthImage>& depth, const ShaderResources& resources, std::span<const CompiledShader> shaders);

private:
    static constexpr std::size_t PipelineLimit = 4096;
    struct Entry {
        std::shared_ptr<ResidentColor> target;
        std::vector<std::shared_ptr<ResidentColor>> extraTargets;
        std::shared_ptr<DepthImage> depth;
        std::shared_ptr<Pipeline> pipeline;
    };
    Context context;
    LruCache<std::string, Entry> entries{PipelineLimit};
};

}

#endif

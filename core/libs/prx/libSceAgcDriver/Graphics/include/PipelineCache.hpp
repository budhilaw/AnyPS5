#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINECACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINECACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <mutex>
#include <string>

namespace AgcDriver::Graphics {

// `name` in the directory of the persisted pipeline cache (empty when persistence is disabled).
std::string CacheFilePath(const char* name);

// The device pipeline cache, persisted next to the executable (or at ANYPS5_PIPELINE_CACHE) so
// the host's shader compiles survive a restart. MoltenVK keeps its SPIR-V to MSL conversions in
// it; the Metal compiler keeps its own cache of the resulting source.
class PipelineCache {
public:
    explicit PipelineCache(const Context& context);
    ~PipelineCache();
    PipelineCache(const PipelineCache&) = delete;
    PipelineCache& operator=(const PipelineCache&) = delete;
    VkPipelineCache Handle() const { return cache; }
    // Writes the cache data to its file (no-op when persistence is disabled).
    void Save();

private:
    Context context;
    VkPipelineCache cache = VK_NULL_HANDLE;
    std::string path;
    std::mutex saving;
};

}

#endif

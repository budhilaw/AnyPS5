#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINECACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINECACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <mutex>
#include <string>

namespace AgcDriver::Graphics {

std::string CacheFilePath(const char* name);

class PipelineCache {
public:
    explicit PipelineCache(const Context& context);
    ~PipelineCache();
    PipelineCache(const PipelineCache&) = delete;
    PipelineCache& operator=(const PipelineCache&) = delete;
    VkPipelineCache Handle() const { return cache; }
    void Save();

private:
    Context context;
    VkPipelineCache cache = VK_NULL_HANDLE;
    std::string path;
    std::mutex saving;
};

}

#endif

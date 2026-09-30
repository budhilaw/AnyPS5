#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINECACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINECACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace AgcDriver::Graphics {

std::string CacheFilePath(const char* name);

class PipelineCache {
public:
    explicit PipelineCache(const Context& context);
    ~PipelineCache();
    PipelineCache(const PipelineCache&) = delete;
    PipelineCache& operator=(const PipelineCache&) = delete;
    VkPipelineCache Handle() const { return cache; }
    void NoteCreated() { created.store(true, std::memory_order_relaxed); }
    void SaveIfDue();

private:
    void save();
    void runSaver();
    Context context;
    VkPipelineCache cache = VK_NULL_HANDLE;
    std::string path;
    std::atomic<bool> created{false};
    std::mutex saverMutex;
    std::condition_variable saverChanged;
    bool saveRequested = false;
    bool saverStopping = false;
    std::chrono::steady_clock::time_point lastSave = std::chrono::steady_clock::now();
    std::thread saver;
};

}

#endif

#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace AgcDriver::Graphics {

std::string CacheFilePath(const char* name) {
    if (const char* configured = std::getenv("ANYPS5_PIPELINE_CACHE")) {
        return *configured == '\0' ? std::string() : (std::filesystem::path(configured).parent_path() / name).string();
    }
    try {
#if defined(_WIN32)
        wchar_t buffer[32768];
        const auto length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
        if (length == 0 || length >= std::size(buffer)) return {};
        return (std::filesystem::path(buffer).parent_path() / name).string();
#elif defined(__APPLE__)
        const char* executable = _dyld_get_image_name(0);
        if (executable == nullptr) return {};
        return (std::filesystem::absolute(executable).parent_path() / name).string();
#else
        return (std::filesystem::read_symlink("/proc/self/exe").parent_path() / name).string();
#endif
    } catch (...) {
        return {};
    }
}

PipelineCache::PipelineCache(const Context& context) : context(context) {
    if (const char* configured = std::getenv("ANYPS5_PIPELINE_CACHE")) path = *configured == '\0' ? std::string() : configured;
    else path = CacheFilePath("pipeline.cache");
    std::vector<char> initial;
    if (!path.empty()) {
        std::ifstream file(path, std::ios::binary);
        if (file) initial.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    info.initialDataSize = initial.size();
    info.pInitialData = initial.empty() ? nullptr : initial.data();
    if (context.Function<PFN_vkCreatePipelineCache>("vkCreatePipelineCache")(context.device, &info, nullptr, &cache) != VK_SUCCESS) {
        // Stale or foreign data: start empty.
        info.initialDataSize = 0;
        info.pInitialData = nullptr;
        Check(context.Function<PFN_vkCreatePipelineCache>("vkCreatePipelineCache")(context.device, &info, nullptr, &cache), "vkCreatePipelineCache");
        initial.clear();
    }
    if (!path.empty()) APS5_LOG_OUT("pipeline cache %s: %zu bytes loaded", path.c_str(), initial.size());
}

PipelineCache::~PipelineCache() {
    try {
        Save();
    } catch (...) {
    }
    context.Function<PFN_vkDestroyPipelineCache>("vkDestroyPipelineCache")(context.device, cache, nullptr);
}

void PipelineCache::Save() {
    if (path.empty() || cache == VK_NULL_HANDLE) return;
    std::lock_guard lock(saving);
    const auto getData = context.Function<PFN_vkGetPipelineCacheData>("vkGetPipelineCacheData");
    std::size_t size = 0;
    if (getData(context.device, cache, &size, nullptr) != VK_SUCCESS || size == 0) return;
    std::vector<char> data(size);
    if (getData(context.device, cache, &size, data.data()) != VK_SUCCESS) return;
    const auto temporary = path + ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) return;
        file.write(data.data(), static_cast<std::streamsize>(size));
        if (!file) return;
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
}

}

#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERWARMUP_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERWARMUP_HPP

#include "Recompiler.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace AgcDriver {

// The shader cache key's hash of a program's code (never zero).
std::uint64_t ShaderCodeHash(std::span<const std::uint32_t> code);

// Shader requests that needed a compile, kept next to the pipeline cache: the next run replays
// them in the background, so its draws find their shaders already compiled.
class ShaderWarmup {
public:
    ShaderWarmup();
    ~ShaderWarmup();
    ShaderWarmup(const ShaderWarmup&) = delete;
    ShaderWarmup& operator=(const ShaderWarmup&) = delete;
    void Record(const ShaderRecompiler::RecompileRequest& request);
    // Ends the replay: the recompiler's caches do not outlive process shutdown.
    void Stop();

private:
    void replay(std::vector<std::string> requests) noexcept;

    std::string path;
    std::mutex mutex;
    std::unordered_set<std::size_t> known;
    bool headerWritten = false;
    std::atomic<bool> stopping{false};
    std::vector<std::thread> threads;
};

}

#endif

#include "prx/libSceAgcDriver/Execution/include/ShaderWarmup.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdlib>
#include <fstream>
#include <functional>
#if defined(__APPLE__)
#include <pthread.h>
#endif

namespace AgcDriver {

namespace {

constexpr const char* Header = "anyps5-shader-requests 1";
constexpr std::size_t MaxRequests = 4096;
constexpr unsigned ReplayThreads = 2;

}

std::uint64_t ShaderCodeHash(std::span<const std::uint32_t> code) {
    std::uint64_t hash = 0x9e3779b97f4a7c15ull ^ code.size();
    for (const auto word : code) {
        hash = (hash ^ word) * 0xff51afd7ed558ccdull;
        hash ^= hash >> 32u;
    }
    return hash != 0 ? hash : 1;
}

ShaderWarmup::ShaderWarmup() {
    static const bool disabled = std::getenv("ANYPS5_NO_SHADER_WARMUP") != nullptr;
    if (disabled) return;
    path = Graphics::CacheFilePath("shader-requests.cache");
    if (path.empty()) return;
    std::vector<std::string> requests;
    std::ifstream file(path, std::ios::binary);
    std::string line;
    if (file && std::getline(file, line) && line == Header) {
        headerWritten = true;
        while (std::getline(file, line) && requests.size() < MaxRequests) {
            if (!line.empty() && known.insert(std::hash<std::string>{}(line)).second) requests.push_back(std::move(line));
        }
    }
    if (requests.empty()) return;
    APS5_LOG_OUT("shader warmup: replaying %zu recorded shader requests", requests.size());
    std::vector<std::vector<std::string>> shares(ReplayThreads);
    for (std::size_t i = 0; i < requests.size(); ++i) shares[i % ReplayThreads].push_back(std::move(requests[i]));
    for (auto& share : shares) threads.emplace_back([this, share = std::move(share)]() mutable { replay(std::move(share)); });
}

ShaderWarmup::~ShaderWarmup() {
    Stop();
}

void ShaderWarmup::Stop() {
    stopping = true;
    for (auto& thread : threads) {
        if (thread.joinable()) thread.join();
    }
}

void ShaderWarmup::Record(const ShaderRecompiler::RecompileRequest& request) {
    if (path.empty()) return;
    std::string line;
    try {
        line = ShaderRecompiler::RequestSerializer{}.Serialize(request);
    } catch (const std::exception&) {
        return;
    }
    std::lock_guard lock(mutex);
    if (known.size() >= MaxRequests || !known.insert(std::hash<std::string>{}(line)).second) return;
    std::ofstream file(path, headerWritten ? std::ios::binary | std::ios::app : std::ios::binary | std::ios::trunc);
    if (!headerWritten) file << Header << '\n';
    headerWritten = true;
    file << line << '\n';
}

void ShaderWarmup::replay(std::vector<std::string> requests) noexcept {
#if defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif
    for (const auto& text : requests) {
        if (stopping) return;
        try {
            auto deserialized = ShaderRecompiler::RequestSerializer{}.Deserialize(text);
            auto& request = deserialized.request;
            request.shader.codeHash = ShaderCodeHash(request.shader.code);
            (void)ShaderRecompiler::Recompile(request);
        } catch (const std::exception& error) {
            APS5_LOG_ERR("shader warmup: request skipped: %s", error.what());
        } catch (...) {
        }
    }
}

}

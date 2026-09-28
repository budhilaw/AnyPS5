#include "prx/libSceAgcDriver/Graphics/include/SlowPipeline.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

namespace AgcDriver::Graphics {

void ReportSlowPipeline(const char* kind, std::chrono::steady_clock::time_point start, std::span<const CompiledShader> shaders) {
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (elapsed < 100.0) return;
    static const char* dumpDirectory = std::getenv("ANYPS5_DUMP_SLOW_SHADERS");
    std::string detail;
    for (const auto& shader : shaders) {
        if (shader.program == nullptr) continue;
        const auto& spirv = shader.program->spirv;
        std::size_t hash = 0;
        for (const auto word : spirv) hash = hash * 1099511628211ull ^ word;
        char text[128];
        std::snprintf(text, sizeof(text), " stage %u: %zu dwords (%016zx)", static_cast<unsigned>(shader.stage), spirv.size(), hash);
        detail += text;
        if (dumpDirectory != nullptr) {
            std::snprintf(text, sizeof(text), "/%016zx.spv", hash);
            if (auto* file = std::fopen((std::string(dumpDirectory) + text).c_str(), "wb")) {
                std::fwrite(spirv.data(), sizeof(std::uint32_t), spirv.size(), file);
                std::fclose(file);
            }
        }
    }
    APS5_LOG_OUT("slow %s pipeline: %.0f ms%s", kind, elapsed, detail.c_str());
}

}

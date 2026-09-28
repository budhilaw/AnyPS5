#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SLOWPIPELINE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SLOWPIPELINE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <chrono>
#include <span>

namespace AgcDriver::Graphics {

// Logs pipelines whose creation (the host's shader compile) took longer than a quarter second,
// and with ANYPS5_DUMP_SLOW_SHADERS=<directory> writes their SPIR-V there for inspection.
void ReportSlowPipeline(const char* kind, std::chrono::steady_clock::time_point start, std::span<const CompiledShader> shaders);

}

#endif

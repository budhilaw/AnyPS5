#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SLOWPIPELINE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SLOWPIPELINE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <chrono>
#include <span>

namespace AgcDriver::Graphics {

void ReportSlowPipeline(const char* kind, std::chrono::steady_clock::time_point start, std::span<const CompiledShader> shaders);

}

#endif

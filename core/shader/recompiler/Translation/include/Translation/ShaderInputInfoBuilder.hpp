#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_SHADERINPUTINFOBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_SHADERINPUTINFOBUILDER_HPP

#include "Optimization/ShaderStageInputInfo.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "Recompiler.hpp"

namespace ShaderRecompiler {

// hostSubgroupSize: the device subgroup width; wave64 workgroup shaders on a 32-wide host run two
// guest lanes per invocation (see SpirvEmitter laneCount).
ShaderStageInputInfo BuildShaderStageInputInfo(ShaderStageKind stage, const GuestContext& context, std::uint32_t hostSubgroupSize = 64);

}

#endif

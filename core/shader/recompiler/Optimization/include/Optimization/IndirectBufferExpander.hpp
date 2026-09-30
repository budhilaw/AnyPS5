#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_INDIRECTBUFFEREXPANDER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_INDIRECTBUFFEREXPANDER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>

namespace ShaderRecompiler {

class IndirectBufferExpander {
public:
    std::uint32_t Expand(IrProgram& program) const;
};

}

#endif

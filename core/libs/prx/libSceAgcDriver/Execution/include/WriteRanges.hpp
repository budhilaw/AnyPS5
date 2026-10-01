#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_WRITERANGES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_WRITERANGES_HPP

#include "Recompiler.hpp"
#include <cstdint>
#include <vector>

namespace AgcDriver {

enum class WriteOrigin : std::uint8_t { ColorTarget, BufferElement, StorageImage, AllocationFallback, Destination };

struct WriteRange {
    std::uint64_t first;
    std::uint64_t last;
    WriteOrigin origin;
};

void CollectWrites(const std::vector<ShaderRecompiler::DescriptorBinding>& bindings, std::vector<WriteRange>& writes, bool& writesUnknown);

}

#endif

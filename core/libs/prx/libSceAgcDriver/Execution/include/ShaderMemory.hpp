#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERMEMORY_HPP

#include "Recompiler.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include <map>

namespace AgcDriver {

class ShaderMemory {
public:
    explicit ShaderMemory(std::span<const ShaderRecompiler::MemoryRegion> initial);
    // Reads the memory the request's resources come from; the resources it materialized stay
    // available (Snapshot, Specialization) until the next capture.
    void Capture(const ShaderRecompiler::RecompileRequest& request);
    const ShaderRecompiler::ResourceSnapshot& Snapshot() const { return snapshot; }
    const ShaderRecompiler::ResourceSpecialization& Specialization() const { return specialization; }
    [[nodiscard]] std::vector<ShaderRecompiler::MemoryRegion> Regions() const;

private:
    static bool read(void* context, std::uint64_t address, std::uint32_t* value);
    std::map<std::uint64_t, std::vector<std::byte>> regions;
    ShaderRecompiler::ResourceSnapshot snapshot;
    ShaderRecompiler::ResourceSpecialization specialization;
};

}

#endif

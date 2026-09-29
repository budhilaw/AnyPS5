#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERMEMORY_HPP

#include "Recompiler.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include <map>
#include <memory>
#include <vector>

namespace AgcDriver {

class ShaderMemory {
public:
    explicit ShaderMemory(std::span<const ShaderRecompiler::MemoryRegion> initial, std::vector<std::shared_ptr<const void>> owners = {});
    void Capture(const ShaderRecompiler::RecompileRequest& request);
    const ShaderRecompiler::ResourceSnapshot& Snapshot() const { return snapshot; }
    const ShaderRecompiler::ResourceSpecialization& Specialization() const { return specialization; }
    const std::shared_ptr<void>& Source() const { return source; }
    [[nodiscard]] std::vector<ShaderRecompiler::MemoryRegion> Regions() const;

private:
    static bool read(void* context, std::uint64_t address, std::uint32_t* value);
    std::vector<ShaderRecompiler::MemoryRegion> initial;
    std::vector<std::shared_ptr<const void>> owners;
    std::vector<std::pair<std::uint64_t, std::uint32_t>> dwords;
    std::vector<std::uint64_t> checkedPages;
    ShaderRecompiler::ResourceSnapshot snapshot;
    ShaderRecompiler::ResourceSpecialization specialization;
    std::shared_ptr<void> source;
};

}

#endif

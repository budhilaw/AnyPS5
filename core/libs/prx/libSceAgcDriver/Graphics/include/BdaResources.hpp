#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BDARESOURCES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BDARESOURCES_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"

namespace AgcDriver::Graphics {

class BdaResources {
public:
    explicit BdaResources(const Context& context);
    BdaResources(const Context& context, const GuestBufferMemory& memory);
    VkDescriptorBufferInfo Table() const;
    VkDescriptorBufferInfo Fault() const;
    std::span<const std::byte> TableBytes() const { return table ? table->Bytes().first(tableBytes) : std::span<const std::byte>(); }
    std::span<const std::byte> FaultBytes() const { return fault->Bytes().first(sizeof(ShaderRecompiler::BdaAbi::Fault)); }
    VkMemoryPropertyFlags TableMemory() const { return table ? table->Properties() : 0u; }
    VkMemoryPropertyFlags FaultMemory() const { return fault->Properties(); }
    void CheckFault(const GuestBufferMemory& memory) const;

private:
    std::unique_ptr<Buffer> table;
    std::unique_ptr<Buffer> fault;
    std::size_t tableBytes = 0;
};

}

#endif

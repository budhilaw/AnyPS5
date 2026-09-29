#ifndef CORE_LIBS_PRX_LIBKERNEL_APR_APR_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APR_APR_HPP

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI AprCommandBufferConstruct(void* commandBuffer);
int APS5_VABI AprCommandBufferSetBuffer(void* commandBuffer, void* buffer, std::uint32_t size);
int APS5_VABI AprCommandBufferReset(void* commandBuffer);
int APS5_VABI AprCommandBufferAppendRead(void* commandBuffer, std::uint32_t fileId, void* destination, std::uint64_t size, std::uint64_t fileOffset);

}

#endif

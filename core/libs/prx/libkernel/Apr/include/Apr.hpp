#ifndef CORE_LIBS_PRX_LIBKERNEL_APR_APR_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APR_APR_HPP

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI AprCommandBufferConstruct_nid_no_patch(void* commandBuffer);
int APS5_VABI AprCommandBufferSetBuffer_nid_no_patch(void* commandBuffer, void* buffer, std::uint32_t size);
int APS5_VABI AprCommandBufferReset_nid_no_patch(void* commandBuffer);
int APS5_VABI AprCommandBufferAppendRead_nid_no_patch(void* commandBuffer, std::uint32_t fileId, void* destination, std::uint64_t size, std::uint64_t fileOffset);

}

#endif

#ifndef CORE_LIBS_PRX_LIBKERNEL_APR_APR_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APR_APR_HPP

#include <cstdint>
#include "SceTypes.hpp"

// Asynchronous page read (APR): titles resolve file paths to ids once, then queue read commands
// into an AMPR command buffer and submit it to the kernel. libSceAmpr builds the command buffers
// through this interface; libkernel owns the file-id registry and executes the submissions.
extern "C" {

// Command buffer front end. Each returns 0 or a negative SCE kernel error.
int APS5_VABI AprCommandBufferConstruct(void* commandBuffer);
int APS5_VABI AprCommandBufferSetBuffer(void* commandBuffer, void* buffer, std::uint32_t size);
int APS5_VABI AprCommandBufferReset(void* commandBuffer);
int APS5_VABI AprCommandBufferAppendRead(void* commandBuffer, std::uint32_t fileId, void* destination, std::uint64_t size, std::uint64_t fileOffset);

}

#endif

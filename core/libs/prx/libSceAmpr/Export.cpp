#include <cstdint>
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Apr/include/Apr.hpp"

// libSceAmpr: asynchronous memory/page request command buffers. The title builds a buffer of
// commands (here: APR file reads) and submits it through libkernel, which executes them.
extern "C" {

int APS5_VABI sceAmprCommandBufferConstructor(void* command_buffer) {
    return AprCommandBufferConstruct(command_buffer);
}

// The reserved state words default to the header's own trailing words; the title passes them
// explicitly (command_buffer + 0x18 and + 0x20) and nothing is kept in them.
int APS5_VABI sceAmprAprCommandBufferConstructor(void* command_buffer, void* reserved_state0, void* reserved_state1) {
    if (command_buffer == nullptr) return 0;
    if (reserved_state0 != nullptr) *static_cast<std::uint64_t*>(reserved_state0) = 0;
    if (reserved_state1 != nullptr) *static_cast<std::uint64_t*>(reserved_state1) = 0;
    return 0;
}

int APS5_VABI sceAmprCommandBufferSetBuffer(void* command_buffer, void* buffer, uint32_t size) {
    return AprCommandBufferSetBuffer(command_buffer, buffer, size);
}

int APS5_VABI sceAmprCommandBufferReset(void* command_buffer) {
    return AprCommandBufferReset(command_buffer);
}

int APS5_VABI sceAmprAprCommandBufferReadFile(void* command_buffer, uint64_t reserved_state0, uint64_t reserved_state1, uint32_t file_id, void* destination, uint64_t size, uint64_t file_offset) {
    (void)reserved_state0;
    (void)reserved_state1;
    return AprCommandBufferAppendRead(command_buffer, file_id, destination, size, file_offset);
}

}

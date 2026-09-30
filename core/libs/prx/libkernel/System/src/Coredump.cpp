#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<std::uint64_t> coredumpHandler{0};

}

extern "C" {

int APS5_VABI sceCoredumpRegisterCoredumpHandler(uint64_t handler, size_t stack_size, uint64_t context) {
 (void)stack_size;
 (void)context;
 coredumpHandler.store(handler);
 return 0;
}

int APS5_VABI sceCoredumpUnregisterCoredumpHandler(void) {
 coredumpHandler.store(0);
 return 0;
}

int APS5_VABI sceCoredumpAttachMemoryRegion(uint64_t address, size_t size, uint32_t flags) {
 (void)address;
 (void)size;
 (void)flags;
 return 0;
}

int APS5_VABI sceCoredumpAttachUserMemoryFile(uint64_t address, size_t size, uint32_t flags) {
 (void)address;
 (void)size;
 (void)flags;
 return 0;
}

int APS5_VABI sceCoredumpSetUserDataType(uint32_t type) {
 (void)type;
 return 0;
}

int64_t APS5_VABI sceCoredumpWriteUserData(const void* data, size_t size) {
 (void)data;
 return static_cast<int64_t>(size);
}

}

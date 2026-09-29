#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_DARWIN_GUESTIMAGE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_DARWIN_GUESTIMAGE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>

namespace GuestImage {

struct ProcessParameters { const void* address; std::size_t size; };
struct ExceptionHeader { const unsigned char* header; };
struct ThreadStorage { const void* image; std::size_t fileSize; std::size_t memorySize; std::size_t alignment; std::uint32_t slot; };

std::optional<ProcessParameters> FindProcessParameters();
std::optional<ExceptionHeader> FindExceptionHeader();
std::optional<ThreadStorage> FindThreadStorage();

bool IsGuestExecutable();

struct ModuleThreadStorage { std::uint64_t module; const void* image; std::size_t fileSize; std::size_t memorySize; std::size_t alignment; };
void ForEachModuleThreadStorage(void (*visit)(const ModuleThreadStorage&, void*), void* context);

void* FindModuleInit(const char* path);

}

#endif

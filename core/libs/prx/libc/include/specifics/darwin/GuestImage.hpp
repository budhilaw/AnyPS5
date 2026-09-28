#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_DARWIN_GUESTIMAGE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_DARWIN_GUESTIMAGE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>

// A guest executable relinked to Mach-O carries an __ANYPS5 segment whose sections hold
// little-endian 64-bit fields describing guest structures by unslid virtual address. These
// helpers resolve them against the loaded main image (see relinker MachOPatcher).
namespace GuestImage {

struct ProcessParameters { const void* address; std::size_t size; };
struct ExceptionHeader { const unsigned char* header; };
struct ThreadStorage { const void* image; std::size_t fileSize; std::size_t memorySize; std::size_t alignment; std::uint32_t slot; };

// Slide-adjusted lookups on the main executable. Each throws std::runtime_error when the main
// image is not a relinked guest or the section is malformed; the optional is empty when absent.
std::optional<ProcessParameters> FindProcessParameters();
std::optional<ExceptionHeader> FindExceptionHeader();
std::optional<ThreadStorage> FindThreadStorage();

// Whether the main executable was produced by the relinker (has an __ANYPS5 segment).
bool IsGuestExecutable();

// Dynamic thread storage of relinked guest modules (dylibs): the block template and the module id
// their R_X86_64_DTPMOD64 slots carry. Visits every loaded image with __modtls metadata.
struct ModuleThreadStorage { std::uint64_t module; const void* image; std::size_t fileSize; std::size_t memorySize; std::size_t alignment; };
void ForEachModuleThreadStorage(void (*visit)(const ModuleThreadStorage&, void*), void* context);

// The `_init(args, argp, start)` entry of a relinked module, by the path it was loaded from
// (matched on file name); null when the module has none or is not loaded.
void* FindModuleInit(const char* path);

}

#endif

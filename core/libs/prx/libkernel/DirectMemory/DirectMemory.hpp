#ifndef CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_DIRECTMEMORY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_DIRECTMEMORY_HPP

#include <cstdint>
#include <cstddef>

static constexpr size_t DIRECT_MEMORY_SIZE = 13824ULL * 1024 * 1024;
static constexpr size_t PS5_PAGE_SIZE = 0x4000;

static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int SCE_KERNEL_ERROR_EAGAIN = static_cast<int>(0x80020023);
static constexpr int SCE_KERNEL_ERROR_ENOMEM = static_cast<int>(0x8002000C);
static constexpr int SCE_KERNEL_ERROR_EACCES = static_cast<int>(0x8002000D);
static constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000E);

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut);
bool DirectMemoryQueryRange(int64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType);
void DirectMemoryFree(int64_t start, size_t len);
int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment);
int DoMapAnon(void** addr, size_t len, int prot, int flags);
int DoMprotect(const void* addr, size_t len, int prot);
int DoMunmap(void* addr, size_t len);
int DoReserveVirtual(void** addr, size_t len, size_t alignment);
bool FindDirectMapping(uintptr_t address, uintptr_t* start, uintptr_t* end, int64_t* physical);
uintptr_t NextDirectMapping(uintptr_t from);
uintptr_t PreviousDirectMappingEnd(uintptr_t before);

#endif
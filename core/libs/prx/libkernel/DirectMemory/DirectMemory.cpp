#if defined(__APPLE__)
#include <unistd.h>
#endif
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <string>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <system_error>

#if !defined(_WIN32)
#include <sys/mman.h>
#else
#include "prx/libc/include/specifics/windows/NativeProtection.hpp"

static constexpr int PROT_NONE = 0;
static constexpr int PROT_READ = 1;
static constexpr int PROT_WRITE = 2;
static constexpr int PROT_EXEC = 4;

static DWORD WinProtFromPosix(int prot) {
    if (prot == PROT_NONE) return PAGE_NOACCESS;
    if ((prot & PROT_EXEC) && (prot & PROT_WRITE)) return PAGE_EXECUTE_READWRITE;
    if ((prot & PROT_EXEC) && (prot & PROT_READ)) return PAGE_EXECUTE_READ;
    if (prot & PROT_EXEC) return PAGE_EXECUTE;
    if (prot & PROT_WRITE) return PAGE_READWRITE;
    return PAGE_READONLY;
}

static int mprotect(void* addr, size_t len, int prot) {
    if (!ProtectNativeRange(reinterpret_cast<std::uintptr_t>(addr), len, WinProtFromPosix(prot)))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualProtect failed");
    return 0;
}
#endif

namespace {

struct DirectMapping {
    std::uintptr_t end;
    int64_t physical;
};

std::mutex directMappingMutex;
std::map<std::uintptr_t, DirectMapping> directMappings;

void ForgetDirectMappings(std::uintptr_t start, std::size_t len) {
    const auto end = start + len;
    std::lock_guard lock(directMappingMutex);
    auto it = directMappings.lower_bound(start);
    if (it != directMappings.begin() && std::prev(it)->second.end > start) --it;
    while (it != directMappings.end() && it->first < end) {
        const auto mappingStart = it->first;
        const auto mapping = it->second;
        it = directMappings.erase(it);
        if (mappingStart < start) directMappings.emplace(mappingStart, DirectMapping{start, mapping.physical});
        if (mapping.end > end) directMappings.emplace(end, DirectMapping{mapping.end, mapping.physical + static_cast<int64_t>(end - mappingStart)});
    }
}

void RecordDirectMapping(const void* address, std::size_t len, int64_t physical) {
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    ForgetDirectMappings(start, len);
    std::lock_guard lock(directMappingMutex);
    directMappings.emplace(start, DirectMapping{start + len, physical});
}

void ValidateLength(size_t len) {
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Memory length must be a positive multiple of the guest page size");
    }
}

size_t ValidateAlignment(size_t alignment) {
    if (alignment == 0) return PS5_PAGE_SIZE;
    if (alignment < PS5_PAGE_SIZE || (alignment & (alignment - 1)) != 0) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Memory alignment must be a power of two no smaller than the guest page size");
    }
    return alignment;
}

void ValidateRange(const void* addr, size_t len, size_t alignment) {
    ValidateLength(len);
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    if (!addr || (start & (alignment - 1)) != 0 || len > std::numeric_limits<std::uintptr_t>::max() - start) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Invalid memory address, alignment or range");
    }
}

int LinuxProtFromSce(int prot) {
    if ((prot & ~0xf7) != 0) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Unsupported memory protection bits 0x" + [](int value) { char text[16]; std::snprintf(text, sizeof(text), "%x", static_cast<unsigned>(value)); return std::string(text); }(prot));
    }
    int result = PROT_NONE;
    if (prot & 1) result |= PROT_READ;
    if (prot & 2) result |= PROT_READ | PROT_WRITE;
    if (prot & 4) result |= PROT_EXEC;
    return result;
}

void Unmap(void* addr, size_t len) {
    GuestMemoryBacking::GuestMemoryBackingUnmap_nid_postfix(addr, len);
}

void* MapAligned(void* addr, size_t len, int prot, int flags, size_t alignment, int64_t physical = -1) {
    ValidateLength(len);
    alignment = ValidateAlignment(alignment);
    constexpr int guestMapFixed = 0x10;
    constexpr int guestMapNoOverwrite = 0x80;
    constexpr int guestMapNoCoalesce = 0x400000;
    if ((flags & ~(guestMapFixed | guestMapNoOverwrite | guestMapNoCoalesce)) != 0) throw std::invalid_argument("Unsupported memory mapping flags " + std::to_string(flags));
    if ((flags & guestMapFixed) != 0) ValidateRange(addr, len, alignment);
    else if (addr != nullptr) throw std::invalid_argument("Non-fixed mapping address hints are not implemented");
    if (physical >= 0) return GuestMemoryBacking::GuestMemoryBackingMapPhysical_nid_postfix(addr, len, alignment, prot, static_cast<std::uint64_t>(physical));
    return GuestMemoryBacking::GuestMemoryBackingMap_nid_postfix(addr, len, alignment, prot);
}

bool CommitReserved(GuestAllocations::Mutation& mutation, void* addr, size_t len, int prot, int flags, size_t alignment) {
    constexpr int guestMapFixed = 0x10;
    if ((flags & guestMapFixed) == 0 || !mutation.IsReserved(addr, len)) return false;
    ValidateRange(addr, len, ValidateAlignment(alignment));
    const int hostProt = LinuxProtFromSce(prot);
    mutation.Protect(addr, len, (prot & 3) != 0, (prot & 2) != 0, [&] {
        GuestMemoryBacking::GuestMemoryBackingActivate_nid_postfix(reinterpret_cast<std::uintptr_t>(addr), len, hostProt);
    });
    return true;
}

void ValidateOutput(void** addr) {
    if (!addr) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Null memory mapping output");
    }
}

}

static_assert(DIRECT_MEMORY_SIZE == GuestMemoryBacking::GuestPhysicalMemoryBytes);

int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    if (physStart < 0 || (static_cast<std::uint64_t>(physStart) & (PS5_PAGE_SIZE - 1)) != 0 || static_cast<std::uint64_t>(physStart) >= DIRECT_MEMORY_SIZE || len > DIRECT_MEMORY_SIZE - static_cast<std::uint64_t>(physStart)) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    GuestAllocations::Mutation mutation;
    if (*addr != nullptr && CommitReserved(mutation, *addr, len, prot, flags, alignment)) {
        RecordDirectMapping(*addr, len, physStart);
        return 0;
    }
    if (*addr != nullptr) mutation.RequireAvailable(*addr, len);
    void* mapped = nullptr;
    try {
        mapped = MapAligned(*addr, len, LinuxProtFromSce(prot), flags, alignment, physStart);
    } catch (const std::runtime_error& error) {
        constexpr int guestMapNoOverwrite = 0x80;
        if (*addr == nullptr || (flags & guestMapNoOverwrite) == 0) throw;
        APS5_LOG_OUT("fixed direct mapping %p+0x%zx is unavailable on this host: %s", *addr, len, error.what());
        return SCE_KERNEL_ERROR_ENOMEM;
    }
    try {
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    RecordDirectMapping(mapped, len, physStart);
    *addr = mapped;
    return 0;
}

int DoMapAnon(void** addr, size_t len, int prot, int flags) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    if (*addr != nullptr && CommitReserved(mutation, *addr, len, prot, flags, PS5_PAGE_SIZE)) return 0;
    if (*addr != nullptr) mutation.RequireAvailable(*addr, len);
    void* mapped = MapAligned(*addr, len, LinuxProtFromSce(prot), flags, PS5_PAGE_SIZE);
    try {
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    return 0;
}

int DoMprotect(const void* addr, size_t len, int prot) {
    const auto address = reinterpret_cast<std::uintptr_t>(addr);
#if defined(__APPLE__)
    const auto pageMask = static_cast<std::uintptr_t>(sysconf(_SC_PAGESIZE) - 1);
#else
    constexpr auto pageMask = static_cast<std::uintptr_t>(PS5_PAGE_SIZE - 1);
#endif
    const auto limit = std::numeric_limits<std::uintptr_t>::max();
    if (address == 0 || len == 0 || len > limit - address || address + len > limit - pageMask) throw std::invalid_argument("Invalid guest memory protection range");
    const auto first = address & ~pageMask;
    const auto end = (address + len + pageMask) & ~pageMask;
    const auto bytes = static_cast<std::size_t>(end - first);
    const auto* pointer = reinterpret_cast<const void*>(first);
    const auto nativeProtection = LinuxProtFromSce(prot);
    GuestAllocations::Mutation mutation;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(pointer, &memory, sizeof(memory)) != sizeof(memory)) throw std::runtime_error("Cannot query guest memory protection range");
    if (memory.Type == MEM_IMAGE) {
        if (memory.AllocationBase != GetModuleHandleW(nullptr)) throw std::invalid_argument("Memory protection of a foreign image is not supported");
        mutation.RegisterMainImage();
    }
#elif defined(__APPLE__)
    mutation.RegisterMainImage();
#endif
    mutation.Protect(pointer, bytes, (prot & 3) != 0, (prot & 2) != 0, [&] {
        if (mprotect(const_cast<void*>(pointer), bytes, nativeProtection) != 0) throw std::system_error(errno, std::generic_category(), "mprotect failed");
    });
    return 0;
}

int DoMunmap(void* addr, size_t len) {
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0 || !addr) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    const auto end = reinterpret_cast<std::uintptr_t>(addr) + len;
    auto cursor = reinterpret_cast<std::uintptr_t>(addr);
    while (cursor < end) {
        GuestAllocations::Range range{};
        if (!mutation.Query(cursor, true, range) || range.address >= end) break;
        const auto first = std::max<std::uintptr_t>(cursor, range.address);
        const auto last = std::min<std::uintptr_t>(end, range.address + range.bytes);
        auto* piece = reinterpret_cast<void*>(first);
        mutation.Unmap(piece, last - first, [&](const void*, bool) {
            Unmap(piece, last - first);
        });
        ForgetDirectMappings(first, last - first);
        cursor = last;
    }
    return 0;
}

bool FindDirectMapping(uintptr_t address, uintptr_t* start, uintptr_t* end, int64_t* physical) {
    std::lock_guard lock(directMappingMutex);
    auto it = directMappings.upper_bound(address);
    if (it == directMappings.begin()) return false;
    --it;
    if (address >= it->second.end) return false;
    *start = it->first;
    *end = it->second.end;
    *physical = it->second.physical;
    return true;
}

uintptr_t NextDirectMapping(uintptr_t from) {
    std::lock_guard lock(directMappingMutex);
    const auto it = directMappings.upper_bound(from);
    return it == directMappings.end() ? std::numeric_limits<uintptr_t>::max() : it->first;
}

uintptr_t PreviousDirectMappingEnd(uintptr_t before) {
    std::lock_guard lock(directMappingMutex);
    auto it = directMappings.upper_bound(before);
    if (it == directMappings.begin()) return 0;
    --it;
    return it->second.end <= before ? it->second.end : 0;
}

int DoReserveVirtual(void** addr, size_t len, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    void* mapped = MapAligned(nullptr, len, PROT_NONE, 0, alignment);
    try {
        mutation.Add(mapped, len, false, false);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    return 0;
}

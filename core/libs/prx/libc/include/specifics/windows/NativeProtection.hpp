#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SPECIFICS_WINDOWS_NATIVEPROTECTION_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SPECIFICS_WINDOWS_NATIVEPROTECTION_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

inline bool ProtectNativeRange(std::uint64_t address, std::size_t bytes, DWORD protection) {
    DWORD previous = 0;
    if (VirtualProtect(reinterpret_cast<void*>(address), bytes, protection, &previous) != FALSE) return true;
    if (GetLastError() != ERROR_INVALID_ADDRESS) return false;
    const auto end = address + bytes;
    while (address < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info)) return false;
        const auto next = std::min<std::uint64_t>(end, reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize);
        if (next <= address || VirtualProtect(reinterpret_cast<void*>(address), static_cast<std::size_t>(next - address), protection, &previous) == FALSE) return false;
        address = next;
    }
    return true;
}

#endif

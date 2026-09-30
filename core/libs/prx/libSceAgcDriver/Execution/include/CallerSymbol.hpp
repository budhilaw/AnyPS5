#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_CALLERSYMBOL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_CALLERSYMBOL_HPP

#include <cstdint>
#include <cstdio>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <execinfo.h>
#endif

namespace AgcDriver {

#ifdef _WIN32
inline bool FollowsCall(std::uintptr_t address) {
    MEMORY_BASIC_INFORMATION memory{};
    if (address < 0x10000 || VirtualQuery(reinterpret_cast<const void*>(address - 7), &memory, sizeof(memory)) != sizeof(memory)) return false;
    const auto protection = memory.Protect & 0xffu;
    if (memory.State != MEM_COMMIT || memory.Type != MEM_IMAGE || (protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)) return false;
    const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
    if (address - 7 < base || address > base + memory.RegionSize) return false;
    const auto* code = reinterpret_cast<const unsigned char*>(address);
    return code[-5] == 0xe8 || (code[-2] == 0xff && (code[-1] & 0x38) == 0x10) || (code[-3] == 0xff && (code[-2] & 0x38) == 0x10) || (code[-6] == 0xff && (code[-5] & 0x38) == 0x10) || (code[-7] == 0xff && (code[-6] & 0x38) == 0x10);
}
#endif

inline int CaptureCallers(void** frames, int count) {
#ifdef _WIN32
    if (count <= 0) return 0;
    frames[0] = nullptr;
    int found = 1;
    const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    auto* slot = static_cast<void* const*>(__builtin_frame_address(0));
    const auto* limit = static_cast<void* const*>(tib->StackBase);
    for (int scanned = 0; slot < limit && found < count && scanned < 4096; ++slot, ++scanned) {
        const auto value = reinterpret_cast<std::uintptr_t>(*slot);
        if (FollowsCall(value)) frames[found++] = *slot;
    }
    return found;
#else
    return ::backtrace(frames, count);
#endif
}

inline std::string DescribeCaller(const void* address) {
    char text[160];
#ifdef _WIN32
    HMODULE module = nullptr;
    char path[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(address), &module) && module != nullptr) GetModuleFileNameA(module, path, sizeof(path));
    const char* name = path;
    for (const char* cursor = path; *cursor != 0; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') name = cursor + 1;
    }
    const auto offset = module != nullptr ? reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(module) : reinterpret_cast<std::uintptr_t>(address);
    std::snprintf(text, sizeof(text), "%s+0x%llx", name, static_cast<unsigned long long>(offset));
#else
    Dl_info info{};
    dladdr(address, &info);
    std::snprintf(text, sizeof(text), "%s+0x%lx", info.dli_sname ? info.dli_sname : "?", info.dli_saddr ? static_cast<unsigned long>(static_cast<const char*>(address) - static_cast<const char*>(info.dli_saddr)) : 0ul);
#endif
    return text;
}

}

#endif

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

inline int CaptureCallers(void** frames, int count) {
#ifdef _WIN32
    return static_cast<int>(CaptureStackBackTrace(0, static_cast<DWORD>(count), frames, nullptr));
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

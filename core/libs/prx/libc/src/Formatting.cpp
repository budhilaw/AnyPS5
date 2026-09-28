#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include "SceTypes.hpp"
#include "prx/libc/include/VarArgsAbi.hpp"
#include "prx/libc/include/FileStream.hpp"

#ifdef _WIN32
#include "prx/libc/include/WindowsFormatting.hpp"
#endif

extern "C" {

int APS5_VABI vfprintf_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    auto* native = GetNativeStream(stream);
#ifdef _WIN32
    std::string buffer;
    const int count = LibcDetail::FormatWindows(nullptr, 0, format, args, &buffer);
    const int result = std::fwrite(buffer.data(), 1, static_cast<size_t>(count), native) ==
        static_cast<size_t>(count) ? count : -1;
#else
#if defined(__x86_64__)
    LibcDetail::ReplaceNullStrings(format, *reinterpret_cast<LibcDetail::VaListLayout*>(args));
#endif
    const int result = std::vfprintf(native, format, *reinterpret_cast<std::va_list*>(args));
#endif
    stream->SyncStatus();
    return result;
}

int APS5_VABI fprintf_nid_postfix(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfprintf_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

#ifdef _WIN32

int APS5_VABI printf_nid_postfix(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI libc_printf_nid_postfix(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI snprintf_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::FormatWindows(buffer, size, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI sprintf_nid_postfix(char* buffer, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::FormatWindows(buffer, SIZE_MAX, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

#else

int APS5_VABI printf_nid_postfix(const char* format, ...) {
    std::va_list args;
    va_start(args, format);
#if defined(__x86_64__)
    LibcDetail::ReplaceNullStrings(format, *reinterpret_cast<LibcDetail::VaListLayout*>(&args));
#endif
    const int result = std::vprintf(format, args);
    va_end(args);
    return result;
}

int APS5_VABI libc_printf_nid_postfix(VA_ARGS) {
    (void)rcx; (void)r8; (void)r9;
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rdi, rsi, rdx, rcx, r8, r9,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 1u,
        reinterpret_cast<void*>(&overflow_arg_area));
    LibcDetail::ReplaceNullStrings(reinterpret_cast<const char*>(rdi), layout);
    return std::vprintf(reinterpret_cast<const char*>(rdi), *va);
}

int APS5_VABI snprintf_nid_postfix(VA_ARGS) {
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rdi, rsi, rdx, rcx, r8, r9,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 3u,
        reinterpret_cast<void*>(&overflow_arg_area));
    LibcDetail::ReplaceNullStrings(reinterpret_cast<const char*>(rdx), layout);
    return std::vsnprintf(
        reinterpret_cast<char*>(rdi),
        static_cast<size_t>(rsi),
        reinterpret_cast<const char*>(rdx),
        *va
    );
}

int APS5_VABI sprintf_nid_postfix(VA_ARGS) {
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rdi, rsi, rdx, rcx, r8, r9,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 2u,
        reinterpret_cast<void*>(&overflow_arg_area));
    LibcDetail::ReplaceNullStrings(reinterpret_cast<const char*>(rsi), layout);
    return std::vsprintf(
        reinterpret_cast<char*>(rdi),
        reinterpret_cast<const char*>(rsi),
        *va
    );
}

#endif

int APS5_VABI sscanf_nid_postfix(VA_ARGS) {
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rdi, rsi, rdx, rcx, r8, r9,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 2u,
        reinterpret_cast<void*>(&overflow_arg_area));
    return std::vsscanf(
        reinterpret_cast<const char*>(rdi),
        reinterpret_cast<const char*>(rsi),
        *va
    );
}

int APS5_VABI vprintf_nid_postfix(const char* str, VaList* c) {
#ifdef _WIN32
    return LibcDetail::PrintWindows(str, c);
#else
    std::va_list* va = reinterpret_cast<std::va_list*>(c);
#if defined(__x86_64__)
    LibcDetail::ReplaceNullStrings(str, *reinterpret_cast<LibcDetail::VaListLayout*>(c));
#endif
    return std::vprintf(str, *va);
#endif
}

int APS5_VABI vsprintf_nid_postfix(char* str, const char* format, VaList* args) {
#ifdef _WIN32
    return LibcDetail::FormatWindows(str, SIZE_MAX, format, args);
#else
#if defined(__x86_64__)
    LibcDetail::ReplaceNullStrings(format, *reinterpret_cast<LibcDetail::VaListLayout*>(args));
#endif
    return std::vsprintf(str, format, *reinterpret_cast<std::va_list*>(args));
#endif
}

int APS5_VABI vsnprintf_nid_postfix(char* str, size_t size, const char* format, VaList* c);

// C11 Annex K: on overflow the destination becomes an empty string and the call fails.
int APS5_VABI vsprintf_s_nid_postfix(char* str, size_t size, const char* format, VaList* args) {
    if (!str || !format || size == 0) return -1;
    const int written = vsnprintf_nid_postfix(str, size, format, args);
    if (written < 0 || static_cast<size_t>(written) >= size) { str[0] = '\0'; return -1; }
    return written;
}

int APS5_VABI vsnprintf_nid_postfix(char* str, size_t size, const char* format, VaList* c) {
#ifdef _WIN32
    return LibcDetail::FormatWindows(str, size, format, c);
#else
    std::va_list* va = reinterpret_cast<std::va_list*>(c);
#if defined(__x86_64__)
    LibcDetail::ReplaceNullStrings(format, *reinterpret_cast<LibcDetail::VaListLayout*>(c));
#endif
    return std::vsnprintf(str, size, format, *va);
#endif
}

int APS5_VABI puts_nid_postfix(const char* s) {
    return std::puts(s);
}

}

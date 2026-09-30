#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <stdexcept>
#ifdef _WIN32
#include "prx/libc/include/WindowsFormatting.hpp"
#endif

namespace {
constexpr int InvalidArgument = 22, OutOfRange = 34;
}

extern "C" {

int APS5_VABI strcpy_s_nid_postfix(char* destination, size_t size, const char* source) {
    if (destination == nullptr || size == 0) return InvalidArgument;
    if (source == nullptr) { destination[0] = 0; return InvalidArgument; }
    const auto length = std::strlen(source);
    if (length >= size) { destination[0] = 0; return OutOfRange; }
    std::memcpy(destination, source, length + 1);
    return 0;
}

int APS5_VABI snprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    if (buffer == nullptr || format == nullptr || size == 0) throw std::invalid_argument("snprintf_s: invalid argument");
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    const int result = LibcDetail::FormatWindows(buffer, size, format, arguments);
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    const int result = std::vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
#endif
    if (result < 0 || static_cast<size_t>(result) >= size) { buffer[0] = 0; return -1; }
    return result;
}

int APS5_VABI sprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    if (buffer == nullptr || format == nullptr || size == 0) throw std::invalid_argument("sprintf_s: invalid argument");
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    const int result = LibcDetail::FormatWindows(buffer, size, format, arguments);
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    const int result = std::vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
#endif
    if (result < 0 || static_cast<size_t>(result) >= size) { buffer[0] = 0; return -1; }
    return result;
}

int APS5_VABI vswprintf_nid_postfix(wchar_t* buffer, size_t count, const wchar_t* format, VaList* arguments) {
#ifdef _WIN32
    (void)buffer; (void)count; (void)format; (void)arguments;
    throw std::runtime_error("vswprintf: guest va_list marshalling is not implemented on Windows");
#else
    return std::vswprintf(buffer, count, format, *reinterpret_cast<std::va_list*>(arguments));
#endif
}

size_t APS5_VABI strspn_nid_postfix(const char* s, const char* accept) { return std::strspn(s, accept); }
size_t APS5_VABI wcslen_nid_postfix(const char16_t* s) {
    size_t length = 0;
    while (s[length] != 0) ++length;
    return length;
}

int APS5_VABI wcscmp_nid_postfix(const char16_t* a, const char16_t* b) {
    for (;; ++a, ++b) {
        if (*a != *b) return *a < *b ? -1 : 1;
        if (*a == 0) return 0;
    }
}

char16_t* APS5_VABI wcsncpy_nid_postfix(char16_t* destination, const char16_t* source, size_t count) {
    size_t index = 0;
    for (; index < count && source[index] != 0; ++index) destination[index] = source[index];
    for (; index < count; ++index) destination[index] = 0;
    return destination;
}
size_t APS5_VABI mbstowcs_nid_postfix(wchar_t* destination, const char* source, size_t count) { return std::mbstowcs(destination, source, count); }
size_t APS5_VABI wcstombs_nid_postfix(char* destination, const wchar_t* source, size_t count) { return std::wcstombs(destination, source, count); }

}

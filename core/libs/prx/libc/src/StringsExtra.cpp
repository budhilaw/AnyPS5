#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <stdexcept>

// Bounds-checked (Annex K) and wide-string functions. Guest wchar_t is 32-bit like the host's.
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

// Returns the length written, or -1 when the buffer would overflow (the buffer is emptied).
int APS5_VABI snprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    if (buffer == nullptr || format == nullptr || size == 0) throw std::invalid_argument("snprintf_s: invalid argument");
    std::va_list arguments;
    va_start(arguments, format);
    const int result = std::vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    if (result < 0 || static_cast<size_t>(result) >= size) { buffer[0] = 0; return -1; }
    return result;
}

int APS5_VABI sprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    if (buffer == nullptr || format == nullptr || size == 0) throw std::invalid_argument("sprintf_s: invalid argument");
    std::va_list arguments;
    va_start(arguments, format);
    const int result = std::vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
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
size_t APS5_VABI wcslen_nid_postfix(const wchar_t* s) { return std::wcslen(s); }
int APS5_VABI wcscmp_nid_postfix(const wchar_t* a, const wchar_t* b) { return std::wcscmp(a, b); }
size_t APS5_VABI mbstowcs_nid_postfix(wchar_t* destination, const char* source, size_t count) { return std::mbstowcs(destination, source, count); }
size_t APS5_VABI wcstombs_nid_postfix(char* destination, const wchar_t* source, size_t count) { return std::wcstombs(destination, source, count); }

}

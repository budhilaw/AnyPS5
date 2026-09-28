#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include "prx/libc/include/General.hpp"

// The bounds-checked string functions of C11 Annex K that the console's libc exports. They
// return errno values, and on a constraint violation the destination is cleared the way the
// standard requires (memcpy_s zeroes it, string copies terminate it) instead of invoking a handler.
namespace {

constexpr std::size_t RSIZE_MAX_VALUE = static_cast<std::size_t>(1) << 63;

}

extern "C" {

int APS5_VABI memcpy_s_nid_postfix(void* dest, std::size_t destsz, const void* src, std::size_t count) {
    if (dest == nullptr || destsz > RSIZE_MAX_VALUE) return EINVAL;
    if (src == nullptr || count > RSIZE_MAX_VALUE || count > destsz || (static_cast<const char*>(src) < static_cast<char*>(dest) + destsz && static_cast<char*>(dest) < static_cast<const char*>(src) + count)) {
        std::memset(dest, 0, destsz);
        return src == nullptr || count > RSIZE_MAX_VALUE ? EINVAL : ERANGE;
    }
    std::memcpy(dest, src, count);
    return 0;
}

int APS5_VABI memmove_s_nid_postfix(void* dest, std::size_t destsz, const void* src, std::size_t count) {
    if (dest == nullptr || destsz > RSIZE_MAX_VALUE) return EINVAL;
    if (src == nullptr || count > RSIZE_MAX_VALUE || count > destsz) {
        std::memset(dest, 0, destsz);
        return src == nullptr || count > RSIZE_MAX_VALUE ? EINVAL : ERANGE;
    }
    std::memmove(dest, src, count);
    return 0;
}

int APS5_VABI memset_s_nid_postfix(void* dest, std::size_t destsz, int ch, std::size_t count) {
    if (dest == nullptr || destsz > RSIZE_MAX_VALUE) return EINVAL;
    const auto clamped = count > destsz ? destsz : count;
    volatile unsigned char* cursor = static_cast<unsigned char*>(dest);
    for (std::size_t index = 0; index < clamped; ++index) cursor[index] = static_cast<unsigned char>(ch);
    return count > RSIZE_MAX_VALUE || count > destsz ? EINVAL : 0;
}

int APS5_VABI strncpy_s_nid_postfix(char* dest, std::size_t destsz, const char* src, std::size_t count) {
    if (dest == nullptr || destsz == 0 || destsz > RSIZE_MAX_VALUE) return EINVAL;
    if (src == nullptr || count > RSIZE_MAX_VALUE) { dest[0] = '\0'; return EINVAL; }
    std::size_t length = 0;
    while (length < count && src[length] != '\0') ++length;
    if (length >= destsz) { dest[0] = '\0'; return ERANGE; }
    std::memmove(dest, src, length);
    dest[length] = '\0';
    return 0;
}

int APS5_VABI strcat_s_nid_postfix(char* dest, std::size_t destsz, const char* src) {
    if (dest == nullptr || destsz == 0 || destsz > RSIZE_MAX_VALUE) return EINVAL;
    if (src == nullptr) { dest[0] = '\0'; return EINVAL; }
    std::size_t used = 0;
    while (used < destsz && dest[used] != '\0') ++used;
    if (used == destsz) { dest[0] = '\0'; return EINVAL; }
    const auto length = std::strlen(src);
    if (length >= destsz - used) { dest[0] = '\0'; return ERANGE; }
    std::memmove(dest + used, src, length + 1);
    return 0;
}

int APS5_VABI strncat_s_nid_postfix(char* dest, std::size_t destsz, const char* src, std::size_t count) {
    if (dest == nullptr || destsz == 0 || destsz > RSIZE_MAX_VALUE) return EINVAL;
    if (src == nullptr || count > RSIZE_MAX_VALUE) { dest[0] = '\0'; return EINVAL; }
    std::size_t used = 0;
    while (used < destsz && dest[used] != '\0') ++used;
    if (used == destsz) { dest[0] = '\0'; return EINVAL; }
    std::size_t length = 0;
    while (length < count && src[length] != '\0') ++length;
    if (length >= destsz - used) { dest[0] = '\0'; return ERANGE; }
    std::memmove(dest + used, src, length);
    dest[used + length] = '\0';
    return 0;
}

std::size_t APS5_VABI strnlen_s_nid_postfix(const char* str, std::size_t strsz) {
    if (str == nullptr) return 0;
    std::size_t length = 0;
    while (length < strsz && str[length] != '\0') ++length;
    return length;
}

}

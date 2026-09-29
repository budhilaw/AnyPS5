#include <string>
#include <cstdint>
#include <cstddef>
#include <ctime>
#include <cstring>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestTime.hpp"

extern "C" {

int64_t APS5_VABI libc_time_nid_postfix(int64_t* timer) {
    std::time_t t = std::time(nullptr);
    if (timer != nullptr) *timer = static_cast<int64_t>(t);
    return static_cast<int64_t>(t);
}

int64_t APS5_VABI time_nid_postfix(int64_t* timer) {
    return libc_time_nid_postfix(timer);
}

double APS5_VABI libc_difftime_nid_postfix(int64_t time1, int64_t time0) {
    return std::difftime(static_cast<std::time_t>(time1), static_cast<std::time_t>(time0));
}

double APS5_VABI difftime_nid_postfix(int64_t time1, int64_t time0) {
    return std::difftime(static_cast<std::time_t>(time1), static_cast<std::time_t>(time0));
}

GuestTm* APS5_VABI libc_gmtime_nid_postfix(const int64_t* timer) {
    static thread_local GuestTm result;
    const std::time_t t = static_cast<std::time_t>(*timer);
    std::tm converted{};
    if (gmtime_r(&t, &converted) == nullptr) return nullptr;
    result = ToGuestTm(converted);
    return &result;
}

GuestTm* APS5_VABI libc_localtime_nid_postfix(const int64_t* timer) {
    static thread_local GuestTm result;
    const std::time_t t = static_cast<std::time_t>(*timer);
    std::tm converted{};
    if (localtime_r(&t, &converted) == nullptr) return nullptr;
    result = ToGuestTm(converted);
    return &result;
}

GuestTm* APS5_VABI localtime_nid_postfix(const int64_t* timer) {
    return libc_localtime_nid_postfix(timer);
}

GuestTm* APS5_VABI localtime_s_nid_postfix(const int64_t* timer, GuestTm* result) {
    if (timer == nullptr || result == nullptr) return nullptr;
    const std::time_t t = static_cast<std::time_t>(*timer);
    std::tm converted{};
    if (localtime_r(&t, &converted) == nullptr) return nullptr;
    *result = ToGuestTm(converted);
    return result;
}

GuestTm* APS5_VABI gmtime_s_nid_postfix(const int64_t* timer, GuestTm* result) {
    if (timer == nullptr || result == nullptr) return nullptr;
    const std::time_t t = static_cast<std::time_t>(*timer);
    std::tm converted{};
    if (gmtime_r(&t, &converted) == nullptr) return nullptr;
    *result = ToGuestTm(converted);
    return result;
}

int64_t APS5_VABI libc_mktime_nid_postfix(GuestTm* timeptr) {
    std::tm host = ToHostTm(*timeptr);
    const auto result = std::mktime(&host);
    *timeptr = ToGuestTm(host);
    return static_cast<int64_t>(result);
}

int64_t APS5_VABI mktime_nid_postfix(GuestTm* timeptr) {
    std::tm host = ToHostTm(*timeptr);
    const auto result = std::mktime(&host);
    *timeptr = ToGuestTm(host);
    return static_cast<int64_t>(result);
}

size_t APS5_VABI libc_strftime_nid_postfix(char* str, size_t count, const char* format, const GuestTm* timeptr) {
    const std::tm host = ToHostTm(*timeptr);
    return std::strftime(str, count, format, &host);
}

size_t APS5_VABI strftime_nid_postfix(char* str, size_t count, const char* format, const GuestTm* timeptr) {
    const std::tm host = ToHostTm(*timeptr);
    return std::strftime(str, count, format, &host);
}

size_t APS5_VABI wcsftime_nid_postfix(std::uint16_t* str, size_t count, const std::uint16_t* format, const GuestTm* guestTime) {
    if (!str || !format || !guestTime || count == 0) return 0;
    const std::tm hostTime = ToHostTm(*guestTime);
    const std::tm* timeptr = &hostTime;
    std::string narrowFormat;
    for (const auto* c = format; *c; ++c) narrowFormat.push_back(*c < 0x80 ? static_cast<char>(*c) : '?');
    std::string narrow(count * 4 + 64, '\0');
    const auto length = std::strftime(narrow.data(), narrow.size(), narrowFormat.c_str(), timeptr);
    if (length == 0 || length >= count) return 0;
    for (size_t i = 0; i < length; ++i) str[i] = static_cast<unsigned char>(narrow[i]);
    str[length] = 0;
    return length;
}

}

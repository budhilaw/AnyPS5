#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTTIME_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTTIME_HPP

#include <ctime>

struct GuestTm {
    int sec, min, hour, mday, mon, year, wday, yday, isdst;
};
static_assert(sizeof(GuestTm) == 36);

inline std::tm ToHostTm(const GuestTm& guest) {
    std::tm host{};
    host.tm_sec = guest.sec; host.tm_min = guest.min; host.tm_hour = guest.hour;
    host.tm_mday = guest.mday; host.tm_mon = guest.mon; host.tm_year = guest.year;
    host.tm_wday = guest.wday; host.tm_yday = guest.yday; host.tm_isdst = guest.isdst;
    return host;
}

inline GuestTm ToGuestTm(const std::tm& host) {
    return GuestTm{host.tm_sec, host.tm_min, host.tm_hour, host.tm_mday, host.tm_mon, host.tm_year, host.tm_wday, host.tm_yday, host.tm_isdst};
}

inline bool HostGmtime(const std::time_t& time, std::tm& result) {
#ifdef _WIN32
    return gmtime_s(&result, &time) == 0;
#else
    return gmtime_r(&time, &result) != nullptr;
#endif
}

inline bool HostLocaltime(const std::time_t& time, std::tm& result) {
#ifdef _WIN32
    return localtime_s(&result, &time) == 0;
#else
    return localtime_r(&time, &result) != nullptr;
#endif
}

#endif

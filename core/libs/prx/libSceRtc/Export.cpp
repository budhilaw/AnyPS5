#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <ctime>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_RTC_ERROR_INVALID_VALUE = static_cast<int>(0x80B00001);
constexpr int SCE_RTC_ERROR_INVALID_POINTER = static_cast<int>(0x80B00002);
constexpr int SCE_RTC_ERROR_INVALID_YEAR = static_cast<int>(0x80B00003);
constexpr int SCE_RTC_ERROR_INVALID_MONTH = static_cast<int>(0x80B00004);
constexpr int SCE_RTC_ERROR_INVALID_DAY = static_cast<int>(0x80B00005);
constexpr int SCE_RTC_ERROR_INVALID_HOUR = static_cast<int>(0x80B00006);
constexpr int SCE_RTC_ERROR_INVALID_MINUTE = static_cast<int>(0x80B00007);
constexpr int SCE_RTC_ERROR_INVALID_SECOND = static_cast<int>(0x80B00008);
constexpr int SCE_RTC_ERROR_INVALID_MICROSECOND = static_cast<int>(0x80B00009);
constexpr int SCE_RTC_ERROR_INVALID_ARG = static_cast<int>(0x80B00011);

constexpr std::uint64_t TicksPerSecond = 1000000;
constexpr std::uint64_t TicksPerDay = TicksPerSecond * 86400;
constexpr std::int64_t DaysToUnixEpoch = 719162;
constexpr std::int64_t DaysToWin32Epoch = 584388;

bool leap(std::int64_t year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

int daysInMonth(std::int64_t year, int month) {
    static constexpr int lengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && leap(year) ? 29 : lengths[month - 1];
}

std::int64_t daysFromCivil(std::int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<std::uint64_t>(y - era * 400);
    const auto doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const auto doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468 + DaysToUnixEpoch;
}

void civilFromDays(std::int64_t days, std::int64_t& y, unsigned& m, unsigned& d) {
    days += 719468 - DaysToUnixEpoch;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const auto doe = static_cast<std::uint64_t>(days - era * 146097);
    const auto yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<std::int64_t>(yoe) + era * 400;
    const auto doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const auto mp = (5 * doy + 2) / 153;
    d = static_cast<unsigned>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<unsigned>(mp < 10 ? mp + 3 : mp - 9);
    y += m <= 2;
}

int validate(const RtcDateTime& time) {
    if (time.year < 1 || time.year > 9999) return SCE_RTC_ERROR_INVALID_YEAR;
    if (time.month < 1 || time.month > 12) return SCE_RTC_ERROR_INVALID_MONTH;
    if (time.day < 1 || time.day > daysInMonth(time.year, time.month)) return SCE_RTC_ERROR_INVALID_DAY;
    if (time.hour > 23) return SCE_RTC_ERROR_INVALID_HOUR;
    if (time.minute > 59) return SCE_RTC_ERROR_INVALID_MINUTE;
    if (time.second > 59) return SCE_RTC_ERROR_INVALID_SECOND;
    if (time.microsecond > 999999) return SCE_RTC_ERROR_INVALID_MICROSECOND;
    return 0;
}

std::uint64_t toTick(const RtcDateTime& time) {
    const auto days = daysFromCivil(time.year, time.month, time.day);
    return static_cast<std::uint64_t>(days) * TicksPerDay + (static_cast<std::uint64_t>(time.hour) * 3600 + time.minute * 60ull + time.second) * TicksPerSecond + time.microsecond;
}

void fromTick(std::uint64_t tick, RtcDateTime& time) {
    std::int64_t year; unsigned month, day;
    civilFromDays(static_cast<std::int64_t>(tick / TicksPerDay), year, month, day);
    const auto ofDay = tick % TicksPerDay;
    const auto seconds = ofDay / TicksPerSecond;
    time.year = static_cast<std::uint16_t>(year);
    time.month = static_cast<std::uint16_t>(month);
    time.day = static_cast<std::uint16_t>(day);
    time.hour = static_cast<std::uint16_t>(seconds / 3600);
    time.minute = static_cast<std::uint16_t>(seconds / 60 % 60);
    time.second = static_cast<std::uint16_t>(seconds % 60);
    time.microsecond = static_cast<std::uint32_t>(ofDay % TicksPerSecond);
}

std::uint64_t nowTick() {
    const auto since = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(DaysToUnixEpoch * TicksPerDay) + since);
}

int localOffsetMinutes(std::uint64_t utcTick) {
    const auto unix = static_cast<std::int64_t>(utcTick / TicksPerSecond) - DaysToUnixEpoch * 86400;
    const auto when = static_cast<std::time_t>(unix);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &when);
    std::tm utc{};
    gmtime_s(&utc, &when);
    const auto localSeconds = _mkgmtime(&local), utcSeconds = _mkgmtime(&utc);
    return static_cast<int>((localSeconds - utcSeconds) / 60);
#else
    localtime_r(&when, &local);
    return static_cast<int>(local.tm_gmtoff / 60);
#endif
}

int addTicks(RtcTick* dst, const RtcTick* src, std::int64_t ticks) {
    if (dst == nullptr || src == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    dst->tick = static_cast<std::uint64_t>(static_cast<std::int64_t>(src->tick) + ticks);
    return 0;
}

int addMonths(RtcTick* dst, const RtcTick* src, std::int64_t months) {
    if (dst == nullptr || src == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    RtcDateTime time{};
    fromTick(src->tick, time);
    std::int64_t index = static_cast<std::int64_t>(time.year) * 12 + (time.month - 1) + months;
    if (index < 0) return SCE_RTC_ERROR_INVALID_VALUE;
    const auto year = index / 12;
    const auto month = static_cast<int>(index % 12) + 1;
    if (year < 1 || year > 9999) return SCE_RTC_ERROR_INVALID_VALUE;
    time.year = static_cast<std::uint16_t>(year);
    time.month = static_cast<std::uint16_t>(month);
    if (time.day > daysInMonth(year, month)) time.day = static_cast<std::uint16_t>(daysInMonth(year, month));
    dst->tick = toTick(time);
    return 0;
}

}

extern "C" {

int APS5_VABI sceRtcInit(void) { return 0; }
int APS5_VABI sceRtcEnd(void) { return 0; }
int APS5_VABI sceRtcGetTickResolution(void) { return static_cast<int>(TicksPerSecond); }
int APS5_VABI sceRtcIsLeapYear(int year) { if (year < 1) return SCE_RTC_ERROR_INVALID_YEAR; return leap(year) ? 1 : 0; }

int APS5_VABI sceRtcGetDaysInMonth(int year, int month) {
    if (year < 1) return SCE_RTC_ERROR_INVALID_YEAR;
    if (month < 1 || month > 12) return SCE_RTC_ERROR_INVALID_MONTH;
    return daysInMonth(year, month);
}

int APS5_VABI sceRtcGetDayOfWeek(int year, int month, int day) {
    if (year < 1) return SCE_RTC_ERROR_INVALID_YEAR;
    if (month < 1 || month > 12) return SCE_RTC_ERROR_INVALID_MONTH;
    if (day < 1 || day > daysInMonth(year, month)) return SCE_RTC_ERROR_INVALID_DAY;
    return static_cast<int>((daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) + 1) % 7);
}

int APS5_VABI sceRtcCheckValid(const RtcDateTime* time) {
    if (time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    return validate(*time);
}

int APS5_VABI sceRtcGetCurrentTick(RtcTick* tick) {
    if (tick == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    tick->tick = nowTick();
    return 0;
}

int APS5_VABI sceRtcGetCurrentNetworkTick(RtcTick* tick) { return sceRtcGetCurrentTick(tick); }
int APS5_VABI sceRtcGetCurrentAdNetworkTick(RtcTick* tick) { return sceRtcGetCurrentTick(tick); }
int APS5_VABI sceRtcGetCurrentDebugNetworkTick(RtcTick* tick) { return sceRtcGetCurrentTick(tick); }
int APS5_VABI sceRtcGetCurrentRawNetworkTick(RtcTick* tick) { return sceRtcGetCurrentTick(tick); }

int APS5_VABI sceRtcGetCurrentClock(RtcDateTime* time, int time_zone_minutes) {
    if (time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    fromTick(static_cast<std::uint64_t>(static_cast<std::int64_t>(nowTick()) + static_cast<std::int64_t>(time_zone_minutes) * 60 * static_cast<std::int64_t>(TicksPerSecond)), *time);
    return 0;
}

int APS5_VABI sceRtcGetCurrentClockLocalTime(RtcDateTime* time) {
    if (time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    const auto now = nowTick();
    return sceRtcGetCurrentClock(time, localOffsetMinutes(now));
}

int APS5_VABI sceRtcConvertUtcToLocalTime(const RtcTick* utc, RtcTick* local_time) {
    if (utc == nullptr || local_time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    return addTicks(local_time, utc, static_cast<std::int64_t>(localOffsetMinutes(utc->tick)) * 60 * static_cast<std::int64_t>(TicksPerSecond));
}

int APS5_VABI sceRtcConvertLocalTimeToUtc(const RtcTick* local_time, RtcTick* utc) {
    if (utc == nullptr || local_time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    return addTicks(utc, local_time, -static_cast<std::int64_t>(localOffsetMinutes(local_time->tick)) * 60 * static_cast<std::int64_t>(TicksPerSecond));
}

int APS5_VABI sceRtcGetTick(const RtcDateTime* time, RtcTick* tick) {
    if (time == nullptr || tick == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    if (const auto error = validate(*time)) return error;
    tick->tick = toTick(*time);
    return 0;
}

int APS5_VABI sceRtcSetTick(RtcDateTime* time, const RtcTick* tick) {
    if (time == nullptr || tick == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    fromTick(tick->tick, *time);
    return 0;
}

int APS5_VABI sceRtcGetTime_t(const RtcDateTime* time, int64_t* seconds) {
    if (time == nullptr || seconds == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    if (const auto error = validate(*time)) return error;
    *seconds = static_cast<std::int64_t>(toTick(*time) / TicksPerSecond) - DaysToUnixEpoch * 86400;
    return 0;
}

int APS5_VABI sceRtcSetTime_t(RtcDateTime* time, int64_t seconds) {
    if (time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    const auto tick = (seconds + DaysToUnixEpoch * 86400) * static_cast<std::int64_t>(TicksPerSecond);
    if (tick < 0) return SCE_RTC_ERROR_INVALID_VALUE;
    fromTick(static_cast<std::uint64_t>(tick), *time);
    return 0;
}

int APS5_VABI sceRtcGetWin32FileTime(const RtcDateTime* time, uint64_t* win32_time) {
    if (time == nullptr || win32_time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    if (const auto error = validate(*time)) return error;
    const auto tick = toTick(*time);
    const auto base = static_cast<std::uint64_t>(DaysToWin32Epoch) * TicksPerDay;
    if (tick < base) return SCE_RTC_ERROR_INVALID_VALUE;
    *win32_time = (tick - base) * 10;
    return 0;
}

int APS5_VABI sceRtcSetWin32FileTime(RtcDateTime* time, uint64_t win32_time) {
    if (time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    fromTick(static_cast<std::uint64_t>(DaysToWin32Epoch) * TicksPerDay + win32_time / 10, *time);
    return 0;
}

int APS5_VABI sceRtcGetDosTime(const RtcDateTime* time, uint32_t* dos_time) {
    if (time == nullptr || dos_time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    if (const auto error = validate(*time)) return error;
    if (time->year < 1980 || time->year > 2107) return SCE_RTC_ERROR_INVALID_YEAR;
    *dos_time = (static_cast<std::uint32_t>(time->year - 1980) << 25) | (static_cast<std::uint32_t>(time->month) << 21) | (static_cast<std::uint32_t>(time->day) << 16) | (static_cast<std::uint32_t>(time->hour) << 11) | (static_cast<std::uint32_t>(time->minute) << 5) | (time->second / 2);
    return 0;
}

int APS5_VABI sceRtcSetDosTime(RtcDateTime* time, uint32_t dos_time) {
    if (time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    time->year = static_cast<std::uint16_t>(1980 + (dos_time >> 25));
    time->month = static_cast<std::uint16_t>((dos_time >> 21) & 0xf);
    time->day = static_cast<std::uint16_t>((dos_time >> 16) & 0x1f);
    time->hour = static_cast<std::uint16_t>((dos_time >> 11) & 0x1f);
    time->minute = static_cast<std::uint16_t>((dos_time >> 5) & 0x3f);
    time->second = static_cast<std::uint16_t>((dos_time & 0x1f) * 2);
    time->microsecond = 0;
    return validate(*time);
}

int APS5_VABI sceRtcTickAddTicks(RtcTick* dst, const RtcTick* src, int64_t ticks) { return addTicks(dst, src, ticks); }
int APS5_VABI sceRtcTickAddMicroseconds(RtcTick* dst, const RtcTick* src, int64_t usec) { return addTicks(dst, src, usec); }
int APS5_VABI sceRtcTickAddSeconds(RtcTick* dst, const RtcTick* src, int64_t seconds) { return addTicks(dst, src, seconds * static_cast<std::int64_t>(TicksPerSecond)); }
int APS5_VABI sceRtcTickAddMinutes(RtcTick* dst, const RtcTick* src, int64_t minutes) { return addTicks(dst, src, minutes * 60 * static_cast<std::int64_t>(TicksPerSecond)); }
int APS5_VABI sceRtcTickAddHours(RtcTick* dst, const RtcTick* src, int32_t hours) { return addTicks(dst, src, static_cast<std::int64_t>(hours) * 3600 * static_cast<std::int64_t>(TicksPerSecond)); }
int APS5_VABI sceRtcTickAddDays(RtcTick* dst, const RtcTick* src, int32_t days) { return addTicks(dst, src, static_cast<std::int64_t>(days) * static_cast<std::int64_t>(TicksPerDay)); }
int APS5_VABI sceRtcTickAddWeeks(RtcTick* dst, const RtcTick* src, int32_t weeks) { return addTicks(dst, src, static_cast<std::int64_t>(weeks) * 7 * static_cast<std::int64_t>(TicksPerDay)); }
int APS5_VABI sceRtcTickAddMonths(RtcTick* dst, const RtcTick* src, int32_t months) { return addMonths(dst, src, months); }
int APS5_VABI sceRtcTickAddYears(RtcTick* dst, const RtcTick* src, int32_t years) { return addMonths(dst, src, static_cast<std::int64_t>(years) * 12); }

int APS5_VABI sceRtcCompareTick(const RtcTick* tick1, const RtcTick* tick2) {
    if (tick1 == nullptr || tick2 == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    return tick1->tick < tick2->tick ? -1 : tick1->tick > tick2->tick ? 1 : 0;
}

int APS5_VABI sceRtcFormatRFC3339(char* date_time, const RtcTick* utc, int time_zone_minutes) {
    if (date_time == nullptr || utc == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    RtcDateTime time{};
    fromTick(static_cast<std::uint64_t>(static_cast<std::int64_t>(utc->tick) + static_cast<std::int64_t>(time_zone_minutes) * 60 * static_cast<std::int64_t>(TicksPerSecond)), time);
    if (time_zone_minutes == 0) {
        std::snprintf(date_time, 32, "%04u-%02u-%02uT%02u:%02u:%02u.%02uZ", time.year, time.month, time.day, time.hour, time.minute, time.second, time.microsecond / 10000);
    } else {
        const auto magnitude = time_zone_minutes < 0 ? -time_zone_minutes : time_zone_minutes;
        std::snprintf(date_time, 32, "%04u-%02u-%02uT%02u:%02u:%02u.%02u%c%02d:%02d", time.year, time.month, time.day, time.hour, time.minute, time.second, time.microsecond / 10000, time_zone_minutes < 0 ? '-' : '+', magnitude / 60, magnitude % 60);
    }
    return 0;
}

int APS5_VABI sceRtcFormatRFC3339LocalTime(char* date_time, const RtcTick* utc) {
    if (date_time == nullptr || utc == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    return sceRtcFormatRFC3339(date_time, utc, localOffsetMinutes(utc->tick));
}

int APS5_VABI sceRtcFormatRFC2822(char* date_time, const RtcTick* utc, int time_zone_minutes) {
    if (date_time == nullptr || utc == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    static constexpr const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static constexpr const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    RtcDateTime time{};
    fromTick(static_cast<std::uint64_t>(static_cast<std::int64_t>(utc->tick) + static_cast<std::int64_t>(time_zone_minutes) * 60 * static_cast<std::int64_t>(TicksPerSecond)), time);
    const auto weekday = sceRtcGetDayOfWeek(time.year, time.month, time.day);
    const auto magnitude = time_zone_minutes < 0 ? -time_zone_minutes : time_zone_minutes;
    std::snprintf(date_time, 32, "%s, %02u %s %04u %02u:%02u:%02u %c%02d%02d", days[weekday < 0 ? 0 : weekday], time.day, months[time.month - 1], time.year, time.hour, time.minute, time.second, time_zone_minutes < 0 ? '-' : '+', magnitude / 60, magnitude % 60);
    return 0;
}

int APS5_VABI sceRtcFormatRFC2822LocalTime(char* date_time, const RtcTick* utc) {
    if (date_time == nullptr || utc == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    return sceRtcFormatRFC2822(date_time, utc, localOffsetMinutes(utc->tick));
}

int APS5_VABI sceRtcParseRFC3339(RtcTick* utc, const char* date_time) {
    if (utc == nullptr || date_time == nullptr) return SCE_RTC_ERROR_INVALID_POINTER;
    unsigned year, month, day, hour, minute, second;
    int consumed = 0;
    if (std::sscanf(date_time, "%4u-%2u-%2uT%2u:%2u:%2u%n", &year, &month, &day, &hour, &minute, &second, &consumed) != 6 || consumed == 0) return SCE_RTC_ERROR_INVALID_ARG;
    const char* cursor = date_time + consumed;
    std::uint32_t microsecond = 0;
    if (*cursor == '.') {
        ++cursor;
        std::uint32_t scale = 100000;
        while (*cursor >= '0' && *cursor <= '9') { microsecond += static_cast<std::uint32_t>(*cursor - '0') * scale; scale /= 10; ++cursor; }
    }
    std::int64_t offsetMinutes = 0;
    if (*cursor == 'Z' || *cursor == 'z') ++cursor;
    else if (*cursor == '+' || *cursor == '-') {
        unsigned offsetHour, offsetMinute;
        if (std::sscanf(cursor + 1, "%2u:%2u", &offsetHour, &offsetMinute) != 2) return SCE_RTC_ERROR_INVALID_ARG;
        offsetMinutes = static_cast<std::int64_t>(offsetHour) * 60 + offsetMinute;
        if (*cursor == '-') offsetMinutes = -offsetMinutes;
        cursor += 6;
    } else return SCE_RTC_ERROR_INVALID_ARG;
    const RtcDateTime time{static_cast<std::uint16_t>(year), static_cast<std::uint16_t>(month), static_cast<std::uint16_t>(day), static_cast<std::uint16_t>(hour), static_cast<std::uint16_t>(minute), static_cast<std::uint16_t>(second), microsecond};
    if (const auto error = validate(time)) return error;
    utc->tick = static_cast<std::uint64_t>(static_cast<std::int64_t>(toTick(time)) - offsetMinutes * 60 * static_cast<std::int64_t>(TicksPerSecond));
    return 0;
}

int APS5_VABI sceRtcParseDateTime(RtcTick* utc, const char* date_time) { return sceRtcParseRFC3339(utc, date_time); }

}

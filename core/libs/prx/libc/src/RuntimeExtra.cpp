#include <string>
#include <set>
#include <mutex>
#include "prx/libc/include/GuestTime.hpp"
#include <climits>
#include <clocale>
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <stdexcept>

extern "C" {

[[noreturn]] void APS5_VABI _Exit_nid_postfix(int code);

int APS5_VABI fgetpos_nid_postfix(FileStream* stream, std::int64_t* position) {
    if (position == nullptr) throw std::invalid_argument("fgetpos: null position");
    const auto result = ::ftello(GetNativeStream(stream));
    if (result < 0) return -1;
    *position = result;
    return 0;
}

int APS5_VABI fsetpos_nid_postfix(FileStream* stream, const std::int64_t* position) {
    if (position == nullptr) throw std::invalid_argument("fsetpos: null position");
    return ::fseeko(GetNativeStream(stream), static_cast<off_t>(*position), SEEK_SET) == 0 ? 0 : -1;
}

void APS5_VABI _Lockfilelock_nid_postfix(FileStream* stream) { ::flockfile(GetNativeStream(stream)); }
void APS5_VABI _Unlockfilelock_nid_postfix(FileStream* stream) { ::funlockfile(GetNativeStream(stream)); }

[[noreturn]] void APS5_VABI quick_exit_nid_postfix(int code) { _Exit_nid_postfix(code); }

struct lconv* APS5_VABI localeconv_nid_postfix() {
    static struct lconv conventions = [] {
        struct lconv c{};
        static char dot[] = ".", empty[] = "";
        c.decimal_point = dot;
        c.thousands_sep = c.grouping = c.int_curr_symbol = c.currency_symbol = empty;
        c.mon_decimal_point = c.mon_thousands_sep = c.mon_grouping = c.positive_sign = c.negative_sign = empty;
        c.int_frac_digits = c.frac_digits = c.p_cs_precedes = c.p_sep_by_space = c.n_cs_precedes = c.n_sep_by_space = c.p_sign_posn = c.n_sign_posn = CHAR_MAX;
        c.int_p_cs_precedes = c.int_n_cs_precedes = c.int_p_sep_by_space = c.int_n_sep_by_space = c.int_p_sign_posn = c.int_n_sign_posn = CHAR_MAX;
        return c;
    }();
    return &conventions;
}

char* APS5_VABI setlocale_nid_postfix(int category, const char* locale) {
    static char current[] = "C";
    if (locale == nullptr || locale[0] == 0 || std::strcmp(locale, "C") == 0 || std::strcmp(locale, "POSIX") == 0) return current;
    static std::mutex mutex;
    static std::set<std::string> names;
    std::lock_guard lock(mutex);
    const auto [it, inserted] = names.insert(locale);
    if (inserted) APS5_LOG_OUT("setlocale(%d, \"%s\"): accepted with C locale behaviour", category, locale);
    return const_cast<char*>(it->c_str());
}

std::int64_t APS5_VABI clock_nid_postfix() {
    return static_cast<std::int64_t>(std::clock()) * 1000000 / CLOCKS_PER_SEC;
}

GuestTm* APS5_VABI gmtime_nid_postfix(const std::int64_t* timer) {
    if (timer == nullptr) throw std::invalid_argument("gmtime: null time");
    static thread_local GuestTm result;
    const auto time = static_cast<std::time_t>(*timer);
    std::tm converted{};
    if (gmtime_r(&time, &converted) == nullptr) return nullptr;
    result = ToGuestTm(converted);
    return &result;
}

}

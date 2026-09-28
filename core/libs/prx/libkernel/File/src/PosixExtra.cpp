#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Further libScePosix file functions and sceKernelFtruncate over host descriptors and paths.
extern "C" int* __error_nid_postfix();
extern "C" int APS5_VABI sceKernelUnlink(const char* path);
extern "C" int APS5_VABI sceKernelRmdir(const char* path);
extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path);

namespace {
constexpr int SCE_KERNEL_ERROR_EBADF = 0x80020009;
int PosixFailure(int value) { *__error_nid_postfix() = value; return -1; }
int PosixFromSce(int result) { return result < 0 ? PosixFailure(result & 0xff) : result; }
int PosixFromHost(int result) { return result < 0 ? PosixFailure(errno) : result; }
struct GuestTimeval { std::int64_t seconds; std::int64_t microseconds; };
void ToHost(const GuestTimeval* guest, timeval* host) { for (int i = 0; i < 2; ++i) { host[i].tv_sec = static_cast<time_t>(guest[i].seconds); host[i].tv_usec = static_cast<suseconds_t>(guest[i].microseconds); } }
}

extern "C" {

int APS5_VABI unlink_nid_postfix(const char* path) { if (path == nullptr) return PosixFailure(14); return PosixFromSce(sceKernelUnlink(path)); }
int APS5_VABI rmdir_nid_postfix(const char* path) { if (path == nullptr) return PosixFailure(14); return PosixFromSce(sceKernelRmdir(path)); }
int APS5_VABI fchmod_nid_postfix(int d, int mode) { return PosixFromHost(::fchmod(d, static_cast<mode_t>(mode))); }

int APS5_VABI sceKernelFchmod(int d, int mode) {
    if (::fchmod(d, static_cast<mode_t>(mode)) == 0) return 0;
    return static_cast<int>(0x80020000u | static_cast<unsigned>(errno & 0xff));
}

int APS5_VABI utimes_nid_postfix(const char* path, const GuestTimeval* times) {
    if (path == nullptr) return PosixFailure(14);
    timeval host[2];
    if (times != nullptr) ToHost(times, host);
    return PosixFromHost(::utimes(ResolvePath_nid_no_patch(path).c_str(), times != nullptr ? host : nullptr));
}

int APS5_VABI futimes_nid_postfix(int d, const GuestTimeval* times) {
    timeval host[2];
    if (times != nullptr) ToHost(times, host);
    return PosixFromHost(::futimes(d, times != nullptr ? host : nullptr));
}

int APS5_VABI sceKernelFtruncate(int d, std::int64_t length) {
    if (::ftruncate(d, static_cast<off_t>(length)) != 0) return errno == EBADF ? SCE_KERNEL_ERROR_EBADF : 0x80020000 | errno;
    return 0;
}

}

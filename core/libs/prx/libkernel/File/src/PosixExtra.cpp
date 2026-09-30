#include <cerrno>
#include <cstdint>
#include <filesystem>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/NativeFile.hpp"

extern "C" int* __error_nid_postfix();
extern "C" int APS5_VABI sceKernelUnlink(const char* path);
extern "C" int APS5_VABI sceKernelRmdir(const char* path);
extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path);

namespace {
constexpr int SCE_KERNEL_ERROR_EBADF = 0x80020009;
int PosixFailure(int value) { *__error_nid_postfix() = value; return -1; }
int PosixFromSce(int result) { return result < 0 ? PosixFailure(result & 0xff) : result; }
int PosixFromHost(int result) { return result < 0 ? PosixFailure(errno) : result; }
int SceFromErrno() { return static_cast<int>(0x80020000u | static_cast<unsigned>(errno & 0xff)); }
struct GuestTimeval { std::int64_t seconds; std::int64_t microseconds; };
const File::NativeTimeval* ToHost(const GuestTimeval* guest, File::NativeTimeval* host) {
    if (guest == nullptr) return nullptr;
    for (int i = 0; i < 2; ++i) host[i] = {guest[i].seconds, guest[i].microseconds};
    return host;
}
}

extern "C" {

int APS5_VABI unlink_nid_postfix(const char* path) { if (path == nullptr) return PosixFailure(14); return PosixFromSce(sceKernelUnlink(path)); }
int APS5_VABI rmdir_nid_postfix(const char* path) { if (path == nullptr) return PosixFailure(14); return PosixFromSce(sceKernelRmdir(path)); }
int APS5_VABI fchmod_nid_postfix(int d, int mode) { return PosixFromHost(File::NativeFchmod(d, mode)); }

int APS5_VABI sceKernelFchmod(int d, int mode) {
    if (File::NativeFchmod(d, mode) == 0) return 0;
    return SceFromErrno();
}

int APS5_VABI utimes_nid_postfix(const char* path, const GuestTimeval* times) {
    if (path == nullptr) return PosixFailure(14);
    File::NativeTimeval host[2];
    return PosixFromHost(File::NativeUtimes(ResolvePath_nid_no_patch(path), ToHost(times, host)));
}

int APS5_VABI futimes_nid_postfix(int d, const GuestTimeval* times) {
    File::NativeTimeval host[2];
    return PosixFromHost(File::NativeFutimes(d, ToHost(times, host)));
}

int APS5_VABI sceKernelUtimes(const char* path, const GuestTimeval* times) {
    if (path == nullptr) return static_cast<int>(0x8002000e);
    File::NativeTimeval host[2];
    if (File::NativeUtimes(ResolvePath_nid_no_patch(path), ToHost(times, host)) != 0) return SceFromErrno();
    return 0;
}

int APS5_VABI sceKernelTruncate(const char* path, std::int64_t length) {
    if (path == nullptr) return static_cast<int>(0x8002000e);
    if (File::NativeTruncate(ResolvePath_nid_no_patch(path), length) != 0) return SceFromErrno();
    return 0;
}

int APS5_VABI fsync_nid_postfix(int d) { return PosixFromHost(File::NativeFsync(d)); }

int APS5_VABI sceKernelFtruncate(int d, std::int64_t length) {
    if (File::NativeFtruncate(d, length) != 0) return errno == EBADF ? SCE_KERNEL_ERROR_EBADF : 0x80020000 | errno;
    return 0;
}

}

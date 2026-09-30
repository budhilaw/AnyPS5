#include <cstdint>
#include <string>
#include <stdexcept>
#include <cstring>
#include <mutex>
#include <map>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <filesystem>
#include <cerrno>
#include <atomic>
#include <cstdlib>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/GuestBufferAccess.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif


extern "C" int* __error_nid_postfix();
extern "C" int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
extern "C" int APS5_VABI sceKernelFstat(int d, FileStat* sb);
extern "C" int APS5_VABI sceKernelStat(const char* path, FileStat* sb);
extern "C" int APS5_VABI sceKernelMkdir(const char* path, uint16_t mode);
extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path);
namespace {
int PosixFailure(int value) { *__error_nid_postfix() = value; return -1; }
int PosixFromSce(int result) { return result < 0 ? PosixFailure(result & 0xff) : result; }
int PosixFromHost(long long result) { return result < 0 ? PosixFailure(errno) : static_cast<int>(result); }
}

namespace {
constexpr int SCE_KERNEL_ERROR_EBADF_ = static_cast<int>(0x80020009);
constexpr int SCE_KERNEL_ERROR_ENOENT_ = static_cast<int>(0x80020002);
constexpr int SCE_KERNEL_ERROR_EEXIST_ = static_cast<int>(0x80020011);
constexpr int SCE_KERNEL_ERROR_ENOTEMPTY_ = static_cast<int>(0x80020042);
constexpr int SCE_KERNEL_ERROR_EINVAL_ = static_cast<int>(0x80020016);
std::mutex directoriesMutex;
std::map<int, DIR*> directories;
struct GuestDirent { std::uint32_t fileno; std::uint16_t reclen; std::uint8_t type; std::uint8_t namlen; char name[256]; };
int MapPathError(int error, const char* what, const std::filesystem::path& native) {
    if (error == ENOENT || error == ENOTDIR) return SCE_KERNEL_ERROR_ENOENT_;
    if (error == EEXIST) return SCE_KERNEL_ERROR_EEXIST_;
    if (error == ENOTEMPTY) return SCE_KERNEL_ERROR_ENOTEMPTY_;
    throw std::runtime_error(std::string(what) + " failed for " + native.string() + ", errno=" + std::to_string(error));
}
int ReadDirectory(int fd, char* buf, int nbytes, std::int64_t* basep) {
    if (buf == nullptr || nbytes < static_cast<int>(sizeof(GuestDirent))) return SCE_KERNEL_ERROR_EINVAL_;
    DIR* stream = nullptr;
    {
        std::lock_guard lock(directoriesMutex);
        auto found = directories.find(fd);
        if (found == directories.end()) {
            const int duplicate = ::dup(fd);
            if (duplicate < 0) return SCE_KERNEL_ERROR_EBADF_;
            stream = ::fdopendir(duplicate);
            if (stream == nullptr) { ::close(duplicate); return SCE_KERNEL_ERROR_EBADF_; }
            directories.emplace(fd, stream);
        } else {
            stream = found->second;
        }
    }
    if (basep != nullptr) *basep = static_cast<std::int64_t>(::telldir(stream));
    int written = 0;
    for (;;) {
        const long position = ::telldir(stream);
        errno = 0;
        const dirent* entry = ::readdir(stream);
        if (entry == nullptr) break;
        const auto length = std::strlen(entry->d_name);
        const auto record = static_cast<std::uint16_t>((offsetof(GuestDirent, name) + length + 1 + 3) & ~std::size_t{3});
        if (written + record > nbytes) { ::seekdir(stream, position); break; }
        GuestDirent guest{};
        guest.fileno = static_cast<std::uint32_t>(entry->d_ino);
        guest.reclen = record;
        guest.type = entry->d_type;
        guest.namlen = static_cast<std::uint8_t>(length);
        std::memcpy(guest.name, entry->d_name, length + 1);
        std::memcpy(buf + written, &guest, record);
        written += record;
    }
    return written;
}
}

void ReleaseDirectoryStream(int fd) {
    std::lock_guard lock(directoriesMutex);
    auto found = directories.find(fd);
    if (found == directories.end()) return;
    ::closedir(found->second);
    directories.erase(found);
}

extern "C" {

int APS5_VABI chmod_nid_postfix(const char* path, int mode) {
    if (path == nullptr) return PosixFailure(14);
    return PosixFromHost(::chmod(ResolvePath_nid_no_patch(path).c_str(), static_cast<mode_t>(mode)));
}

int APS5_VABI close_nid_postfix(int d) {
    ReleaseDirectoryStream(d);
    if (d >= GuestSockets::FirstDescriptor) return GuestSockets::Close(d);
#ifdef _WIN32
    return _close(d);
#else
    return ::close(d);
#endif
}

int APS5_VABI flock_nid_postfix(int d, int operation) {
    return PosixFromHost(::flock(d, operation));
}

int64_t APS5_VABI fstat_nid_disambig1_nid_postfix(int d, FileStat* sb) {
    if (sb == nullptr) return PosixFailure(14);
    return PosixFromSce(sceKernelFstat(d, sb));
}

int APS5_VABI ftruncate_nid_postfix(int d, int64_t length) {
    return PosixFromHost(::ftruncate(d, static_cast<off_t>(length)));
}

int64_t APS5_VABI lseek_nid_postfix(int d, int64_t offset, int whence) {
    const auto result = ::lseek(d, static_cast<off_t>(offset), whence);
    if (result < 0) { PosixFailure(errno); return -1; }
    return result;
}

int APS5_VABI mkdir_nid_postfix(const char* path, uint16_t mode) {
    if (path == nullptr) return PosixFailure(14);
    return PosixFromSce(sceKernelMkdir(path, mode));
}

int APS5_VABI open_nid_postfix(const char* path, int flags, int mode) {
    if (path == nullptr) return PosixFailure(14);
    return PosixFromSce(sceKernelOpen(path, flags, static_cast<std::uint16_t>(mode)));
}

int64_t APS5_VABI pread_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr && nbytes != 0) return PosixFailure(14);
    const auto result = ReadIntoGuest(buf, nbytes, [&](void* target, std::size_t count) { return ::pread(d, target, count, static_cast<off_t>(offset)); });
    if (result < 0) { PosixFailure(errno); return -1; }
    return result;
}

int64_t APS5_VABI pwrite_nid_disambig1_nid_postfix(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr && nbytes != 0) return PosixFailure(14);
    PrepareGuestBuffer(buf, nbytes, false);
    const auto result = ::pwrite(d, buf, nbytes, static_cast<off_t>(offset));
    if (result < 0) { PosixFailure(errno); return -1; }
    return result;
}

int64_t APS5_VABI read_nid_postfix(int d, void* buf, uint64_t nbytes) {
    if (buf == nullptr && nbytes != 0) return PosixFailure(14);
    const auto result = ReadIntoGuest(buf, static_cast<size_t>(nbytes), [&](void* target, std::size_t count) { return ::read(d, target, count); });
    if (result < 0) return PosixFailure(errno);
    return static_cast<int>(result);
}

int64_t APS5_VABI write_nid_postfix(int d, const char* str, int64_t size) {
    if (str == nullptr && size != 0) return PosixFailure(14);
    PrepareGuestBuffer(str, static_cast<size_t>(size), false);
    const auto result = ::write(d, str, static_cast<size_t>(size));
    if (result < 0) return PosixFailure(errno);
    return static_cast<int>(result);
}

int APS5_VABI stat_nid_postfix(const char* path, FileStat* sb) {
    if (path == nullptr || sb == nullptr) return PosixFailure(14);
    return PosixFromSce(sceKernelStat(path, sb));
}

int APS5_VABI sceKernelCheckReachability(const char* path) {
 (void)path;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelFstat(int d, FileStat* sb) {
 if (sb == nullptr) return SCE_KERNEL_ERROR_EINVAL_;
 const int error = File::FillDescriptorStat(d, sb);
 if (error == EBADF) return SCE_KERNEL_ERROR_EBADF_;
 if (error != 0) throw std::runtime_error(std::string(__func__) + ": fstat failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(error));
 return 0;
}

int APS5_VABI sceKernelFsync(int fd) {
    if (::fsync(fd) != 0) return errno == EBADF ? SCE_KERNEL_ERROR_EBADF_ : static_cast<int>(0x80020000u | static_cast<unsigned>(errno & 0xff));
    return 0;
}

int APS5_VABI sceKernelGetdents(int fd, char* buf, int nbytes) {
    return ReadDirectory(fd, buf, nbytes, nullptr);
}

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
    return ReadDirectory(fd, buf, nbytes, basep);
}

int APS5_VABI sceKernelMkdir(const char* path, uint16_t mode) {
    if (path == nullptr) throw std::invalid_argument(std::string(__func__) + ": path is null");
    const auto native = ResolvePath_nid_no_patch(path);
    if (::mkdir(native.c_str(), static_cast<mode_t>(mode)) != 0) return MapPathError(errno, "mkdir", native);
    return 0;
}

int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr && nbytes != 0) throw std::invalid_argument(std::string(__func__) + ": buf is null");
    const auto result = ReadIntoGuest(buf, nbytes, [&](void* target, std::size_t count) { return ::pread(d, target, count, static_cast<off_t>(offset)); });
    {
        static const bool trace = std::getenv("ANYPS5_TRACE_IO") != nullptr;
        static std::atomic<std::uint64_t> preads{0};
        const auto count = ++preads;
        if (trace && (count & (count - 1)) == 0) APS5_LOG_OUT("pread #%llu fd=%d bytes=%zu at %lld -> %lld", static_cast<unsigned long long>(count), d, nbytes, static_cast<long long>(offset), static_cast<long long>(result));
    }
    if (result < 0) { if (errno == EBADF) return SCE_KERNEL_ERROR_EBADF_; throw std::runtime_error(std::string(__func__) + ": pread failed, errno=" + std::to_string(errno)); }
    return result;
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr && nbytes != 0) throw std::invalid_argument(std::string(__func__) + ": buf is null");
    PrepareGuestBuffer(buf, nbytes, false);
    const auto result = ::pwrite(d, buf, nbytes, static_cast<off_t>(offset));
    if (result < 0) { if (errno == EBADF) return SCE_KERNEL_ERROR_EBADF_; throw std::runtime_error(std::string(__func__) + ": pwrite failed, errno=" + std::to_string(errno)); }
    return result;
}

int APS5_VABI sceKernelRename(const char* from, const char* to) {
    if (from == nullptr || to == nullptr) throw std::invalid_argument(std::string(__func__) + ": path is null");
    const auto source = ResolvePath_nid_no_patch(from);
    const auto destination = ResolvePath_nid_no_patch(to);
    if (::rename(source.c_str(), destination.c_str()) != 0) return MapPathError(errno, "rename", source);
    return 0;
}

int APS5_VABI sceKernelRmdir(const char* path) {
    if (path == nullptr) throw std::invalid_argument(std::string(__func__) + ": path is null");
    const auto native = ResolvePath_nid_no_patch(path);
    if (::rmdir(native.c_str()) != 0) return MapPathError(errno, "rmdir", native);
    return 0;
}

}

#include "prx/libkernel/File/include/NativeFile.hpp"

#include <cerrno>
#include <cstdio>
#include <functional>
#include <limits>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace File {
namespace {

int failWithErrorCode(const std::error_code& error) {
    const auto condition = error.default_error_condition();
    errno = condition.category() == std::generic_category() ? condition.value() : EIO;
    return -1;
}

#ifdef _WIN32

int errnoFromWindows(DWORD error) {
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_NETPATH:
        return ENOENT;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return EACCES;
    case ERROR_INVALID_HANDLE:
        return EBADF;
    case ERROR_NOACCESS:
        return EFAULT;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return EEXIST;
    case ERROR_DIR_NOT_EMPTY:
        return ENOTEMPTY;
    case ERROR_DIRECTORY:
        return ENOTDIR;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ENOMEM;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return ENOSPC;
    case ERROR_INVALID_PARAMETER:
    case ERROR_NEGATIVE_SEEK:
        return EINVAL;
    case ERROR_LOCK_FAILED:
        return EWOULDBLOCK;
    default:
        return EIO;
    }
}

int failWithLastError() {
    errno = errnoFromWindows(GetLastError());
    return -1;
}

HANDLE nativeHandle(int descriptor) {
    if (descriptor < 0) return INVALID_HANDLE_VALUE;
    return reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
}

LARGE_INTEGER filePosition(HANDLE handle, bool& valid) {
    LARGE_INTEGER zero{};
    LARGE_INTEGER position{};
    valid = SetFilePointerEx(handle, zero, &position, FILE_CURRENT) != FALSE;
    return position;
}

template<typename TTransfer>
std::int64_t transferAt(int descriptor, std::size_t bytes, std::int64_t offset, TTransfer transfer) {
    if (offset < 0) { errno = EINVAL; return -1; }
    const HANDLE handle = nativeHandle(descriptor);
    if (handle == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    bool restore = false;
    const auto saved = filePosition(handle, restore);
    std::int64_t done = 0;
    while (static_cast<std::size_t>(done) < bytes) {
        const auto remaining = bytes - static_cast<std::size_t>(done);
        const auto chunk = static_cast<DWORD>(remaining > (1u << 30) ? (1u << 30) : remaining);
        const auto position = static_cast<std::uint64_t>(offset + done);
        OVERLAPPED overlapped{};
        overlapped.Offset = static_cast<DWORD>(position);
        overlapped.OffsetHigh = static_cast<DWORD>(position >> 32u);
        DWORD transferred = 0;
        if (!transfer(handle, static_cast<std::size_t>(done), chunk, &transferred, &overlapped)) {
            const auto error = GetLastError();
            if (error == ERROR_HANDLE_EOF) break;
            if (done != 0) break;
            if (restore) SetFilePointerEx(handle, saved, nullptr, FILE_BEGIN);
            errno = errnoFromWindows(error);
            return -1;
        }
        done += transferred;
        if (transferred < chunk) break;
    }
    if (restore) SetFilePointerEx(handle, saved, nullptr, FILE_BEGIN);
    return done;
}

FILETIME fileTime(const NativeTimeval& time) {
    const auto ticks = static_cast<std::uint64_t>(time.seconds * 10000000 + time.microseconds * 10 + 116444736000000000ll);
    return FILETIME{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32u)};
}

int setTimes(HANDLE handle, const NativeTimeval* times) {
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    const FILETIME access = times != nullptr ? fileTime(times[0]) : now;
    const FILETIME modification = times != nullptr ? fileTime(times[1]) : now;
    return SetFileTime(handle, nullptr, &access, &modification) != FALSE ? 0 : failWithLastError();
}

#endif

}

int NativeOpenRead(const std::filesystem::path& path) {
#ifdef _WIN32
    return ::_wopen(path.wstring().c_str(), _O_RDONLY | _O_BINARY);
#else
    return ::open(path.c_str(), O_RDONLY);
#endif
}

std::int64_t NativePread(int descriptor, void* buffer, std::size_t bytes, std::int64_t offset) {
#ifdef _WIN32
    auto* target = static_cast<std::byte*>(buffer);
    return transferAt(descriptor, bytes, offset, [&](HANDLE handle, std::size_t done, DWORD chunk, DWORD* transferred, OVERLAPPED* overlapped) {
        return ReadFile(handle, target + done, chunk, transferred, overlapped) != FALSE;
    });
#else
    return static_cast<std::int64_t>(::pread(descriptor, buffer, bytes, static_cast<off_t>(offset)));
#endif
}

std::int64_t NativePwrite(int descriptor, const void* buffer, std::size_t bytes, std::int64_t offset) {
#ifdef _WIN32
    const auto* source = static_cast<const std::byte*>(buffer);
    return transferAt(descriptor, bytes, offset, [&](HANDLE handle, std::size_t done, DWORD chunk, DWORD* transferred, OVERLAPPED* overlapped) {
        return WriteFile(handle, source + done, chunk, transferred, overlapped) != FALSE;
    });
#else
    return static_cast<std::int64_t>(::pwrite(descriptor, buffer, bytes, static_cast<off_t>(offset)));
#endif
}

std::int64_t NativeLseek(int descriptor, std::int64_t offset, int whence) {
#ifdef _WIN32
    return ::_lseeki64(descriptor, offset, whence);
#else
    return static_cast<std::int64_t>(::lseek(descriptor, static_cast<off_t>(offset), whence));
#endif
}

int NativeFsync(int descriptor) {
#ifdef _WIN32
    return ::_commit(descriptor);
#else
    return ::fsync(descriptor);
#endif
}

int NativeFtruncate(int descriptor, std::int64_t length) {
#ifdef _WIN32
    const auto error = ::_chsize_s(descriptor, length);
    if (error == 0) return 0;
    errno = error;
    return -1;
#else
    return ::ftruncate(descriptor, static_cast<off_t>(length));
#endif
}

int NativeTruncate(const std::filesystem::path& path, std::int64_t length) {
    if (length < 0) { errno = EINVAL; return -1; }
    std::error_code error;
    std::filesystem::resize_file(path, static_cast<std::uintmax_t>(length), error);
    return error ? failWithErrorCode(error) : 0;
}

int NativeChmod(const std::filesystem::path& path, int mode) {
#ifdef _WIN32
    return ::_wchmod(path.wstring().c_str(), (mode & 0200) != 0 ? _S_IREAD | _S_IWRITE : _S_IREAD);
#else
    return ::chmod(path.c_str(), static_cast<mode_t>(mode));
#endif
}

int NativeFchmod(int descriptor, int mode) {
#ifdef _WIN32
    const HANDLE handle = nativeHandle(descriptor);
    if (handle == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    FILE_BASIC_INFO info{};
    if (!GetFileInformationByHandleEx(handle, FileBasicInfo, &info, sizeof(info))) return failWithLastError();
    const auto readOnly = (mode & 0200) == 0;
    const auto attributes = readOnly ? info.FileAttributes | FILE_ATTRIBUTE_READONLY : info.FileAttributes & ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY);
    if (attributes == info.FileAttributes) return 0;
    info.FileAttributes = attributes;
    return SetFileInformationByHandle(handle, FileBasicInfo, &info, sizeof(info)) != FALSE ? 0 : failWithLastError();
#else
    return ::fchmod(descriptor, static_cast<mode_t>(mode));
#endif
}

int NativeMkdir(const std::filesystem::path& path, int mode) {
#ifdef _WIN32
    static_cast<void>(mode);
    return CreateDirectoryW(path.wstring().c_str(), nullptr) != FALSE ? 0 : failWithLastError();
#else
    return ::mkdir(path.c_str(), static_cast<mode_t>(mode));
#endif
}

int NativeRmdir(const std::filesystem::path& path) {
#ifdef _WIN32
    return RemoveDirectoryW(path.wstring().c_str()) != FALSE ? 0 : failWithLastError();
#else
    return ::rmdir(path.c_str());
#endif
}

int NativeRename(const std::filesystem::path& from, const std::filesystem::path& to) {
#ifdef _WIN32
    return MoveFileExW(from.wstring().c_str(), to.wstring().c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE ? 0 : failWithLastError();
#else
    return ::rename(from.c_str(), to.c_str());
#endif
}

int NativeFlock(int descriptor, int operation) {
#ifdef _WIN32
    const HANDLE handle = nativeHandle(descriptor);
    if (handle == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    OVERLAPPED overlapped{};
    if ((operation & 8) != 0) return UnlockFileEx(handle, 0, MAXDWORD, MAXDWORD, &overlapped) != FALSE || GetLastError() == ERROR_NOT_LOCKED ? 0 : failWithLastError();
    DWORD flags = 0;
    if ((operation & 2) != 0) flags |= LOCKFILE_EXCLUSIVE_LOCK;
    if ((operation & 4) != 0) flags |= LOCKFILE_FAIL_IMMEDIATELY;
    return LockFileEx(handle, flags, 0, MAXDWORD, MAXDWORD, &overlapped) != FALSE ? 0 : failWithLastError();
#else
    return ::flock(descriptor, operation);
#endif
}

int NativeUtimes(const std::filesystem::path& path, const NativeTimeval* times) {
#ifdef _WIN32
    const HANDLE handle = CreateFileW(path.wstring().c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return failWithLastError();
    const int result = setTimes(handle, times);
    const auto error = errno;
    CloseHandle(handle);
    errno = error;
    return result;
#else
    timeval host[2];
    if (times != nullptr) {
        for (int index = 0; index < 2; ++index) {
            host[index].tv_sec = static_cast<time_t>(times[index].seconds);
            host[index].tv_usec = static_cast<suseconds_t>(times[index].microseconds);
        }
    }
    return ::utimes(path.c_str(), times != nullptr ? host : nullptr);
#endif
}

int NativeFutimes(int descriptor, const NativeTimeval* times) {
#ifdef _WIN32
    const HANDLE handle = nativeHandle(descriptor);
    if (handle == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    return setTimes(handle, times);
#else
    timeval host[2];
    if (times != nullptr) {
        for (int index = 0; index < 2; ++index) {
            host[index].tv_sec = static_cast<time_t>(times[index].seconds);
            host[index].tv_usec = static_cast<suseconds_t>(times[index].microseconds);
        }
    }
    return ::futimes(descriptor, times != nullptr ? host : nullptr);
#endif
}

void NativeSync() {
#ifdef _WIN32
    ::_flushall();
#else
    ::sync();
#endif
}

bool NativeListDirectory(int descriptor, std::vector<NativeDirectoryEntry>& entries) {
    entries.clear();
#ifdef _WIN32
    const HANDLE handle = nativeHandle(descriptor);
    if (handle == INVALID_HANDLE_VALUE) { errno = EBADF; return false; }
    std::vector<std::byte> buffer(64 * 1024);
    auto information = FileIdBothDirectoryRestartInfo;
    for (;;) {
        if (!GetFileInformationByHandleEx(handle, information, buffer.data(), static_cast<DWORD>(buffer.size()))) {
            if (GetLastError() == ERROR_NO_MORE_FILES) return true;
            failWithLastError();
            return false;
        }
        information = FileIdBothDirectoryInfo;
        for (std::size_t offset = 0;;) {
            const auto* entry = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(buffer.data() + offset);
            const std::wstring wide(entry->FileName, entry->FileNameLength / sizeof(wchar_t));
            const auto name = std::filesystem::path(wide).u8string();
            const auto fileId = static_cast<std::uint64_t>(entry->FileId.QuadPart);
            const auto fileno = static_cast<std::uint32_t>(fileId ^ (fileId >> 32u));
            entries.push_back({fileno != 0 ? fileno : static_cast<std::uint32_t>(std::hash<std::wstring>{}(wide) | 1u), static_cast<std::uint8_t>((entry->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? 4 : 8), std::string(name.begin(), name.end())});
            if (entry->NextEntryOffset == 0) break;
            offset += entry->NextEntryOffset;
        }
    }
#else
    const int duplicate = ::dup(descriptor);
    if (duplicate < 0) return false;
    DIR* stream = ::fdopendir(duplicate);
    if (stream == nullptr) {
        const auto error = errno;
        ::close(duplicate);
        errno = error;
        return false;
    }
    ::rewinddir(stream);
    for (;;) {
        errno = 0;
        const dirent* entry = ::readdir(stream);
        if (entry == nullptr) break;
        entries.push_back({static_cast<std::uint32_t>(entry->d_ino), static_cast<std::uint8_t>(entry->d_type), entry->d_name});
    }
    const auto error = errno;
    ::closedir(stream);
    errno = error;
    return error == 0;
#endif
}

}

#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_NATIVEFILE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_NATIVEFILE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace File {

struct NativeTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

struct NativeDirectoryEntry {
    std::uint32_t fileno;
    std::uint8_t type;
    std::string name;
};

int NativeOpenRead(const std::filesystem::path& path);
std::int64_t NativePread(int descriptor, void* buffer, std::size_t bytes, std::int64_t offset);
std::int64_t NativePwrite(int descriptor, const void* buffer, std::size_t bytes, std::int64_t offset);
std::int64_t NativeLseek(int descriptor, std::int64_t offset, int whence);
int NativeFsync(int descriptor);
int NativeFtruncate(int descriptor, std::int64_t length);
int NativeTruncate(const std::filesystem::path& path, std::int64_t length);
int NativeChmod(const std::filesystem::path& path, int mode);
int NativeFchmod(int descriptor, int mode);
int NativeMkdir(const std::filesystem::path& path, int mode);
int NativeRmdir(const std::filesystem::path& path);
int NativeRename(const std::filesystem::path& from, const std::filesystem::path& to);
int NativeFlock(int descriptor, int operation);
int NativeUtimes(const std::filesystem::path& path, const NativeTimeval* times);
int NativeFutimes(int descriptor, const NativeTimeval* times);
void NativeSync();
bool NativeListDirectory(int descriptor, std::vector<NativeDirectoryEntry>& entries);

}

#endif

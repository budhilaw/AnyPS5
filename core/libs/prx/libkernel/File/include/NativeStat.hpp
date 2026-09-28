#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_NATIVESTAT_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_NATIVESTAT_HPP

#include <filesystem>
#include "SceTypes.hpp"

namespace File {

// Fills the guest stat buffer; returns 0 or the host errno of a failed stat (ENOENT for a missing path).
int FillFileStat(const std::filesystem::path& nativePath, FileStat* sb);
// Same for an open host descriptor; returns 0 or the host errno (EBADF for an invalid descriptor).
int FillDescriptorStat(int fd, FileStat* sb);

}

#endif

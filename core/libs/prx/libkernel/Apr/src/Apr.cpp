#include "prx/libkernel/Apr/include/Apr.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/GuestBufferAccess.hpp"
#include "prx/libkernel/File/include/NativeFile.hpp"
#include <atomic>
#include <cstdlib>

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {

constexpr int SCE_KERNEL_ERROR_ENOENT = static_cast<int>(0x80020002);
constexpr int SCE_KERNEL_ERROR_EIO = static_cast<int>(0x80020005);
constexpr int SCE_KERNEL_ERROR_ENOMEM = static_cast<int>(0x8002000C);
constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000E);
constexpr int SCE_KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010);
constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
constexpr int SCE_KERNEL_ERROR_EPERM = static_cast<int>(0x80020001);

constexpr std::size_t HeaderTypeOffset = 0x00;
constexpr std::size_t HeaderWriteOffset = 0x04;
constexpr std::size_t HeaderCountOffset = 0x08;
constexpr std::size_t HeaderSizeOffset = 0x0c;
constexpr std::size_t HeaderBufferOffset = 0x10;
constexpr std::size_t HeaderSize = 0x18;
constexpr std::uint32_t MaximumBufferSize = 64u << 20;
constexpr std::uint8_t ReadFileOpcode = 0x17;
constexpr std::uint32_t ReadFileRecordSize = 0x14;
constexpr std::uint32_t ReadFileRecordSizeExtended = 0x18;
constexpr std::uint32_t InvalidFileId = 0xffffffffu;

struct ReadCommand {
    std::uint32_t recordOffset;
    std::uint32_t fileId;
    void* destination;
    std::uint64_t size;
    std::uint64_t fileOffset;
};

struct CommandBufferState {
    std::byte* buffer = nullptr;
    std::uint32_t size = 0;
    std::uint32_t writeOffset = 0;
    std::vector<ReadCommand> reads;
};

struct ResolvedFile {
    std::filesystem::path path;
    std::uint64_t size = 0;
    int descriptor = -1;
};

struct SubmissionResult {
    std::int32_t result;
    std::uint32_t errorOffset;
};

std::mutex commandMutex;
std::unordered_map<void*, CommandBufferState> commandBuffers;

std::mutex fileMutex;
std::unordered_map<std::uint32_t, ResolvedFile> files;
std::unordered_map<std::string, std::uint32_t> fileIds;
std::uint32_t nextFileId = 1;

std::mutex submissionMutex;
std::unordered_set<std::uint32_t> completedSubmissions;
std::uint32_t nextSubmissionId = 1;

template<typename TValue>
void writeHeader(void* commandBuffer, std::size_t offset, TValue value) {
    std::memcpy(static_cast<std::byte*>(commandBuffer) + offset, &value, sizeof(value));
}

int syscallFailure(int kernelError) {
    *__error_nid_postfix() = kernelError & 0xff;
    return -1;
}

int readIntoGuest(ResolvedFile& file, const ReadCommand& command) {
    if (file.descriptor < 0) {
        file.descriptor = File::NativeOpenRead(file.path);
        if (file.descriptor < 0) return errno == ENOENT ? SCE_KERNEL_ERROR_ENOENT : SCE_KERNEL_ERROR_EIO;
    }
    auto* cursor = static_cast<std::byte*>(command.destination);
    std::uint64_t remaining = command.size;
    PrepareGuestBuffer(command.destination, static_cast<std::size_t>(command.size), true);
    std::uint64_t offset = command.fileOffset;
    while (remaining > 0) {
        const auto chunk = remaining > (1u << 30) ? static_cast<std::size_t>(1u << 30) : static_cast<std::size_t>(remaining);
        const auto got = File::NativePread(file.descriptor, cursor, chunk, static_cast<std::int64_t>(offset));
        if (got < 0) {
            if (errno == EINTR) continue;
            APS5_LOG_OUT("APR read of %s (%llu bytes at %llu into %p) failed: errno %d", file.path.string().c_str(), static_cast<unsigned long long>(command.size), static_cast<unsigned long long>(command.fileOffset), command.destination, errno);
            return SCE_KERNEL_ERROR_EIO;
        }
        if (got == 0) break;
        cursor += got;
        remaining -= static_cast<std::uint64_t>(got);
        offset += static_cast<std::uint64_t>(got);
    }
    return 0;
}

int execute(void* commandBuffer, SubmissionResult& result) {
    std::vector<ReadCommand> reads;
    {
        std::lock_guard lock(commandMutex);
        const auto it = commandBuffers.find(commandBuffer);
        if (it == commandBuffers.end() || it->second.buffer == nullptr) return SCE_KERNEL_ERROR_EINVAL;
        reads = it->second.reads;
    }
    result = {0, 0};
    for (const auto& command : reads) {
        std::lock_guard lock(fileMutex);
        const auto it = files.find(command.fileId);
        if (it == files.end()) {
            result = {SCE_KERNEL_ERROR_ENOENT, command.recordOffset};
            return 0;
        }
        const int error = readIntoGuest(it->second, command);
        if (error != 0) {
            result = {error, command.recordOffset};
            return 0;
        }
    }
    return 0;
}

}

extern "C" {

int APS5_VABI AprCommandBufferConstruct(void* commandBuffer) {
    if (commandBuffer == nullptr) return 0;
    std::memset(commandBuffer, 0, HeaderSize);
    std::lock_guard lock(commandMutex);
    commandBuffers.erase(commandBuffer);
    return 0;
}

int APS5_VABI AprCommandBufferSetBuffer(void* commandBuffer, void* buffer, std::uint32_t size) {
    if (commandBuffer == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    if (buffer == nullptr || (reinterpret_cast<std::uintptr_t>(buffer) & 3u) != 0 || size == 0 || size > MaximumBufferSize || (size & 3u) != 0) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(commandMutex);
    auto& state = commandBuffers[commandBuffer];
    if (state.buffer != nullptr) return SCE_KERNEL_ERROR_EBUSY;
    state.buffer = static_cast<std::byte*>(buffer);
    state.size = size;
    state.writeOffset = 0;
    state.reads.clear();
    writeHeader(commandBuffer, HeaderBufferOffset, buffer);
    writeHeader(commandBuffer, HeaderSizeOffset, size);
    writeHeader(commandBuffer, HeaderWriteOffset, std::uint32_t{0});
    writeHeader(commandBuffer, HeaderCountOffset, std::int32_t{0});
    return 0;
}

int APS5_VABI AprCommandBufferReset(void* commandBuffer) {
    if (commandBuffer == nullptr) return SCE_KERNEL_ERROR_EPERM;
    std::lock_guard lock(commandMutex);
    const auto it = commandBuffers.find(commandBuffer);
    if (it == commandBuffers.end() || it->second.buffer == nullptr) return SCE_KERNEL_ERROR_EPERM;
    it->second.writeOffset = 0;
    it->second.reads.clear();
    writeHeader(commandBuffer, HeaderWriteOffset, std::uint32_t{0});
    writeHeader(commandBuffer, HeaderCountOffset, std::int32_t{0});
    return 0;
}

int APS5_VABI AprCommandBufferAppendRead(void* commandBuffer, std::uint32_t fileId, void* destination, std::uint64_t size, std::uint64_t fileOffset) {
    if (commandBuffer == nullptr || destination == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(commandMutex);
    const auto it = commandBuffers.find(commandBuffer);
    if (it == commandBuffers.end() || it->second.buffer == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    auto& state = it->second;
    const auto recordSize = (fileOffset >> 32u) != 0 ? ReadFileRecordSizeExtended : ReadFileRecordSize;
    if (state.size - state.writeOffset < recordSize) return SCE_KERNEL_ERROR_ENOMEM;
    std::memset(state.buffer + state.writeOffset, 0, recordSize);
    state.buffer[state.writeOffset] = static_cast<std::byte>(ReadFileOpcode);
    state.reads.push_back({state.writeOffset, fileId, destination, size, fileOffset});
    state.writeOffset += recordSize;
    writeHeader(commandBuffer, HeaderWriteOffset, state.writeOffset);
    writeHeader(commandBuffer, HeaderCountOffset, static_cast<std::int32_t>(state.reads.size()));
    return 0;
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizes(const char* const* path_list, uint32_t count, uint32_t* ids, uint64_t* sizes, uint32_t* error_index) {
    if (path_list == nullptr || count == 0 || count > 1024 || (ids == nullptr && sizes == nullptr)) return syscallFailure(SCE_KERNEL_ERROR_EINVAL);
    for (uint32_t i = 0; i < count; ++i) {
        if (path_list[i] == nullptr) {
            if (error_index != nullptr) *error_index = i;
            return syscallFailure(SCE_KERNEL_ERROR_EFAULT);
        }
        const std::string guestPath(path_list[i]);
        const auto native = ResolvePath_nid_no_patch(path_list[i]);
        std::error_code error;
        const auto status = std::filesystem::status(native, error);
        if (error || !std::filesystem::exists(status)) {
            if (ids != nullptr) ids[i] = InvalidFileId;
            if (sizes != nullptr) sizes[i] = 0;
            if (error_index != nullptr) *error_index = i;
            return syscallFailure(SCE_KERNEL_ERROR_ENOENT);
        }
        const bool directory = std::filesystem::is_directory(status);
        const auto size = directory ? std::uintmax_t{0} : std::filesystem::file_size(native, error);
        if (error) throw std::runtime_error("sceKernelAprResolveFilepathsToIdsAndFileSizes: cannot size " + native.string() + ": " + error.message());
        std::lock_guard lock(fileMutex);
        auto idIt = fileIds.find(guestPath);
        if (idIt == fileIds.end()) {
            const auto id = nextFileId++;
            files[id] = ResolvedFile{native, static_cast<std::uint64_t>(size), -1};
            idIt = fileIds.emplace(guestPath, id).first;
        } else {
            files[idIt->second].size = static_cast<std::uint64_t>(size);
        }
        if (ids != nullptr) ids[i] = idIt->second;
        if (sizes != nullptr) sizes[i] = static_cast<std::uint64_t>(size);
    }
    return 0;
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetResult(void* command_buffer, uint64_t flags, SubmissionResult* result, uint32_t* out_submission_id) {
    if (command_buffer == nullptr) return syscallFailure(SCE_KERNEL_ERROR_EINVAL);
    SubmissionResult executed{};
    const int error = execute(command_buffer, executed);
    if (error != 0) return syscallFailure(error);
    std::uint32_t id;
    {
        std::lock_guard lock(submissionMutex);
        id = nextSubmissionId++;
        if (nextSubmissionId == 0) nextSubmissionId = 1;
        completedSubmissions.insert(id);
    }
    if (out_submission_id != nullptr) *out_submission_id = id;
    if (result != nullptr) *result = executed;
    {
        static const bool trace = std::getenv("ANYPS5_TRACE_IO") != nullptr;
        static std::atomic<std::uint64_t> submissions{0};
        const auto count = ++submissions;
        if (trace && ((count & (count - 1)) == 0 || executed.result != 0)) APS5_LOG_OUT("APR submission #%llu id %u flags 0x%llx: result 0x%x at record %u", static_cast<unsigned long long>(count), id, static_cast<unsigned long long>(flags), static_cast<unsigned>(executed.result), executed.errorOffset);
    }
    return 0;
}

int APS5_VABI sceKernelAprWaitCommandBuffer(uint32_t submission_id) {
    std::lock_guard lock(submissionMutex);
    const auto it = completedSubmissions.find(submission_id);
    if (it == completedSubmissions.end()) return syscallFailure(static_cast<int>(0x80020003));
    completedSubmissions.erase(it);
    return 0;
}

}

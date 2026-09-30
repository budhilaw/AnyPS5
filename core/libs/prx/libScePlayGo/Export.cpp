#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <string>

namespace {

constexpr int SCE_PLAYGO_ERROR_BAD_CHUNK_ID = static_cast<int>(0x80b2000c);
constexpr std::uint16_t InstalledChunkCount = 1;
constexpr std::int32_t InstallSpeedSuspended = 0;
constexpr std::int32_t InstallSpeedFull = 2;
constexpr std::int8_t LocusLocalFast = 3;

std::atomic<std::int32_t> installSpeed{InstallSpeedFull};

int RequireChunks(const std::uint16_t* chunkIds, std::uint32_t count, const char* function) {
    if (chunkIds == nullptr && count != 0) throw std::invalid_argument(std::string(function) + ": chunk list is null");
    for (std::uint32_t index = 0; index < count; ++index)
        if (chunkIds[index] >= InstalledChunkCount) return SCE_PLAYGO_ERROR_BAD_CHUNK_ID;
    return 0;
}

int ListInstalledChunks(std::uint16_t* chunkIds, std::uint32_t capacity, std::uint32_t* count, const char* function) {
    if (count == nullptr) throw std::invalid_argument(std::string(function) + ": entry count is null");
    if (chunkIds == nullptr || capacity == 0) {
        *count = InstalledChunkCount;
        return 0;
    }
    const auto listed = std::min<std::uint32_t>(capacity, InstalledChunkCount);
    for (std::uint32_t index = 0; index < listed; ++index) chunkIds[index] = static_cast<std::uint16_t>(index);
    *count = listed;
    return 0;
}

}

extern "C" {

int APS5_VABI scePlayGoClose(int handle) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    return 0;
}

int APS5_VABI scePlayGoGetChunkId(int handle, uint16_t* out_chunk_id_list, uint32_t number_of_entries, uint32_t* out_entries) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    return ListInstalledChunks(out_chunk_id_list, number_of_entries, out_entries, __func__);
}

int APS5_VABI scePlayGoGetEta(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int64_t* out_eta) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    if (const auto error = RequireChunks(chunk_ids, number_of_entries, __func__); error != 0) return error;
    if (out_eta == nullptr) throw std::invalid_argument(std::string(__func__) + ": eta is null");
    *out_eta = 0;
    return 0;
}

int APS5_VABI scePlayGoGetInstallChunkId(int handle, uint16_t* out_chunk_id_list, uint32_t number_of_entries, uint32_t* out_entries) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    return ListInstalledChunks(out_chunk_id_list, number_of_entries, out_entries, __func__);
}

int APS5_VABI scePlayGoGetInstallSpeed(int handle, int32_t* out_speed) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    if (out_speed == nullptr) throw std::invalid_argument(std::string(__func__) + ": speed is null");
    *out_speed = installSpeed.load();
    return 0;
}

int APS5_VABI scePlayGoGetLanguageMask(int handle, uint64_t* out_language_mask) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    if (out_language_mask == nullptr) throw std::invalid_argument(std::string(__func__) + ": language mask is null");
    *out_language_mask = ~std::uint64_t{0};
    return 0;
}

int APS5_VABI scePlayGoGetLocus(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int8_t* out_loci) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    if (const auto error = RequireChunks(chunk_ids, number_of_entries, __func__); error != 0) return error;
    if (out_loci == nullptr) throw std::invalid_argument(std::string(__func__) + ": loci is null");
    std::fill_n(out_loci, number_of_entries, LocusLocalFast);
    return 0;
}

int APS5_VABI scePlayGoGetOptionalChunk(int handle, int32_t type, PlayGoOptionalChunk* option) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    (void)type;
    if (option == nullptr) throw std::invalid_argument(std::string(__func__) + ": option is null");
    option->bitmask = ~std::uint64_t{0};
    return 0;
}

int APS5_VABI scePlayGoGetProgress(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, PlayGoProgress* out_progress) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    if (const auto error = RequireChunks(chunk_ids, number_of_entries, __func__); error != 0) return error;
    if (out_progress == nullptr) throw std::invalid_argument(std::string(__func__) + ": progress is null");
    out_progress->progress_size = 0;
    out_progress->total_size = 0;
    return 0;
}

int APS5_VABI scePlayGoGetSupportedOptionalChunk(int handle, int32_t type, PlayGoOptionalChunk* option) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    (void)type;
    if (option == nullptr) throw std::invalid_argument(std::string(__func__) + ": option is null");
    option->bitmask = ~std::uint64_t{0};
    return 0;
}

int APS5_VABI scePlayGoGetToDoList(int handle, PlayGoToDo* out_todo_list, uint32_t number_of_entries, uint32_t* out_entries) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    (void)out_todo_list;
    (void)number_of_entries;
    if (out_entries == nullptr) throw std::invalid_argument(std::string(__func__) + ": entry count is null");
    *out_entries = 0;
    return 0;
}

int APS5_VABI scePlayGoInitialize(const PlayGoInitParams* init) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (init == nullptr || init->buf_addr == nullptr) throw std::invalid_argument(std::string(__func__) + ": missing work buffer");
    return 0;
}

int APS5_VABI scePlayGoOpen(int* out_handle, const void* param) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)param;
    if (out_handle == nullptr) throw std::invalid_argument(std::string(__func__) + ": handle is null");
    *out_handle = 1;
    return 0;
}

int APS5_VABI scePlayGoPrefetch(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int8_t minimum_locus) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    (void)minimum_locus;
    if (const auto error = RequireChunks(chunk_ids, number_of_entries, __func__); error != 0) return error;
    return 0;
}

int APS5_VABI scePlayGoPrefetchOptionalChunk(int handle, int32_t type, const PlayGoOptionalChunk* option) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    (void)type;
    if (option == nullptr) throw std::invalid_argument(std::string(__func__) + ": option is null");
    return 0;
}

int APS5_VABI scePlayGoSetInstallSpeed(int handle, int32_t speed) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)handle;
    if (speed < InstallSpeedSuspended || speed > InstallSpeedFull) throw std::invalid_argument(std::string(__func__) + ": invalid install speed");
    installSpeed.store(speed);
    return 0;
}

int APS5_VABI scePlayGoSetToDoList(int handle, const PlayGoToDo* todo_list, uint32_t number_of_entries) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)handle;
 (void)todo_list;
 (void)number_of_entries;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoTerminate(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

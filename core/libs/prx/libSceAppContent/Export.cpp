#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <system_error>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_APP_CONTENT_ERROR_PARAMETER = static_cast<int>(0x80D90002);
constexpr int SCE_APP_CONTENT_ERROR_BUSY = static_cast<int>(0x80D90003);
constexpr int SCE_APP_CONTENT_ERROR_NOT_MOUNTED = static_cast<int>(0x80D90004);
constexpr int SCE_APP_CONTENT_ERROR_NOT_FOUND = static_cast<int>(0x80D90005);
constexpr int SCE_APP_CONTENT_ERROR_MOUNT_FULL = static_cast<int>(0x80D90006);
constexpr int SCE_APP_CONTENT_ERROR_NOT_INITIALIZED = static_cast<int>(0x80D9000C);
constexpr std::uint32_t PARAM_ID_SKU_FLAG = 0, PARAM_ID_USER_DEFINED_1 = 1, PARAM_ID_USER_DEFINED_4 = 4;
constexpr std::int32_t SKU_FLAG_FULL = 3;
constexpr const char* TemporaryMount = "/temp0";
constexpr const char* DownloadMount = "/download0";

std::mutex appMutex;
bool initialized = false;
bool temporaryMounted = false;

bool ParamInteger(const char* key, std::int32_t& value) {
    std::ifstream file(ResolvePath_nid_no_patch("/app0/sce_sys/param.json"));
    if (!file) throw std::runtime_error("sceAppContent: /app0/sce_sys/param.json is unavailable");
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) return false;
    value = static_cast<std::int32_t>(std::stol(match[1].str()));
    return true;
}

}

extern "C" {

int APS5_VABI sceAppContentInitialize(const AppContentInitParam* init_param, AppContentBootParam* boot_param) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (init_param == nullptr || boot_param == nullptr) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::lock_guard lock(appMutex);
    std::memset(boot_param, 0, sizeof(*boot_param));
    boot_param->attr = 0;
    initialized = true;
    return 0;
}

int APS5_VABI sceAppContentAppParamGetInt(uint32_t param_id, int32_t* value) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (value == nullptr) return SCE_APP_CONTENT_ERROR_PARAMETER;
    {
        std::lock_guard lock(appMutex);
        if (!initialized) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    }
    if (param_id == PARAM_ID_SKU_FLAG) { *value = SKU_FLAG_FULL; return 0; }
    if (param_id >= PARAM_ID_USER_DEFINED_1 && param_id <= PARAM_ID_USER_DEFINED_4) {
        const std::string key = "userDefinedParam" + std::to_string(param_id);
        std::int32_t parsed = 0;
        if (!ParamInteger(key.c_str(), parsed)) return SCE_APP_CONTENT_ERROR_NOT_FOUND;
        *value = parsed;
        return 0;
    }
    return SCE_APP_CONTENT_ERROR_PARAMETER;
}

int APS5_VABI sceAppContentTemporaryDataMount2(uint32_t option, AppContentMountPoint* mount_point) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (mount_point == nullptr || option > 1) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::lock_guard lock(appMutex);
    if (!initialized) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (temporaryMounted) return SCE_APP_CONTENT_ERROR_BUSY;
    const auto native = ResolvePath_nid_no_patch(TemporaryMount);
    std::error_code error;
    if (option == 1) std::filesystem::remove_all(native, error);
    std::filesystem::create_directories(native, error);
    if (error) throw std::runtime_error("sceAppContentTemporaryDataMount2: cannot create " + native.string());
    std::memset(mount_point, 0, sizeof(*mount_point));
    std::strncpy(mount_point->data, TemporaryMount, sizeof(mount_point->data) - 1);
    temporaryMounted = true;
    return 0;
}

int APS5_VABI sceAppContentTemporaryDataFormat(const AppContentMountPoint* mount_point) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (mount_point == nullptr || std::strncmp(mount_point->data, TemporaryMount, sizeof(mount_point->data)) != 0) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::lock_guard lock(appMutex);
    if (!temporaryMounted) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    const auto native = ResolvePath_nid_no_patch(TemporaryMount);
    std::error_code error;
    std::filesystem::remove_all(native, error);
    std::filesystem::create_directories(native, error);
    if (error) throw std::runtime_error("sceAppContentTemporaryDataFormat: cannot recreate " + native.string());
    return 0;
}

int APS5_VABI sceAppContentTemporaryDataGetAvailableSpaceKb(const AppContentMountPoint* mount_point, size_t* available_space_kb) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (mount_point == nullptr || available_space_kb == nullptr || std::strncmp(mount_point->data, TemporaryMount, sizeof(mount_point->data)) != 0) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::lock_guard lock(appMutex);
    if (!temporaryMounted) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    std::error_code error;
    const auto space = std::filesystem::space(ResolvePath_nid_no_patch(TemporaryMount), error);
    if (error) throw std::runtime_error("sceAppContentTemporaryDataGetAvailableSpaceKb: " + error.message());
    *available_space_kb = static_cast<size_t>(space.available / 1024);
    return 0;
}

int APS5_VABI sceAppContentDownloadDataGetAvailableSpaceKb(const AppContentMountPoint* mount_point, size_t* available_space_kb) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (mount_point == nullptr || available_space_kb == nullptr || std::strncmp(mount_point->data, DownloadMount, sizeof(mount_point->data)) != 0) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::int32_t quotaMegabytes = 0;
    if (!ParamInteger("downloadDataSize", quotaMegabytes) || quotaMegabytes <= 0) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    const auto native = ResolvePath_nid_no_patch(DownloadMount);
    std::error_code error;
    std::filesystem::create_directories(native, error);
    if (error) throw std::runtime_error("sceAppContentDownloadDataGetAvailableSpaceKb: cannot create " + native.string());
    std::uintmax_t used = 0;
    for (auto it = std::filesystem::recursive_directory_iterator(native, error); !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
        if (it->is_regular_file(error)) used += it->file_size(error);
    }
    const auto quota = static_cast<std::uintmax_t>(quotaMegabytes) * 1024;
    const auto usedKb = (used + 1023) / 1024;
    *available_space_kb = static_cast<size_t>(usedKb >= quota ? 0 : quota - usedKb);
    return 0;
}

int APS5_VABI sceAppContentAddcontMount(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, AppContentMountPoint* mount_point) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)service_label;
    if (entitlement_label == nullptr || mount_point == nullptr) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentAddcontUnmount(const AppContentMountPoint* mount_point) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (mount_point == nullptr) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
}

}

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// The share service (screenshots, video clips, the share menu) has no host counterpart: the
// library initializes, accepts its configuration, and reports that no capture is ever running.
namespace {

constexpr int SCE_SHARE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80A50002);
constexpr int SCE_SHARE_ERROR_NOT_INITIALIZED = static_cast<int>(0x80A50004);
constexpr int SCE_SHARE_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80A50005);
constexpr int SCE_SHARE_ERROR_NOT_AVAILABLE = static_cast<int>(0x80A5000A);

std::mutex mutex;
bool initialized = false;
void* contentCallback = nullptr;
std::uint32_t prohibited = 0;

}

extern "C" {

int APS5_VABI sceShareInitialize(size_t heap_size, int thread_priority, uint64_t affinity_mask) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)thread_priority; (void)affinity_mask;
    if (heap_size == 0) return SCE_SHARE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(mutex);
    if (initialized) return SCE_SHARE_ERROR_ALREADY_INITIALIZED;
    initialized = true;
    return 0;
}

int APS5_VABI sceShareTerminate(void) {
    Aps5TraceCall_nid_no_patch(__func__);
    std::lock_guard lock(mutex);
    if (!initialized) return SCE_SHARE_ERROR_NOT_INITIALIZED;
    initialized = false;
    contentCallback = nullptr;
    prohibited = 0;
    return 0;
}

int APS5_VABI sceShareFeaturePermit(uint32_t feature_flags) {
    Aps5TraceCall_nid_no_patch(__func__);
    std::lock_guard lock(mutex);
    if (!initialized) return SCE_SHARE_ERROR_NOT_INITIALIZED;
    prohibited &= ~feature_flags;
    return 0;
}

int APS5_VABI sceShareFeatureProhibit(uint32_t feature_flags) {
    Aps5TraceCall_nid_no_patch(__func__);
    std::lock_guard lock(mutex);
    if (!initialized) return SCE_SHARE_ERROR_NOT_INITIALIZED;
    prohibited |= feature_flags;
    return 0;
}

int APS5_VABI sceShareGetCurrentStatus(uint32_t feature_flag, ShareCurrentStatus* status) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)feature_flag;
    if (status == nullptr) return SCE_SHARE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(mutex);
    if (!initialized) return SCE_SHARE_ERROR_NOT_INITIALIZED;
    std::memset(status, 0, sizeof(*status)); // nothing is being captured or shared
    return 0;
}

int APS5_VABI sceShareGetRunningStatus_nid_postfix(uint32_t feature_flag, ShareCurrentStatus* status) { return sceShareGetCurrentStatus(feature_flag, status); }

int APS5_VABI sceShareRegisterContentEventCallback(void* callback, void* user_data) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)user_data;
    if (callback == nullptr) return SCE_SHARE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(mutex);
    if (!initialized) return SCE_SHARE_ERROR_NOT_INITIALIZED;
    contentCallback = callback;
    return 0;
}

int APS5_VABI sceShareUnregisterContentEventCallback(void* callback) {
    Aps5TraceCall_nid_no_patch(__func__);
    std::lock_guard lock(mutex);
    if (!initialized) return SCE_SHARE_ERROR_NOT_INITIALIZED;
    if (callback != nullptr && callback != contentCallback) return SCE_SHARE_ERROR_INVALID_ARGUMENT;
    contentCallback = nullptr;
    return 0;
}

int APS5_VABI sceShareSetCaptureSource(uint32_t feature_flags, const void* tap_point) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)feature_flags;
    if (tap_point == nullptr) return SCE_SHARE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(mutex);
    return initialized ? 0 : SCE_SHARE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceShareSetContentParam(const char* content_param) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (content_param == nullptr) return SCE_SHARE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(mutex);
    return initialized ? 0 : SCE_SHARE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceShareSetContentParamForApplicationTitle_nid_postfix(const char* content_param) { return sceShareSetContentParam(content_param); }

int APS5_VABI sceShareSetScreenshotOverlayImage(const char* file_path, int32_t margin_x, int32_t margin_y, int32_t origin) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)margin_x; (void)margin_y; (void)origin;
    if (file_path == nullptr) return SCE_SHARE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(mutex);
    return initialized ? 0 : SCE_SHARE_ERROR_NOT_INITIALIZED;
}

// Captures and the share menu are system features the host does not provide.
int APS5_VABI sceShareCaptureScreenshot(const void* param, int32_t* req_id) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (param == nullptr || req_id == nullptr) return SCE_SHARE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(mutex);
    return initialized ? SCE_SHARE_ERROR_NOT_AVAILABLE : SCE_SHARE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceShareCaptureScreenshotExtended_nid_postfix(const void* param, int32_t* req_id) { return sceShareCaptureScreenshot(param, req_id); }

int APS5_VABI sceShareCaptureVideoClip(const void* param, int32_t* req_id) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (param == nullptr || req_id == nullptr) return SCE_SHARE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(mutex);
    return initialized ? SCE_SHARE_ERROR_NOT_AVAILABLE : SCE_SHARE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceShareCaptureVideoClipExtended_nid_postfix(const void* param, int32_t* req_id) { return sceShareCaptureVideoClip(param, req_id); }

int APS5_VABI sceShareOpenMenuForContent(const void* content_id) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (content_id == nullptr) return SCE_SHARE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(mutex);
    return initialized ? SCE_SHARE_ERROR_NOT_AVAILABLE : SCE_SHARE_ERROR_NOT_INITIALIZED;
}

}

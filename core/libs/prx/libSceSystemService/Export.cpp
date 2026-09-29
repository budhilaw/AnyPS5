#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include "prx/libc/include/Shutdown.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceSystemService/SystemService.hpp"

namespace {
bool noticeScreenSkip = false;
}

extern "C" {

int APS5_VABI sceSystemServiceLoadExec(const char* path, const char* const* arguments) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (!path || !*path) return SYSTEM_SERVICE_ERROR_PARAMETER;
    if (std::strcmp(path, "exit") != 0) {
        NotImplemented_nid_no_patch("sceSystemServiceLoadExec: executable replacement");
    }
    (void)arguments;
    LibcRunShutdown_nid_postfix();
    std::exit(0);
}

int APS5_VABI sceSystemServiceDisableNoticeScreenSkipFlagAutoSet(void) {
    Aps5TraceCall_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetDisplaySafeAreaInfo(SystemServiceDisplaySafeAreaInfo* info) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (info == nullptr) return SYSTEM_SERVICE_ERROR_PARAMETER;
    *info = SystemServiceDisplaySafeAreaInfo{};
    info->ratio = 1.0f;
    return 0;
}

int APS5_VABI sceSystemServiceGetHdrToneMapLuminance(SystemServiceHdrToneMapLuminance* luminance) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (luminance == nullptr) return SYSTEM_SERVICE_ERROR_PARAMETER;
    luminance->max_full_frame_tone_map_luminance = 100.0f;
    luminance->max_tone_map_luminance = 100.0f;
    luminance->min_tone_map_luminance = 0.0f;
    return 0;
}

int APS5_VABI sceSystemServiceGetNoticeScreenSkipFlag(bool* value) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (value == nullptr) return SYSTEM_SERVICE_ERROR_PARAMETER;
    *value = noticeScreenSkip;
    return 0;
}

int APS5_VABI sceSystemServiceGetStatus(SystemServiceStatus* status) {
    Aps5TraceCall_nid_no_patch(__func__);
 if (status == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 *status = SystemServiceStatus{};
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceHideSplashScreen(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceParamGetInt(int paramId, int* value) {
    Aps5TraceCall_nid_no_patch(__func__);
 if (value == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 switch (paramId) {
  case SYSTEM_SERVICE_PARAM_ID_LANG: *value = SYSTEM_SERVICE_PARAM_LANG_ENGLISH_US; break;
  case SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT: *value = SYSTEM_SERVICE_PARAM_DATE_FORMAT_DDMMYYYY; break;
  case SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT: *value = SYSTEM_SERVICE_PARAM_TIME_FORMAT_24HOUR; break;
  case SYSTEM_SERVICE_PARAM_ID_TIME_ZONE: *value = 0; break;
  case SYSTEM_SERVICE_PARAM_ID_SUMMERTIME: *value = 0; break;
  case SYSTEM_SERVICE_PARAM_ID_GAME_PARENTAL_LEVEL: *value = SYSTEM_SERVICE_PARAM_GAME_PARENTAL_OFF; break;
  case SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN: *value = SYSTEM_SERVICE_PARAM_ENTER_BUTTON_CROSS; break;
  default: *value = 0; break;
 }
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceParamGetString(int param_id, char* buf, size_t buf_size) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)param_id;
 (void)buf;
 (void)buf_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemServicePowerTick(void) {
    Aps5TraceCall_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceReceiveEvent(SystemServiceEvent* event) {
    Aps5TraceCall_nid_no_patch(__func__);
 if (event == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 event->event_type = -1;
 std::memset(event->data, 0, sizeof(event->data));
 return SYSTEM_SERVICE_ERROR_NO_EVENT;
}

int APS5_VABI sceSystemServiceReportAbnormalTermination(const void* info) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)info;
    std::fprintf(stderr, "sceSystemServiceReportAbnormalTermination: title reported an abnormal termination\n");
    return 0;
}

int APS5_VABI sceSystemServiceSetNoticeScreenSkipFlag(void) {
    Aps5TraceCall_nid_no_patch(__func__);
    noticeScreenSkip = true;
    return 0;
}

}

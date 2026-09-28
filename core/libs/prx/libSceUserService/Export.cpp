#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceUserService/UserService.hpp"

extern "C" {

int APS5_VABI sceUserServiceGetAccessibilityChatTranscription(int user_id, int32_t* chat_transcription) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (user_id != USER_SERVICE_INITIAL_USER_ID) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    if (chat_transcription == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    *chat_transcription = 0; // the console default for this accessibility setting
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityPressAndHoldDelay(int user_id, int32_t* press_and_hold_delay) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (user_id != USER_SERVICE_INITIAL_USER_ID) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    if (press_and_hold_delay == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    *press_and_hold_delay = 0; // the console default for this accessibility setting
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityTriggerEffect(int user_id, int32_t* trigger_effect) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (user_id != USER_SERVICE_INITIAL_USER_ID) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    if (trigger_effect == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    *trigger_effect = 0; // the console default for this accessibility setting
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityVibration(int user_id, int32_t* vibration) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (user_id != USER_SERVICE_INITIAL_USER_ID) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    if (vibration == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    *vibration = 0; // the console default for this accessibility setting
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityZoomEnabled(int user_id, int32_t* zoom_enabled) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (user_id != USER_SERVICE_INITIAL_USER_ID) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    if (zoom_enabled == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    *zoom_enabled = 0; // the console default for this accessibility setting
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAgeLevel(int user_id, uint32_t* age_level) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (user_id != USER_SERVICE_INITIAL_USER_ID) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    if (age_level == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    *age_level = 30; // an adult account; no parental restrictions apply on the host
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetEvent(SceUserServiceEvent* event) {
    Aps5TraceCall_nid_no_patch(__func__);
    // The single user is logged in before the title starts and never logs out; no events arise.
    if (event == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    return USER_SERVICE_ERROR_NO_EVENT;
}

int APS5_VABI sceUserServiceGetGamePresets(int user_id, UserServiceGamePresets* presets) {
    Aps5TraceCall_nid_no_patch(__func__);
    // Per-user game presets; a user who never chose any reports "use the game's default" (0) for each.
    if (user_id != USER_SERVICE_INITIAL_USER_ID) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    if (presets == nullptr || presets->this_size != sizeof(UserServiceGamePresets)) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    const auto size = presets->this_size;
    std::memset(presets, 0, sizeof(UserServiceGamePresets));
    presets->this_size = size;
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetInitialUser(int* user_id) {
    Aps5TraceCall_nid_no_patch(__func__);
 if (user_id == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *user_id = USER_SERVICE_INITIAL_USER_ID;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* user_id_list) {
    Aps5TraceCall_nid_no_patch(__func__);
 if (user_id_list == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 user_id_list->user_id[0] = USER_SERVICE_INITIAL_USER_ID;
 user_id_list->user_id[1] = USER_SERVICE_USER_ID_INVALID;
 user_id_list->user_id[2] = USER_SERVICE_USER_ID_INVALID;
 user_id_list->user_id[3] = USER_SERVICE_USER_ID_INVALID;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetUserName(int user_id, char* name, size_t size) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (name == nullptr || size == 0) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    if (user_id != USER_SERVICE_INITIAL_USER_ID) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    static constexpr char UserName[] = "Player";
    if (size < sizeof(UserName)) return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    std::memcpy(name, UserName, sizeof(UserName));
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetUserNumber(int user_id, int32_t* number) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (number == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    if (user_id != USER_SERVICE_INITIAL_USER_ID) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    *number = 1; // the only local user
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceInitialize(const void* params) {
    Aps5TraceCall_nid_no_patch(__func__);
 (void)params;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceInitialize2(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceTerminate(void) {
    Aps5TraceCall_nid_no_patch(__func__);
 return USER_SERVICE_OK;
}

}

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <thread>

#include "SceTypes.hpp"
#include "prx//libc/include/General.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

extern "C" {
int APS5_VABI scePadReadState(int handle, PadData* data);

int APS5_VABI scePadClose_nid_postfix(int handle) {
 if (handle != PAD_HANDLE) {
  return PAD_ERROR_INVALID_HANDLE;
 }
 return PAD_OK;
}

int APS5_VABI scePadDeviceClassGetExtendedInformation(int handle, PadDeviceClassExtendedInformation* info) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    if (info == nullptr) return PAD_ERROR_INVALID_ARG;
    std::memset(info, 0, sizeof(*info));
    info->deviceClass = PAD_DEVICE_CLASS_STANDARD; // a standard controller carries no class-specific data
    return PAD_OK;
}

int APS5_VABI scePadDeviceClassParseData(int handle, const PadData* data, PadDeviceClassData* class_data) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    if (data == nullptr || class_data == nullptr) return PAD_ERROR_INVALID_ARG;
    std::memset(class_data, 0, sizeof(*class_data));
    class_data->deviceClass = PAD_DEVICE_CLASS_STANDARD;
    class_data->dataValid = false; // standard controllers have no device-class payload
    return PAD_OK;
}

int APS5_VABI scePadGetControllerInformation(int handle, PadControllerInformation* info) {
 if (handle != PAD_HANDLE) {
  return PAD_ERROR_INVALID_HANDLE;
 }
 if (info == nullptr) {
  return PAD_ERROR_INVALID_ARG;
 }
 std::memset(info, 0, sizeof(*info));
 info->touchPadInfo.pixelDensity = 44.86f;
 info->touchPadInfo.resolution.x = 1920;
 info->touchPadInfo.resolution.y = 943;
 info->stickInfo.deadZoneLeft = 2;
 info->stickInfo.deadZoneRight = 2;
 info->connectionType = PAD_CONNECTION_TYPE_LOCAL;
 info->connectedCount = 1;
 info->connected = true;
 info->deviceClass = PAD_DEVICE_CLASS_STANDARD;
 return PAD_OK;
}

int APS5_VABI scePadGetHandle(int user_id, int type, int index) {
    // The single controller opened by scePadOpen: personal ports of the local user, index 0.
    if (index != 0) return PAD_ERROR_INVALID_ARG;
    const bool personalPort = (type == PAD_PORT_TYPE_STANDARD || type == PAD_PORT_TYPE_SPECIAL);
    const bool systemRemote = (user_id == PAD_USER_ID_SYSTEM && type == PAD_PORT_TYPE_REMOTE);
    if (!personalPort && !systemRemote) return PAD_ERROR_INVALID_ARG;
    return PAD_HANDLE;
}

int APS5_VABI scePadGetTriggerEffectState(int handle, PadTriggerEffectStateInformation* info) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    if (info == nullptr) return PAD_ERROR_INVALID_ARG;
    std::memset(info, 0, sizeof(*info)); // no trigger effect is active on the host's controller
    return PAD_OK;
}

int APS5_VABI scePadInit_nid_postfix(void) {
 Pad::Initialize();
 return PAD_OK;
}

int APS5_VABI scePadOpen_nid_postfix(int userId, int type, int index, const void* param) {
 (void)param;
 if (index != 0) {
  return PAD_ERROR_INVALID_ARG;
 }
 const bool personalPort = (type == PAD_PORT_TYPE_STANDARD || type == PAD_PORT_TYPE_SPECIAL);
 const bool systemRemote = (userId == PAD_USER_ID_SYSTEM && type == PAD_PORT_TYPE_REMOTE);
 if (!personalPort && !systemRemote) {
  return PAD_ERROR_INVALID_ARG;
 }
 return PAD_HANDLE;
}

int APS5_VABI scePadRead_nid_postfix(int handle, PadData* data, int num) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    if (data == nullptr || num <= 0) return PAD_ERROR_INVALID_ARG;
    // The host samples input on demand, so the buffered read yields the current state once.
    const int result = scePadReadState(handle, data);
    return result == PAD_OK ? 1 : result;
}

int APS5_VABI scePadReadState(int handle, PadData* data) {
 if (handle != 1) APS5_INVALID_ARG_EX;
 if (data == nullptr) APS5_INVALID_ARG_EX;

 *data = Pad::ReadState();

 return 0;
}

int APS5_VABI scePadResetLightBar(int handle) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    return PAD_OK;
}

int APS5_VABI scePadResetOrientation(int handle) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    return PAD_OK;
}

int APS5_VABI scePadSetAngularVelocityDeadbandState(int handle, bool enable) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    return PAD_OK; // motion sensor processing settings; the host controller reports no motion data
}

int APS5_VABI scePadSetLightBar(int handle, const PadLightBarParam* param) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    if (param == nullptr) return PAD_ERROR_INVALID_ARG;
    // Output to the controller (light bar, rumble, adaptive triggers) has no host device to reach yet; the request is accepted.
    return PAD_OK;
}

int APS5_VABI scePadSetMotionSensorState(int handle, bool enable) {
 (void)handle;
 (void)enable;
 // if (enable) {
 //  throw std::runtime_error("scePadSetMotionSensorState: motion sensor not supported");
 // }
 return PAD_OK;
}

int APS5_VABI scePadSetTiltCorrectionState(int handle, bool enabled) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    return PAD_OK; // motion sensor processing settings; the host controller reports no motion data
}

int APS5_VABI scePadSetTriggerEffect(int handle, const void* param) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    if (param == nullptr) return PAD_ERROR_INVALID_ARG;
    // Output to the controller (light bar, rumble, adaptive triggers) has no host device to reach yet; the request is accepted.
    return PAD_OK;
}

int APS5_VABI scePadSetVibration(int handle, const PadVibrationParam* param) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    if (param == nullptr) return PAD_ERROR_INVALID_ARG;
    // Output to the controller (light bar, rumble, adaptive triggers) has no host device to reach yet; the request is accepted.
    return PAD_OK;
}

int APS5_VABI scePadSetVibrationMode(int handle, int mode) {
    if (handle != PAD_HANDLE) return PAD_ERROR_INVALID_HANDLE;
    if (mode != 0 && mode != 1) return PAD_ERROR_INVALID_ARG; // SCE_PAD_VIBRATION_MODE_COMPATIBLE / ADVANCED
    return PAD_OK;
}

int APS5_VABI scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(bool enabled) {
    (void)enabled;
    return PAD_OK;
}

}

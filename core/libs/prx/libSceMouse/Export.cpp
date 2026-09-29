#include <cstdint>
#include <cstddef>
#include <mutex>
#include <set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_MOUSE_ERROR_INVALID_ARG = static_cast<int>(0x80DF0001);
constexpr int SCE_MOUSE_ERROR_NOT_INITIALIZED = static_cast<int>(0x80DF0003);
constexpr int SCE_MOUSE_ERROR_INVALID_HANDLE = static_cast<int>(0x80DF0005);
constexpr int SCE_MOUSE_ERROR_ALREADY_OPENED = static_cast<int>(0x80DF0004);

std::mutex mouseMutex;
bool initialized = false;
std::set<int32_t> handles;
int32_t nextHandle = 1;

}

extern "C" {

int APS5_VABI sceMouseInit(void) {
    std::lock_guard lock(mouseMutex);
    initialized = true;
    return 0;
}

int APS5_VABI sceMouseOpen(int user_id, int32_t type, int32_t index, const void* param) {
    (void)param;
    std::lock_guard lock(mouseMutex);
    if (!initialized) return SCE_MOUSE_ERROR_NOT_INITIALIZED;
    if (user_id < 0 || type != 0 || index != 0) return SCE_MOUSE_ERROR_INVALID_ARG;
    if (!handles.empty()) return SCE_MOUSE_ERROR_ALREADY_OPENED;
    const auto handle = nextHandle++;
    handles.insert(handle);
    return handle;
}

int APS5_VABI sceMouseClose(int32_t handle) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return SCE_MOUSE_ERROR_NOT_INITIALIZED;
    if (handles.erase(handle) == 0) return SCE_MOUSE_ERROR_INVALID_HANDLE;
    return 0;
}

int APS5_VABI sceMouseRead(int32_t handle, MouseData* data, int32_t num) {
    std::lock_guard lock(mouseMutex);
    if (!initialized) return SCE_MOUSE_ERROR_NOT_INITIALIZED;
    if (handles.count(handle) == 0) return SCE_MOUSE_ERROR_INVALID_HANDLE;
    if (data == nullptr || num <= 0) return SCE_MOUSE_ERROR_INVALID_ARG;
    return 0;
}

}

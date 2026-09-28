#include <cstdint>
#include <cstddef>
#include <cstring>
#include <deque>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// The input method editor: the physical keyboard part opens and reports one connected keyboard;
// the on-screen keyboard has no host dialog, so opening it succeeds and the title sees no text
// events until it closes the panel. Events are delivered from sceImeUpdate on the caller's thread.
namespace {

constexpr int SCE_IME_ERROR_BUSY = static_cast<int>(0x80BC0001);
constexpr int SCE_IME_ERROR_NOT_OPENED = static_cast<int>(0x80BC0002);
constexpr int SCE_IME_ERROR_INVALID_USER_ID = static_cast<int>(0x80BC0004);
constexpr int SCE_IME_ERROR_INVALID_ARG = static_cast<int>(0x80BC0007);
constexpr int SCE_IME_ERROR_INVALID_HANDLER = static_cast<int>(0x80BC0008);
constexpr std::uint32_t SCE_IME_EVENT_OPEN = 0;
constexpr std::uint32_t SCE_IME_EVENT_KEYBOARD_OPEN = 256;
constexpr std::uint32_t KeyboardResourceId = 1;
constexpr std::uint32_t KeyboardDeviceUsb = 1;
constexpr std::uint32_t KeyboardTypeUs = 2;
constexpr std::uint32_t KeyboardStatusConnected = 1;

struct Pending { EventHandler handler; void* arg; ImeEvent event; };

std::mutex mutex;
bool keyboardOpen = false, panelOpen = false;
std::int32_t keyboardUser = 0;
std::deque<Pending> pending;

}

extern "C" {

int APS5_VABI sceImeKeyboardOpen(int32_t user_id, const KeyboardParam* param) {
    if (param == nullptr) return SCE_IME_ERROR_INVALID_ARG;
    if (param->handler == nullptr) return SCE_IME_ERROR_INVALID_HANDLER;
    std::lock_guard lock(mutex);
    if (keyboardOpen) return SCE_IME_ERROR_BUSY;
    keyboardOpen = true;
    keyboardUser = user_id;
    ImeEvent event{};
    event.id = SCE_IME_EVENT_KEYBOARD_OPEN;
    event.param.resource_id_array.user_id = user_id;
    event.param.resource_id_array.resource_id[0] = KeyboardResourceId;
    pending.push_back(Pending{param->handler, param->arg, event});
    return 0;
}

int APS5_VABI sceImeKeyboardClose(int32_t user_id) {
    std::lock_guard lock(mutex);
    if (!keyboardOpen) return SCE_IME_ERROR_NOT_OPENED;
    if (user_id != keyboardUser) return SCE_IME_ERROR_INVALID_USER_ID;
    keyboardOpen = false;
    return 0;
}

int APS5_VABI sceImeKeyboardGetResourceId(int32_t user_id, KeyboardResourceIdArray* resource_ids) {
    if (resource_ids == nullptr) return SCE_IME_ERROR_INVALID_ARG;
    std::lock_guard lock(mutex);
    if (!keyboardOpen) return SCE_IME_ERROR_NOT_OPENED;
    *resource_ids = KeyboardResourceIdArray{};
    resource_ids->user_id = user_id;
    resource_ids->resource_id[0] = KeyboardResourceId;
    return 0;
}

int APS5_VABI sceImeKeyboardGetInfo(uint32_t resource_id, KeyboardInfo* info) {
    if (info == nullptr) return SCE_IME_ERROR_INVALID_ARG;
    std::lock_guard lock(mutex);
    if (!keyboardOpen) return SCE_IME_ERROR_NOT_OPENED;
    if (resource_id != KeyboardResourceId) return SCE_IME_ERROR_INVALID_ARG;
    *info = KeyboardInfo{};
    info->user_id = keyboardUser;
    info->device = KeyboardDeviceUsb;
    info->type = KeyboardTypeUs;
    info->repeat_delay = 500;
    info->repeat_rate = 30;
    info->status = KeyboardStatusConnected;
    return 0;
}

int APS5_VABI sceImeKeyboardSetMode(int32_t user_id, uint32_t mode) {
    (void)mode;
    std::lock_guard lock(mutex);
    if (!keyboardOpen) return SCE_IME_ERROR_NOT_OPENED;
    return user_id == keyboardUser ? 0 : SCE_IME_ERROR_INVALID_USER_ID;
}

void APS5_VABI sceImeParamInit(Param* param) {
    if (param == nullptr) return;
    *param = Param{};
    param->user_id = -1;
}

int APS5_VABI sceImeOpen_nid_postfix(const Param* param, const ExtendedParam* extended) {
    (void)extended;
    if (param == nullptr) return SCE_IME_ERROR_INVALID_ARG;
    if (param->handler == nullptr) return SCE_IME_ERROR_INVALID_HANDLER;
    std::lock_guard lock(mutex);
    if (panelOpen) return SCE_IME_ERROR_BUSY;
    panelOpen = true;
    ImeEvent event{};
    event.id = SCE_IME_EVENT_OPEN;
    event.param.rect = ImeRect{param->posx, param->posy, 0, 0};
    pending.push_back(Pending{param->handler, param->arg, event});
    return 0;
}

int APS5_VABI sceImeClose_nid_postfix(void) {
    std::lock_guard lock(mutex);
    if (!panelOpen) return SCE_IME_ERROR_NOT_OPENED;
    panelOpen = false;
    return 0;
}

int APS5_VABI sceImeGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
    if (param == nullptr || width == nullptr || height == nullptr) return SCE_IME_ERROR_INVALID_ARG;
    *width = 0;
    *height = 0;
    return 0;
}

int APS5_VABI sceImeSetCaret(const Caret* caret) {
    if (caret == nullptr) return SCE_IME_ERROR_INVALID_ARG;
    std::lock_guard lock(mutex);
    return panelOpen ? 0 : SCE_IME_ERROR_NOT_OPENED;
}

int APS5_VABI sceImeSetText(const char16_t* text, uint32_t length) {
    if (text == nullptr && length != 0) return SCE_IME_ERROR_INVALID_ARG;
    std::lock_guard lock(mutex);
    return panelOpen ? 0 : SCE_IME_ERROR_NOT_OPENED;
}

int APS5_VABI sceImeSetTextGeometry(TextAreaMode mode, const TextGeometry* geometry) {
    (void)mode;
    if (geometry == nullptr) return SCE_IME_ERROR_INVALID_ARG;
    std::lock_guard lock(mutex);
    return panelOpen ? 0 : SCE_IME_ERROR_NOT_OPENED;
}

int APS5_VABI sceImeUpdate(EventHandler handler) {
    if (handler == nullptr) return SCE_IME_ERROR_INVALID_HANDLER;
    std::deque<Pending> events;
    {
        std::lock_guard lock(mutex);
        if (!keyboardOpen && !panelOpen) return SCE_IME_ERROR_NOT_OPENED;
        events.swap(pending);
    }
    for (const auto& item : events) (item.handler != nullptr ? item.handler : handler)(item.arg, &item.event);
    return 0;
}

}

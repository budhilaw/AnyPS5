#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
constexpr int SCE_NP_ERROR_CALLBACK_ALREADY_REGISTERED = static_cast<int>(0x8055000B);
constexpr int SCE_NP_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x80550018);
constexpr std::uint32_t SCE_NP_STATE_SIGNED_OUT = 1;
constexpr std::uint32_t SCE_NP_REACHABILITY_STATE_UNAVAILABLE = 0;
constexpr int LocalUser = 0x10000000;

std::mutex npMutex;
std::set<int> requests;
int nextRequest = 1;
void* stateCallback = nullptr;
void* stateCallbackA = nullptr;
void* reachabilityCallback = nullptr;
void* plusCallback = nullptr;
void* premiumCallback = nullptr;
void* presenceCallback = nullptr;
NpTitleId titleId{};
bool titleIdSet = false;

int createRequest() {
    std::lock_guard lock(npMutex);
    const int id = nextRequest++;
    requests.insert(id);
    return id;
}

bool validRequest(int id) { std::lock_guard lock(npMutex); return requests.count(id) != 0; }

}

extern "C" {

int APS5_VABI sceNpCreateRequest(void) { return createRequest(); }
int APS5_VABI sceNpCreateAsyncRequest(const NpCreateAsyncRequestParameter* param) { if (param == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; return createRequest(); }
int APS5_VABI sceNpDeleteRequest(int req_id) { std::lock_guard lock(npMutex); return requests.erase(req_id) != 0 ? 0 : SCE_NP_ERROR_REQUEST_NOT_FOUND; }
int APS5_VABI sceNpAbortRequest(int req_id) { return validRequest(req_id) ? 0 : SCE_NP_ERROR_REQUEST_NOT_FOUND; }
int APS5_VABI sceNpPollAsync(int req_id, int* result) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (result == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (!validRequest(req_id)) return SCE_NP_ERROR_REQUEST_NOT_FOUND;
    *result = SCE_NP_ERROR_SIGNED_OUT;
    return 0;
}

int APS5_VABI sceNpCheckCallback(void) { return 0; }
int APS5_VABI sceNpCheckNpAvailability(int req_id, const char* user, void* result) { (void)user; (void)result; return validRequest(req_id) ? SCE_NP_ERROR_SIGNED_OUT : SCE_NP_ERROR_REQUEST_NOT_FOUND; }
int APS5_VABI sceNpCheckNpReachability(int req_id, int user_id) { (void)user_id; return validRequest(req_id) ? SCE_NP_ERROR_SIGNED_OUT : SCE_NP_ERROR_REQUEST_NOT_FOUND; }
int APS5_VABI sceNpCheckPremium(int req_id, const NpCheckPremiumParameter* param, NpCheckPremiumResult* result) { (void)param; (void)result; return validRequest(req_id) ? SCE_NP_ERROR_SIGNED_OUT : SCE_NP_ERROR_REQUEST_NOT_FOUND; }

int APS5_VABI sceNpGetAccountAge(int req_id, int user_id, uint8_t* age) { (void)user_id; (void)age; return validRequest(req_id) ? SCE_NP_ERROR_SIGNED_OUT : SCE_NP_ERROR_REQUEST_NOT_FOUND; }
int APS5_VABI sceNpGetAccountCountryA(int user_id, void* country_code) { if (user_id != LocalUser || country_code == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; return SCE_NP_ERROR_SIGNED_OUT; }
int APS5_VABI sceNpGetAccountIdA(int user_id, uint64_t* account_id) { if (user_id != LocalUser || account_id == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; return SCE_NP_ERROR_SIGNED_OUT; }
int APS5_VABI sceNpGetNpId(int user_id, NpId* np_id) { if (user_id != LocalUser || np_id == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; return SCE_NP_ERROR_SIGNED_OUT; }
int APS5_VABI sceNpGetOnlineId(int user_id, NpOnlineId* online_id) { if (user_id != LocalUser || online_id == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; return SCE_NP_ERROR_SIGNED_OUT; }
int APS5_VABI sceNpGetState(int user_id, uint32_t* state) { if (user_id != LocalUser || state == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; *state = SCE_NP_STATE_SIGNED_OUT; return 0; }
int APS5_VABI sceNpGetNpReachabilityState(int user_id, uint32_t* state) { if (user_id != LocalUser || state == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; *state = SCE_NP_REACHABILITY_STATE_UNAVAILABLE; return 0; }
int APS5_VABI sceNpHasSignedUp(int user_id, bool* has_signed_up) { if (user_id != LocalUser || has_signed_up == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; *has_signed_up = false; return 0; }

void APS5_VABI sceNpRegisterGamePresenceCallback(void* callback, void* userdata) { (void)userdata; std::lock_guard lock(npMutex); presenceCallback = callback; }
int APS5_VABI sceNpRegisterNpReachabilityStateCallback(void* callback, void* userdata) { (void)userdata; if (callback == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; std::lock_guard lock(npMutex); reachabilityCallback = callback; return 0; }
int APS5_VABI sceNpRegisterPlusEventCallback(void* callback, void* userdata) { (void)userdata; if (callback == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; std::lock_guard lock(npMutex); plusCallback = callback; return 0; }
int APS5_VABI sceNpRegisterPremiumEventCallback(void* callback, void* userdata) { (void)userdata; if (callback == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; std::lock_guard lock(npMutex); premiumCallback = callback; return 0; }
int APS5_VABI sceNpRegisterStateCallback(void* callback, void* userdata) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)userdata;
    if (callback == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(npMutex);
    if (stateCallback != nullptr) return SCE_NP_ERROR_CALLBACK_ALREADY_REGISTERED;
    stateCallback = callback;
    return 0;
}
int APS5_VABI sceNpUnregisterStateCallback(void) { std::lock_guard lock(npMutex); stateCallback = nullptr; return 0; }

int APS5_VABI sceNpSetContentRestriction(const NpContentRestriction* restriction) { if (restriction == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; return 0; }
int APS5_VABI sceNpSetNpTitleId(const NpTitleId* title_id, const NpTitleSecret* title_secret) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (title_id == nullptr || title_secret == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(npMutex);
    titleId = *title_id;
    titleIdSet = true;
    return 0;
}

int APS5_VABI sceNpRegisterStateCallbackA(void* callback, void* userdata) {
    (void)userdata;
    if (callback == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(npMutex);
    if (stateCallbackA != nullptr) return SCE_NP_ERROR_CALLBACK_ALREADY_REGISTERED;
    stateCallbackA = callback;
    return 0;
}

int APS5_VABI sceNpUnregisterStateCallbackA(int callback_id) {
    (void)callback_id;
    std::lock_guard lock(npMutex);
    stateCallbackA = nullptr;
    return 0;
}

}

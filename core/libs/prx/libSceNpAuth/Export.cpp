#include <cstdint>
#include <cstddef>
#include <mutex>
#include <set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
constexpr int SCE_NP_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x80550018);

std::mutex mutex;
std::set<int> requests;
int nextRequest = 1;

int createRequest() { std::lock_guard lock(mutex); const auto id = nextRequest++; requests.insert(id); return id; }
bool validRequest(int id) { std::lock_guard lock(mutex); return requests.count(id) != 0; }

}

extern "C" {

int APS5_VABI sceNpAuthCreateRequest(void) { return createRequest(); }
int APS5_VABI sceNpAuthCreateAsyncRequest(const void* param) { if (param == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT; return createRequest(); }
int APS5_VABI sceNpAuthDeleteRequest(int req_id) { std::lock_guard lock(mutex); return requests.erase(req_id) != 0 ? 0 : SCE_NP_ERROR_REQUEST_NOT_FOUND; }
int APS5_VABI sceNpAuthAbortRequest(int req_id) { return validRequest(req_id) ? 0 : SCE_NP_ERROR_REQUEST_NOT_FOUND; }

int APS5_VABI sceNpAuthGetAuthorizationCodeV3(int req_id, const void* param, void* auth_code, int* issuer_id) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (param == nullptr || auth_code == nullptr || issuer_id == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return validRequest(req_id) ? SCE_NP_ERROR_SIGNED_OUT : SCE_NP_ERROR_REQUEST_NOT_FOUND;
}

int APS5_VABI sceNpAuthGetIdTokenV3(int req_id, const void* param, void* id_token) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (param == nullptr || id_token == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return validRequest(req_id) ? SCE_NP_ERROR_SIGNED_OUT : SCE_NP_ERROR_REQUEST_NOT_FOUND;
}

int APS5_VABI sceNpAuthPollAsync(int req_id, int* result) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (result == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (!validRequest(req_id)) return SCE_NP_ERROR_REQUEST_NOT_FOUND;
    *result = SCE_NP_ERROR_SIGNED_OUT;
    return 0;
}

int APS5_VABI sceNpAuthWaitAsync(int req_id, int* result) { return sceNpAuthPollAsync(req_id, result); }

}

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// The PSN web API on the host behaves like a console whose user is signed out: the library and
// its push-event objects initialize, but no user context (and so no request) can be created.
namespace {

constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_ID = static_cast<int>(0x80552b09);

struct IdTable {
    std::mutex mutex;
    std::set<int> ids;
    int next;
    explicit IdTable(int first) : next(first) {}
    int Create() { std::lock_guard lock(mutex); const auto id = next++; ids.insert(id); return id; }
    bool Destroy(int id) { std::lock_guard lock(mutex); return ids.erase(id) != 0; }
    bool Has(int id) { std::lock_guard lock(mutex); return ids.count(id) != 0; }
};

IdTable contexts(1);
IdTable handles(1);
IdTable filters(1);

}

extern "C" {

int APS5_VABI sceNpWebApi2Initialize(int lib_http_ctx_id, size_t pool_size) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)lib_http_ctx_id;
    if (pool_size == 0) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return contexts.Create();
}

int APS5_VABI sceNpWebApi2Terminate(int lib_ctx_id) { return contexts.Destroy(lib_ctx_id) ? 0 : SCE_NP_WEBAPI2_ERROR_INVALID_ID; }

int APS5_VABI sceNpWebApi2CreateUserContext(int lib_ctx_id, int user_id) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)user_id;
    if (!contexts.Has(lib_ctx_id)) return SCE_NP_WEBAPI2_ERROR_INVALID_ID;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpWebApi2DeleteUserContext(int user_context_id) { (void)user_context_id; return SCE_NP_WEBAPI2_ERROR_INVALID_ID; }

int APS5_VABI sceNpWebApi2CreateRequest(int user_context_id, const char* api_group, const char* path, const char* method, const void* content_parameter, int64_t* request_id) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)user_context_id; (void)content_parameter;
    if (api_group == nullptr || path == nullptr || method == nullptr || request_id == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_WEBAPI2_ERROR_INVALID_ID;
}

int APS5_VABI sceNpWebApi2DeleteRequest(int64_t request_id) { (void)request_id; return SCE_NP_WEBAPI2_ERROR_INVALID_ID; }
int APS5_VABI sceNpWebApi2AbortRequest(int64_t request_id) { (void)request_id; return SCE_NP_WEBAPI2_ERROR_INVALID_ID; }
int APS5_VABI sceNpWebApi2AddHttpRequestHeader(int64_t request_id, const char* field_name, const char* value) { (void)request_id; (void)field_name; (void)value; return SCE_NP_WEBAPI2_ERROR_INVALID_ID; }
int APS5_VABI sceNpWebApi2SendRequest(int64_t request_id, const void* data, size_t data_size, NpWebApi2ResponseInformationOption* response_info_option) { (void)request_id; (void)data; (void)data_size; (void)response_info_option; return SCE_NP_WEBAPI2_ERROR_INVALID_ID; }
int APS5_VABI sceNpWebApi2ReadData(int64_t request_id, void* data, size_t size) { (void)request_id; (void)data; (void)size; return SCE_NP_WEBAPI2_ERROR_INVALID_ID; }
int APS5_VABI sceNpWebApi2GetHttpResponseHeaderValue(int64_t request_id, const char* field_name, char* value, size_t value_size) { (void)request_id; (void)field_name; (void)value; (void)value_size; return SCE_NP_WEBAPI2_ERROR_INVALID_ID; }
int APS5_VABI sceNpWebApi2GetHttpResponseHeaderValueLength(int64_t request_id, const char* field_name, size_t* value_length) { (void)request_id; (void)field_name; (void)value_length; return SCE_NP_WEBAPI2_ERROR_INVALID_ID; }
void APS5_VABI sceNpWebApi2CheckTimeout(void) {}

int APS5_VABI sceNpWebApi2PushEventCreateHandle(int lib_ctx_id) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (!contexts.Has(lib_ctx_id)) return SCE_NP_WEBAPI2_ERROR_INVALID_ID;
    return handles.Create();
}

int APS5_VABI sceNpWebApi2PushEventDeleteHandle(int lib_ctx_id, int handle_id) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (!contexts.Has(lib_ctx_id)) return SCE_NP_WEBAPI2_ERROR_INVALID_ID;
    return handles.Destroy(handle_id) ? 0 : SCE_NP_WEBAPI2_ERROR_INVALID_ID;
}

int APS5_VABI sceNpWebApi2PushEventCreateFilter(int lib_ctx_id, int handle_id, const char* np_service_name, uint32_t np_service_label, const void* filter_param, size_t filter_param_num) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)np_service_label;
    if (!contexts.Has(lib_ctx_id) || !handles.Has(handle_id)) return SCE_NP_WEBAPI2_ERROR_INVALID_ID;
    if (np_service_name == nullptr || (filter_param == nullptr && filter_param_num != 0)) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return filters.Create();
}

int APS5_VABI sceNpWebApi2PushEventDeleteFilter(int lib_ctx_id, int filter_id) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (!contexts.Has(lib_ctx_id)) return SCE_NP_WEBAPI2_ERROR_INVALID_ID;
    return filters.Destroy(filter_id) ? 0 : SCE_NP_WEBAPI2_ERROR_INVALID_ID;
}

int APS5_VABI sceNpWebApi2PushEventRegisterCallback(int user_context_id, int filter_id, void* callback, void* user_arg) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)user_context_id; (void)user_arg;
    if (callback == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return filters.Has(filter_id) ? SCE_NP_ERROR_SIGNED_OUT : SCE_NP_WEBAPI2_ERROR_INVALID_ID;
}

int APS5_VABI sceNpWebApi2PushEventUnregisterCallback(int user_context_id, int callback_id) { (void)user_context_id; (void)callback_id; return SCE_NP_WEBAPI2_ERROR_INVALID_ID; }
int APS5_VABI sceNpWebApi2PushEventDeletePushContext(int user_context_id, const void* push_context_id) { (void)user_context_id; (void)push_context_id; return SCE_NP_WEBAPI2_ERROR_INVALID_ID; }

}

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_HTTP2_ERROR_INVALID_ID = static_cast<int>(0x80431100);
constexpr int SCE_HTTP2_ERROR_INVALID_VALUE = static_cast<int>(0x804311fe);
constexpr int SCE_HTTP2_ERROR_INVALID_URL = static_cast<int>(0x80433060);
constexpr int SCE_HTTP2_ERROR_NETWORK = static_cast<int>(0x80431063);
constexpr int SCE_HTTP2_ERROR_BEFORE_SEND = static_cast<int>(0x80431065);

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
IdTable templates(0x100);
IdTable requests(0x1000000);

bool anyId(int id) { return templates.Has(id) || requests.Has(id); }

int networkFailure(const char* what) {
    static std::mutex mutex;
    static std::set<const char*> reported;
    std::lock_guard lock(mutex);
    if (reported.insert(what).second) APS5_LOG_OUT("%s: network is unavailable on the host", what);
    return SCE_HTTP2_ERROR_NETWORK;
}

}

extern "C" {

int APS5_VABI sceHttp2Init(int libnet_mem_id, int libssl_ctx_id, size_t pool_size, int max_concurrent_request) {
    (void)libnet_mem_id; (void)libssl_ctx_id;
    if (pool_size == 0 || max_concurrent_request <= 0) return SCE_HTTP2_ERROR_INVALID_VALUE;
    return contexts.Create();
}

int APS5_VABI sceHttp2Term(int lib_http2_ctx_id) { return contexts.Destroy(lib_http2_ctx_id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }

int APS5_VABI sceHttp2CreateTemplate(int lib_http2_ctx_id, const char* user_agent, int http_ver, int is_auto_proxy_conf) {
    (void)http_ver; (void)is_auto_proxy_conf;
    if (!contexts.Has(lib_http2_ctx_id)) return SCE_HTTP2_ERROR_INVALID_ID;
    if (user_agent == nullptr) return SCE_HTTP2_ERROR_INVALID_VALUE;
    return templates.Create();
}

int APS5_VABI sceHttp2DeleteTemplate(int tmpl_id) { return templates.Destroy(tmpl_id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }

int APS5_VABI sceHttp2CreateRequestWithURL(int tmpl_id, const char* method, const char* url, uint64_t content_length) {
    (void)content_length;
    if (!templates.Has(tmpl_id)) return SCE_HTTP2_ERROR_INVALID_ID;
    if (method == nullptr) return SCE_HTTP2_ERROR_INVALID_VALUE;
    if (url == nullptr) return SCE_HTTP2_ERROR_INVALID_URL;
    return requests.Create();
}

int APS5_VABI sceHttp2DeleteRequest(int req_id) { return requests.Destroy(req_id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2AbortRequest_nid_postfix(int req_id) { return requests.Has(req_id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }

int APS5_VABI sceHttp2AddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
    (void)mode;
    if (!anyId(id)) return SCE_HTTP2_ERROR_INVALID_ID;
    return name != nullptr && value != nullptr ? 0 : SCE_HTTP2_ERROR_INVALID_VALUE;
}

int APS5_VABI sceHttp2SetRequestContentLength(int id, uint64_t content_length) { (void)content_length; return requests.Has(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }

int APS5_VABI sceHttp2SendRequest(int req_id, const void* post_data, size_t size) {
    (void)post_data; (void)size;
    if (!requests.Has(req_id)) return SCE_HTTP2_ERROR_INVALID_ID;
    return networkFailure("sceHttp2SendRequest");
}

int APS5_VABI sceHttp2SendRequestAsync(int req_id, const void* post_data, size_t size, void* kqueue_option, void* option) {
    (void)post_data; (void)size; (void)kqueue_option; (void)option;
    if (!requests.Has(req_id)) return SCE_HTTP2_ERROR_INVALID_ID;
    return networkFailure("sceHttp2SendRequestAsync");
}

int APS5_VABI sceHttp2ReadData(int req_id, void* data, size_t size) {
    if (data == nullptr || size == 0) return SCE_HTTP2_ERROR_INVALID_VALUE;
    return requests.Has(req_id) ? SCE_HTTP2_ERROR_BEFORE_SEND : SCE_HTTP2_ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2ReadDataAsync(int req_id, void* data, size_t size, void* kqueue_option, void* option) {
    (void)kqueue_option; (void)option;
    return sceHttp2ReadData(req_id, data, size);
}

int APS5_VABI sceHttp2WaitAsync(int req_id, Http2AsyncResult* result, uint32_t* timeout, void* option) {
    (void)timeout; (void)option;
    if (result == nullptr) return SCE_HTTP2_ERROR_INVALID_VALUE;
    return requests.Has(req_id) ? SCE_HTTP2_ERROR_BEFORE_SEND : SCE_HTTP2_ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2GetStatusCode(int req_id, int* status_code) {
    if (status_code == nullptr) return SCE_HTTP2_ERROR_INVALID_VALUE;
    return requests.Has(req_id) ? SCE_HTTP2_ERROR_BEFORE_SEND : SCE_HTTP2_ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2GetResponseContentLength(int req_id, int* result, uint64_t* content_length) {
    if (result == nullptr || content_length == nullptr) return SCE_HTTP2_ERROR_INVALID_VALUE;
    return requests.Has(req_id) ? SCE_HTTP2_ERROR_BEFORE_SEND : SCE_HTTP2_ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2GetAllResponseHeaders(int req_id, char** header, size_t* header_size) {
    if (header == nullptr || header_size == nullptr) return SCE_HTTP2_ERROR_INVALID_VALUE;
    return requests.Has(req_id) ? SCE_HTTP2_ERROR_BEFORE_SEND : SCE_HTTP2_ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2SetAuthEnabled(int id, int is_enable) { (void)is_enable; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetAutoRedirect(int id, int enable) { (void)enable; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetConnectionWaitTimeOut(int id, uint32_t usec) { (void)usec; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetConnectTimeOut(int id, uint32_t usec) { (void)usec; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetInflateGZIPEnabled(int id, int enable) { (void)enable; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetRecvTimeOut(int id, uint32_t usec) { (void)usec; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetRedirectCallback(int id, void* cb_func, void* user_arg) { (void)cb_func; (void)user_arg; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetResolveTimeOut(int id, uint32_t usec) { (void)usec; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetSendTimeOut(int id, uint32_t usec) { (void)usec; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetSslCallback(int id, void* cb_func, void* user_arg) { (void)cb_func; (void)user_arg; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetTimeOut(int id, uint32_t usec) { (void)usec; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SslDisableOption(int id, uint32_t ssl_flags) { (void)ssl_flags; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SslEnableOption(int id, uint32_t ssl_flags) { (void)ssl_flags; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }
int APS5_VABI sceHttp2SetMinSslVersion(int id, uint32_t ssl_version) { (void)ssl_version; return anyId(id) ? 0 : SCE_HTTP2_ERROR_INVALID_ID; }

}

#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// HTTP on the host follows the console without a network: contexts, templates, connections and
// requests are created and configured normally, sending fails with a network error, and the URI
// helpers (pure string functions) work fully.
namespace {

constexpr int SCE_HTTP_ERROR_INVALID_ID = static_cast<int>(0x80431100);
constexpr int SCE_HTTP_ERROR_OUT_OF_SIZE = static_cast<int>(0x80431104);
constexpr int SCE_HTTP_ERROR_INVALID_VALUE = static_cast<int>(0x804311fe);
constexpr int SCE_HTTP_ERROR_INVALID_URL = static_cast<int>(0x80433060);
constexpr int SCE_HTTP_ERROR_NETWORK = static_cast<int>(0x80431063);
constexpr int SCE_HTTP_ERROR_BEFORE_SEND = static_cast<int>(0x80431065);
constexpr int SCE_HTTP_ERROR_RESOLVER_ENODNS = static_cast<int>(0x80436002);

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
IdTable connections(0x10000);
IdTable requests(0x1000000);
IdTable epolls(1);

bool anyId(int id) { return templates.Has(id) || connections.Has(id) || requests.Has(id); }

int networkFailure(const char* what) {
    static std::mutex mutex;
    static std::set<const char*> reported;
    std::lock_guard lock(mutex);
    if (reported.insert(what).second) APS5_LOG_OUT("%s: network is unavailable on the host", what);
    return SCE_HTTP_ERROR_NETWORK;
}

bool unreserved(unsigned char c) {
    return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':' || c == '@' || c == '?' || c == '=' || c == '&' || c == '#' || c == '+' || c == '%' || c == ';' || c == ',' || c == '!' || c == '$' || c == '\'' || c == '(' || c == ')' || c == '*';
}

}

extern "C" {

int APS5_VABI sceHttpInit_nid_postfix(int memid, int ssl_ctx_id, uint64_t pool_size) {
    (void)memid; (void)ssl_ctx_id;
    if (pool_size == 0) return SCE_HTTP_ERROR_INVALID_VALUE;
    return contexts.Create();
}

int APS5_VABI sceHttpTerm_nid_postfix(int http_ctx_id) { return contexts.Destroy(http_ctx_id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }

int APS5_VABI sceHttpCreateTemplate(int http_ctx_id, const char* user_agent, int http_ver, int is_auto_proxy_conf) {
    (void)http_ver; (void)is_auto_proxy_conf;
    if (!contexts.Has(http_ctx_id)) return SCE_HTTP_ERROR_INVALID_ID;
    if (user_agent == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    return templates.Create();
}

int APS5_VABI sceHttpDeleteTemplate(int tmpl_id) { return templates.Destroy(tmpl_id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }

int APS5_VABI sceHttpCreateConnection(int tmpl_id, const char* server_name, const char* scheme, uint16_t port, int enable_keep_alive) {
    (void)port; (void)enable_keep_alive;
    if (!templates.Has(tmpl_id)) return SCE_HTTP_ERROR_INVALID_ID;
    if (server_name == nullptr || scheme == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    return connections.Create();
}

int APS5_VABI sceHttpCreateConnectionWithURL(int tmpl_id, const char* url, int enable_keep_alive) {
    (void)enable_keep_alive;
    if (!templates.Has(tmpl_id)) return SCE_HTTP_ERROR_INVALID_ID;
    if (url == nullptr) return SCE_HTTP_ERROR_INVALID_URL;
    return connections.Create();
}

int APS5_VABI sceHttpDeleteConnection(int conn_id) { return connections.Destroy(conn_id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }

int APS5_VABI sceHttpCreateRequest(int conn_id, int method, const char* path, uint64_t content_length) {
    (void)method; (void)content_length;
    if (!connections.Has(conn_id)) return SCE_HTTP_ERROR_INVALID_ID;
    if (path == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    return requests.Create();
}

int APS5_VABI sceHttpCreateRequestWithURL2(int conn_id, const char* method, const char* url, uint64_t content_length) {
    (void)content_length;
    if (!connections.Has(conn_id)) return SCE_HTTP_ERROR_INVALID_ID;
    if (method == nullptr || url == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    return requests.Create();
}

int APS5_VABI sceHttpDeleteRequest(int req_id) { return requests.Destroy(req_id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpAbortRequest(int request_id) { return requests.Has(request_id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }

int APS5_VABI sceHttpAddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
    (void)mode;
    if (!anyId(id)) return SCE_HTTP_ERROR_INVALID_ID;
    return name != nullptr && value != nullptr ? 0 : SCE_HTTP_ERROR_INVALID_VALUE;
}

int APS5_VABI sceHttpSetRequestContentLength(int request_id, uint64_t content_length) { (void)content_length; return requests.Has(request_id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }

int APS5_VABI sceHttpSendRequest(int request_id, const void* post_data, size_t size) {
    (void)post_data; (void)size;
    if (!requests.Has(request_id)) return SCE_HTTP_ERROR_INVALID_ID;
    return networkFailure("sceHttpSendRequest");
}

int APS5_VABI sceHttpGetStatusCode(int request_id, int* status_code) {
    if (status_code == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    return requests.Has(request_id) ? SCE_HTTP_ERROR_BEFORE_SEND : SCE_HTTP_ERROR_INVALID_ID;
}

int APS5_VABI sceHttpGetResponseContentLength(int request_id, int* result, uint64_t* content_length) {
    if (result == nullptr || content_length == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    return requests.Has(request_id) ? SCE_HTTP_ERROR_BEFORE_SEND : SCE_HTTP_ERROR_INVALID_ID;
}

int APS5_VABI sceHttpGetAllResponseHeaders(int request_id, char** header, size_t* header_size) {
    if (header == nullptr || header_size == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    return requests.Has(request_id) ? SCE_HTTP_ERROR_BEFORE_SEND : SCE_HTTP_ERROR_INVALID_ID;
}

int APS5_VABI sceHttpReadData(int request_id, void* data, size_t size) {
    if (data == nullptr || size == 0) return SCE_HTTP_ERROR_INVALID_VALUE;
    return requests.Has(request_id) ? SCE_HTTP_ERROR_BEFORE_SEND : SCE_HTTP_ERROR_INVALID_ID;
}

// Per-object settings: accepted and ignored, the objects never reach the network.
int APS5_VABI sceHttpSetAuthEnabled(int id, int enable) { (void)enable; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpSetAutoRedirect(int id, int enable) { (void)enable; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpSetConnectTimeOut(int id, uint32_t usec) { (void)usec; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpSetRecvTimeOut(int id, uint32_t usec) { (void)usec; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpSetSendTimeOut(int id, uint32_t usec) { (void)usec; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpSetResolveTimeOut(int id, uint32_t usec) { (void)usec; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpSetResolveRetry(int id, int32_t retry) { (void)retry; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpSetNonblock(int id, int enable) { (void)enable; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpsSetSslCallback(int id, HttpsCallback cbfunc, void* user_arg) { (void)cbfunc; (void)user_arg; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpsSetMinSslVersion(int id, uint32_t ssl_version) { (void)ssl_version; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpsDisableOption(int id, uint32_t ssl_flags) { (void)ssl_flags; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }
int APS5_VABI sceHttpsEnableOption(int id, uint32_t ssl_flags) { (void)ssl_flags; return anyId(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }

int APS5_VABI sceHttpCreateEpoll(int http_ctx_id, HttpEpollHandle* eh) {
    if (eh == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    if (!contexts.Has(http_ctx_id)) return SCE_HTTP_ERROR_INVALID_ID;
    *eh = reinterpret_cast<HttpEpollHandle>(static_cast<std::uintptr_t>(epolls.Create()));
    return 0;
}

int APS5_VABI sceHttpDestroyEpoll(int http_ctx_id, HttpEpollHandle eh) {
    if (!contexts.Has(http_ctx_id)) return SCE_HTTP_ERROR_INVALID_ID;
    return epolls.Destroy(static_cast<int>(reinterpret_cast<std::uintptr_t>(eh))) ? 0 : SCE_HTTP_ERROR_INVALID_VALUE;
}

int APS5_VABI sceHttpSetEpoll(int id, HttpEpollHandle eh, void* user_arg) {
    (void)user_arg;
    if (!requests.Has(id)) return SCE_HTTP_ERROR_INVALID_ID;
    return epolls.Has(static_cast<int>(reinterpret_cast<std::uintptr_t>(eh))) ? 0 : SCE_HTTP_ERROR_INVALID_VALUE;
}

int APS5_VABI sceHttpUnsetEpoll(int id) { return requests.Has(id) ? 0 : SCE_HTTP_ERROR_INVALID_ID; }

int APS5_VABI sceHttpWaitRequest(HttpEpollHandle eh, HttpNBEvent* nbev, int maxevents, int timeout) {
    (void)nbev; (void)timeout;
    if (!epolls.Has(static_cast<int>(reinterpret_cast<std::uintptr_t>(eh))) || maxevents <= 0) return SCE_HTTP_ERROR_INVALID_VALUE;
    return 0; // nothing is ever in flight
}

int APS5_VABI sceHttpUriParse(SceHttpUriElement* out, const char* src_url, void* pool, size_t* require, size_t prepare) {
    if (src_url == nullptr) return SCE_HTTP_ERROR_INVALID_URL;
    const std::string_view url(src_url);
    const auto schemeEnd = url.find("://");
    if (schemeEnd == std::string_view::npos || schemeEnd == 0) return SCE_HTTP_ERROR_INVALID_URL;
    std::string_view scheme = url.substr(0, schemeEnd);
    std::string_view rest = url.substr(schemeEnd + 3);
    std::string_view fragment, query, path, authority;
    if (const auto hash = rest.find('#'); hash != std::string_view::npos) { fragment = rest.substr(hash + 1); rest = rest.substr(0, hash); }
    if (const auto question = rest.find('?'); question != std::string_view::npos) { query = rest.substr(question + 1); rest = rest.substr(0, question); }
    if (const auto slash = rest.find('/'); slash != std::string_view::npos) { path = rest.substr(slash); authority = rest.substr(0, slash); } else { authority = rest; path = "/"; }
    std::string_view userinfo, username, password, hostport, host, portText;
    if (const auto at = authority.rfind('@'); at != std::string_view::npos) { userinfo = authority.substr(0, at); hostport = authority.substr(at + 1); } else { hostport = authority; }
    if (const auto colon = userinfo.find(':'); colon != std::string_view::npos) { username = userinfo.substr(0, colon); password = userinfo.substr(colon + 1); } else { username = userinfo; }
    if (!hostport.empty() && hostport.front() == '[') {
        const auto close = hostport.find(']');
        if (close == std::string_view::npos) return SCE_HTTP_ERROR_INVALID_URL;
        host = hostport.substr(1, close - 1);
        if (close + 1 < hostport.size() && hostport[close + 1] == ':') portText = hostport.substr(close + 2);
    } else if (const auto colon = hostport.find(':'); colon != std::string_view::npos) { host = hostport.substr(0, colon); portText = hostport.substr(colon + 1); } else { host = hostport; }
    std::uint16_t port = 0;
    if (!portText.empty()) {
        unsigned long value = 0;
        for (const char c : portText) { if (c < '0' || c > '9') return SCE_HTTP_ERROR_INVALID_URL; value = value * 10 + static_cast<unsigned long>(c - '0'); if (value > 65535) return SCE_HTTP_ERROR_INVALID_URL; }
        port = static_cast<std::uint16_t>(value);
    } else if (scheme == "https") port = 443; else if (scheme == "http") port = 80;
    const std::string_view parts[] = {scheme, username, password, host, path, query, fragment};
    std::size_t needed = 0;
    for (const auto part : parts) needed += part.size() + 1;
    if (require != nullptr) *require = needed;
    if (out == nullptr || pool == nullptr) return 0;
    if (prepare < needed) return SCE_HTTP_ERROR_OUT_OF_SIZE;
    auto* cursor = static_cast<char*>(pool);
    char* slots[7] = {};
    for (std::size_t index = 0; index < 7; ++index) {
        slots[index] = cursor;
        std::memcpy(cursor, parts[index].data(), parts[index].size());
        cursor[parts[index].size()] = '\0';
        cursor += parts[index].size() + 1;
    }
    *out = SceHttpUriElement{};
    out->scheme = slots[0]; out->username = slots[1]; out->password = slots[2]; out->hostname = slots[3]; out->path = slots[4]; out->query = slots[5]; out->fragment = slots[6];
    out->port = port;
    return 0;
}

int APS5_VABI sceHttpUriBuild(char* out, size_t* require, size_t prepare, const SceHttpUriElement* src_element, uint32_t option) {
    if (src_element == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    const auto text = [](const char* value) { return value != nullptr ? std::string(value) : std::string(); };
    std::string result;
    // Option bits select the components; all of them are emitted when no bits are set.
    const bool all = option == 0;
    if ((all || (option & 0x1u)) && src_element->scheme != nullptr) result += text(src_element->scheme) + "://";
    if ((all || (option & 0x2u)) && src_element->username != nullptr && *src_element->username != '\0') {
        result += text(src_element->username);
        if ((all || (option & 0x4u)) && src_element->password != nullptr && *src_element->password != '\0') result += ":" + text(src_element->password);
        result += "@";
    }
    if ((all || (option & 0x8u)) && src_element->hostname != nullptr) result += text(src_element->hostname);
    if ((all || (option & 0x10u)) && src_element->port != 0) result += ":" + std::to_string(src_element->port);
    if ((all || (option & 0x20u)) && src_element->path != nullptr) result += text(src_element->path);
    if ((all || (option & 0x40u)) && src_element->query != nullptr && *src_element->query != '\0') result += "?" + text(src_element->query);
    if ((all || (option & 0x80u)) && src_element->fragment != nullptr && *src_element->fragment != '\0') result += "#" + text(src_element->fragment);
    if (require != nullptr) *require = result.size() + 1;
    if (out == nullptr) return 0;
    if (prepare < result.size() + 1) return SCE_HTTP_ERROR_OUT_OF_SIZE;
    std::memcpy(out, result.c_str(), result.size() + 1);
    return 0;
}

int APS5_VABI sceHttpUriEscape(char* out, size_t* require, size_t prepare, const char* in) {
    if (in == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    std::string result;
    for (const char* cursor = in; *cursor != '\0'; ++cursor) {
        const auto c = static_cast<unsigned char>(*cursor);
        if (unreserved(c)) result += static_cast<char>(c); else { char buffer[4]; std::snprintf(buffer, sizeof(buffer), "%%%02X", c); result += buffer; }
    }
    if (require != nullptr) *require = result.size() + 1;
    if (out == nullptr) return 0;
    if (prepare < result.size() + 1) return SCE_HTTP_ERROR_OUT_OF_SIZE;
    std::memcpy(out, result.c_str(), result.size() + 1);
    return 0;
}

int APS5_VABI sceHttpUriUnescape(char* out, size_t* require, size_t prepare, const char* in) {
    if (in == nullptr) return SCE_HTTP_ERROR_INVALID_VALUE;
    std::string result;
    for (const char* cursor = in; *cursor != '\0'; ++cursor) {
        if (*cursor == '%' && std::isxdigit(static_cast<unsigned char>(cursor[1])) && std::isxdigit(static_cast<unsigned char>(cursor[2]))) {
            const char hex[3] = {cursor[1], cursor[2], '\0'};
            result += static_cast<char>(std::strtoul(hex, nullptr, 16));
            cursor += 2;
        } else result += *cursor;
    }
    if (require != nullptr) *require = result.size() + 1;
    if (out == nullptr) return 0;
    if (prepare < result.size() + 1) return SCE_HTTP_ERROR_OUT_OF_SIZE;
    std::memcpy(out, result.c_str(), result.size() + 1);
    return 0;
}

}

#include <arpa/inet.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <thread>
#include <chrono>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_NET_ERROR_EBADF = static_cast<int>(0x80410009);
constexpr int SCE_NET_ERROR_EINVAL = static_cast<int>(0x80410016);
constexpr int SCE_NET_ERROR_ENETDOWN = static_cast<int>(0x80410032);
constexpr int SCE_NET_ERROR_EAFNOSUPPORT = static_cast<int>(0x8041002f);
constexpr int SCE_NET_ERROR_RESOLVER_ENODNS = static_cast<int>(0x80410202);
constexpr int SCE_NET_AF_INET = 2;
constexpr int SCE_NET_AF_INET6 = 28;
constexpr int SCE_NET_INET_ADDRSTRLEN = 16;
constexpr int SCE_NET_INET6_ADDRSTRLEN = 46;

struct IdTable {
    std::mutex mutex;
    std::set<int> ids;
    int next;
    explicit IdTable(int first) : next(first) {}
    int Create() { std::lock_guard lock(mutex); const auto id = next++; ids.insert(id); return id; }
    bool Destroy(int id) { std::lock_guard lock(mutex); return ids.erase(id) != 0; }
    bool Has(int id) { std::lock_guard lock(mutex); return ids.count(id) != 0; }
};

IdTable pools(1);
IdTable resolvers(1);
IdTable epolls(1);
thread_local int netErrno = 0;

int fail(const char* what, int error, int errnoValue) {
    static std::mutex mutex;
    static std::set<const char*> reported;
    {
        std::lock_guard lock(mutex);
        if (reported.insert(what).second) APS5_LOG_OUT("%s: network is unavailable on the host", what);
    }
    netErrno = errnoValue;
    return error;
}

}

extern "C" {

int APS5_VABI sceNetInit_nid_postfix(void) { return 0; }

int APS5_VABI sceNetTerm_nid_postfix(void) { return 0; }

int* APS5_VABI sceNetErrnoLoc_nid_postfix(void) { return &netErrno; }

int APS5_VABI sceNetPoolCreate(const char* name, int size, int flags) {
    (void)name; (void)flags;
    if (size <= 0) return SCE_NET_ERROR_EINVAL;
    return pools.Create();
}

int APS5_VABI sceNetPoolDestroy(int memid) { return pools.Destroy(memid) ? 0 : SCE_NET_ERROR_EINVAL; }

int APS5_VABI sceNetResolverCreate(const char* name, int memid, int flags) {
    (void)name; (void)flags;
    if (!pools.Has(memid)) return SCE_NET_ERROR_EINVAL;
    return resolvers.Create();
}

int APS5_VABI sceNetResolverDestroy_nid_postfix(int rid) { return resolvers.Destroy(rid) ? 0 : SCE_NET_ERROR_EINVAL; }

int APS5_VABI sceNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags) {
    (void)timeout; (void)retry; (void)flags;
    if (!resolvers.Has(rid) || hostname == nullptr || addr == nullptr) return SCE_NET_ERROR_EINVAL;
    return fail("sceNetResolverStartNtoa", SCE_NET_ERROR_RESOLVER_ENODNS, 0);
}

int APS5_VABI sceNetResolverStartAton_nid_postfix(int rid, const void* addr, char* hostname, int len, int timeout, int retry, int flags) {
    (void)timeout; (void)retry; (void)flags;
    if (!resolvers.Has(rid) || hostname == nullptr || addr == nullptr || len <= 0) return SCE_NET_ERROR_EINVAL;
    return fail("sceNetResolverStartAton", SCE_NET_ERROR_RESOLVER_ENODNS, 0);
}

int APS5_VABI sceNetEpollCreate(const char* name, int flags) { (void)name; (void)flags; return epolls.Create(); }

int APS5_VABI sceNetEpollDestroy(int eid) { return epolls.Destroy(eid) ? 0 : SCE_NET_ERROR_EBADF; }

int APS5_VABI sceNetEpollControl(int eid, int op, int id, const NetEpollEvent* event) {
    (void)op; (void)id; (void)event;
    if (!epolls.Has(eid)) return SCE_NET_ERROR_EBADF;
    return SCE_NET_ERROR_EBADF;
}

int APS5_VABI sceNetEpollWait(int eid, NetEpollEvent* events, int maxevents, int timeout) {
    (void)events;
    if (!epolls.Has(eid) || maxevents <= 0) return SCE_NET_ERROR_EINVAL;
    if (timeout > 0) std::this_thread::sleep_for(std::chrono::microseconds(timeout));
    return 0;
}

int APS5_VABI sceNetSocket(const char* name, int family, int type, int protocol) {
    (void)name; (void)type; (void)protocol;
    if (family != SCE_NET_AF_INET && family != SCE_NET_AF_INET6) return SCE_NET_ERROR_EAFNOSUPPORT;
    return fail("sceNetSocket", SCE_NET_ERROR_ENETDOWN, 50);
}

int APS5_VABI sceNetSocketClose(int s) { (void)s; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetShutdown(int s, int how) { (void)s; (void)how; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetAccept(int s, void* addr, uint32_t* addrlen) { (void)s; (void)addr; (void)addrlen; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetBind_nid_postfix(int s, const void* addr, uint32_t addrlen) { (void)s; (void)addr; (void)addrlen; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetListen(int s, int backlog) { (void)s; (void)backlog; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetConnect_nid_postfix(int s, const void* addr, uint32_t addrlen) { (void)s; (void)addr; (void)addrlen; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetSend_nid_postfix(int s, const void* buf, size_t len, int flags) { (void)s; (void)buf; (void)len; (void)flags; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetSendto_nid_postfix(int s, const void* buf, size_t len, int flags, const void* addr, uint32_t addrlen) { (void)s; (void)buf; (void)len; (void)flags; (void)addr; (void)addrlen; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetRecv_nid_postfix(int s, void* buf, size_t len, int flags) { (void)s; (void)buf; (void)len; (void)flags; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetRecvfrom_nid_postfix(int s, void* buf, size_t len, int flags, void* addr, uint32_t* addrlen) { (void)s; (void)buf; (void)len; (void)flags; (void)addr; (void)addrlen; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen) { (void)s; (void)level; (void)optname; (void)optval; (void)optlen; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetGetsockopt_nid_postfix(int s, int level, int optname, void* optval, uint32_t* optlen) { (void)s; (void)level; (void)optname; (void)optval; (void)optlen; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetGetsockname(int s, void* addr, uint32_t* addrlen) { (void)s; (void)addr; (void)addrlen; return SCE_NET_ERROR_EBADF; }
int APS5_VABI sceNetGetSockInfo(int s, void* info, int n, int flags) { (void)s; (void)info; (void)n; (void)flags; return SCE_NET_ERROR_EBADF; }

int APS5_VABI sceNetGetMacAddress(NetEtherAddr* addr, int flags) {
    (void)flags;
    if (addr == nullptr) return SCE_NET_ERROR_EINVAL;
    static constexpr std::uint8_t local[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    std::memcpy(addr->data, local, sizeof(local));
    return 0;
}

int APS5_VABI sceNetEtherNtostr(const NetEtherAddr* n, char* str, size_t len) {
    if (n == nullptr || str == nullptr || len < 18) return SCE_NET_ERROR_EINVAL;
    std::snprintf(str, len, "%02x:%02x:%02x:%02x:%02x:%02x", n->data[0], n->data[1], n->data[2], n->data[3], n->data[4], n->data[5]);
    return 0;
}

int APS5_VABI sceNetInetPton(int af, const char* src, void* dst) {
    if (src == nullptr || dst == nullptr) return SCE_NET_ERROR_EINVAL;
    const auto family = af == SCE_NET_AF_INET ? AF_INET : af == SCE_NET_AF_INET6 ? AF_INET6 : -1;
    if (family < 0) return SCE_NET_ERROR_EAFNOSUPPORT;
    return ::inet_pton(family, src, dst) == 1 ? 1 : 0;
}

const char* APS5_VABI sceNetInetNtop(int af, const void* src, char* dst, uint32_t size) {
    if (src == nullptr || dst == nullptr) { netErrno = 22; return nullptr; }
    const auto family = af == SCE_NET_AF_INET ? AF_INET : af == SCE_NET_AF_INET6 ? AF_INET6 : -1;
    if (family < 0 || size < static_cast<uint32_t>(family == AF_INET ? SCE_NET_INET_ADDRSTRLEN : SCE_NET_INET6_ADDRSTRLEN)) { netErrno = 22; return nullptr; }
    return ::inet_ntop(family, src, dst, size);
}

uint32_t APS5_VABI sceNetHtonl_nid_postfix(uint32_t host32) { return htonl(host32); }
uint16_t APS5_VABI sceNetHtons_nid_postfix(uint16_t host16) { return htons(host16); }
uint32_t APS5_VABI sceNetNtohl_nid_postfix(uint32_t net32) { return ntohl(net32); }
uint16_t APS5_VABI sceNetNtohs_nid_postfix(uint16_t net16) { return ntohs(net16); }

}

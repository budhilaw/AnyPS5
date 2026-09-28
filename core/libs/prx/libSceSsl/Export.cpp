#include <cstdint>
#include <cstddef>
#include <mutex>
#include <set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// SSL contexts are bookkeeping only: no connection is ever made on the host, so certificate
// queries report that there is nothing to inspect.
namespace {

constexpr int SCE_SSL_ERROR_INVALID_ID = static_cast<int>(0x80435003);
constexpr int SCE_SSL_ERROR_INVALID_VALUE = static_cast<int>(0x804351fe);
constexpr int SCE_SSL_ERROR_INVALID_CERT = static_cast<int>(0x80435005);

std::mutex mutex;
std::set<int> contexts;
int nextContext = 1;

}

extern "C" {

int APS5_VABI sceSslInit_nid_postfix(uint64_t pool_size) {
    if (pool_size == 0) return SCE_SSL_ERROR_INVALID_VALUE;
    std::lock_guard lock(mutex);
    const auto id = nextContext++;
    contexts.insert(id);
    return id;
}

int APS5_VABI sceSslTerm_nid_postfix(int ssl_ctx_id) {
    std::lock_guard lock(mutex);
    return contexts.erase(ssl_ctx_id) != 0 ? 0 : SCE_SSL_ERROR_INVALID_ID;
}

int APS5_VABI sceSslGetMemoryPoolStats_nid_postfix(int ssl_ctx_id, void* stats) {
    if (stats == nullptr) return SCE_SSL_ERROR_INVALID_VALUE;
    std::lock_guard lock(mutex);
    if (contexts.count(ssl_ctx_id) == 0) return SCE_SSL_ERROR_INVALID_ID;
    // {pool size, current in use, max in use} as reported by the console.
    auto* words = static_cast<std::uint64_t*>(stats);
    words[0] = 0; words[1] = 0; words[2] = 0;
    return 0;
}

int APS5_VABI sceSslGetCaCerts(int ssl_ctx_id, void* ca_certs) {
    if (ca_certs == nullptr) return SCE_SSL_ERROR_INVALID_VALUE;
    std::lock_guard lock(mutex);
    return contexts.count(ssl_ctx_id) != 0 ? SCE_SSL_ERROR_INVALID_CERT : SCE_SSL_ERROR_INVALID_ID;
}

int APS5_VABI sceSslFreeCaCerts(int ssl_ctx_id, void* ca_certs) {
    (void)ca_certs;
    std::lock_guard lock(mutex);
    return contexts.count(ssl_ctx_id) != 0 ? 0 : SCE_SSL_ERROR_INVALID_ID;
}

int APS5_VABI sceSslGetSubjectName_nid_postfix(int ssl_ctx_id, void* cert, void** name) { (void)ssl_ctx_id; (void)cert; (void)name; return SCE_SSL_ERROR_INVALID_CERT; }
int APS5_VABI sceSslGetIssuerName_nid_postfix(int ssl_ctx_id, void* cert, void** name) { (void)ssl_ctx_id; (void)cert; (void)name; return SCE_SSL_ERROR_INVALID_CERT; }
int APS5_VABI sceSslGetSerialNumber_nid_postfix(int ssl_ctx_id, void* cert, const void** data, size_t* length) { (void)ssl_ctx_id; (void)cert; (void)data; (void)length; return SCE_SSL_ERROR_INVALID_CERT; }
int APS5_VABI sceSslGetNameEntryCount_nid_postfix(int ssl_ctx_id, void* name) { (void)ssl_ctx_id; (void)name; return SCE_SSL_ERROR_INVALID_VALUE; }
int APS5_VABI sceSslGetNameEntryInfo_nid_postfix(int ssl_ctx_id, void* name, int index, char* oid, size_t oid_size, void* value, size_t value_size, size_t* length) { (void)ssl_ctx_id; (void)name; (void)index; (void)oid; (void)oid_size; (void)value; (void)value_size; (void)length; return SCE_SSL_ERROR_INVALID_VALUE; }
int APS5_VABI sceSslFreeSslCertName_nid_postfix(int ssl_ctx_id, void* name) { (void)ssl_ctx_id; (void)name; return 0; }

}

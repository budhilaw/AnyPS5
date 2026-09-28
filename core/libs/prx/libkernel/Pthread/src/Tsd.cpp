#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <mutex>
#include <cerrno>
#include <pthread.h>
#include <stdexcept>
#include <vector>

// Guest thread-specific data keys map onto host pthread keys (key 0 stays invalid, as on the
// console). Destructors are guest functions that the host runs at thread exit.
namespace {

constexpr int SCE_OK = 0;
constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
constexpr int SCE_KERNEL_ERROR_EAGAIN = 0x80020023;
constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;

std::mutex keysMutex;
std::vector<std::pair<pthread_key_t, bool>> keys; // (host key, allocated)

bool lookup(PthreadKey key, pthread_key_t& host) {
    std::lock_guard lock(keysMutex);
    if (key <= 0 || static_cast<std::size_t>(key) > keys.size() || !keys[key - 1].second) return false;
    host = keys[key - 1].first;
    return true;
}

}

extern "C" {

int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    if (key == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    pthread_key_t host;
    const int result = pthread_key_create(&host, destructor);
    if (result == EAGAIN) return SCE_KERNEL_ERROR_EAGAIN;
    if (result == ENOMEM) return SCE_KERNEL_ERROR_ENOMEM;
    if (result != 0) throw std::runtime_error("scePthreadKeyCreate: host key creation failed");
    std::lock_guard lock(keysMutex);
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (!keys[index].second) {
            keys[index] = {host, true};
            *key = static_cast<PthreadKey>(index + 1);
            return SCE_OK;
        }
    }
    keys.emplace_back(host, true);
    *key = static_cast<PthreadKey>(keys.size());
    return SCE_OK;
}

int APS5_VABI scePthreadKeyDelete(PthreadKey key) {
    pthread_key_t host;
    if (!lookup(key, host)) return SCE_KERNEL_ERROR_EINVAL;
    if (pthread_key_delete(host) != 0) throw std::runtime_error("scePthreadKeyDelete: host key deletion failed");
    std::lock_guard lock(keysMutex);
    keys[key - 1].second = false;
    return SCE_OK;
}

void* APS5_VABI scePthreadGetspecific(PthreadKey key) {
    pthread_key_t host;
    if (!lookup(key, host)) return nullptr;
    return pthread_getspecific(host);
}

int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value) {
    pthread_key_t host;
    if (!lookup(key, host)) return SCE_KERNEL_ERROR_EINVAL;
    if (pthread_setspecific(host, value) != 0) return SCE_KERNEL_ERROR_ENOMEM;
    return SCE_OK;
}

}

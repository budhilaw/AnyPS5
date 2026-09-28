#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// POSIX thread-specific data over the scePthread implementations (errno results).
extern "C" {
int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor);
int APS5_VABI scePthreadKeyDelete(PthreadKey key);
void* APS5_VABI scePthreadGetspecific(PthreadKey key);
int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value);

void* APS5_VABI pthread_getspecific_nid_postfix(PthreadKey key) { return scePthreadGetspecific(key); }

int APS5_VABI pthread_setspecific_nid_postfix(PthreadKey key, void* value) {
    const int result = scePthreadSetspecific(key, value);
    return result == 0 ? 0 : result & 0xff;
}

int APS5_VABI pthread_key_create_nid_postfix(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    const int result = scePthreadKeyCreate(key, destructor);
    return result == 0 ? 0 : result & 0xff;
}

int APS5_VABI pthread_key_delete_nid_postfix(PthreadKey key) {
    const int result = scePthreadKeyDelete(key);
    return result == 0 ? 0 : result & 0xff;
}
}

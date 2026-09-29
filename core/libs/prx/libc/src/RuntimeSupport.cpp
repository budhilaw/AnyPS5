#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <functional>
#include <regex>
#include <mutex>
#include <vector>
#include <utility>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/specifics/gcc/AtomicOps.hpp"
#include "prx/libc/include/FileStream.hpp"

namespace {

std::recursive_mutex g_sysLock;

}

extern "C" {

FileStream _Stderr_nid_postfix{stderr};
FileStream _Stdout_nid_postfix{stdout};
FileStream _Stdin_nid_postfix{stdin};

namespace {
struct Destructor { void (*function)(void*); void* argument; void* dso; };
std::vector<Destructor>& destructors() { static auto* list = new std::vector<Destructor>; return *list; }
std::mutex destructorsMutex;

void runDestructors(void* dso) {
    for (;;) {
        Destructor next{};
        {
            std::lock_guard lock(destructorsMutex);
            auto& list = destructors();
            auto it = std::find_if(list.rbegin(), list.rend(), [&](const Destructor& d) { return dso == nullptr || d.dso == dso; });
            if (it == list.rend()) return;
            next = *it;
            list.erase(std::next(it).base());
        }
        next.function(next.argument);
    }
}
}

int APS5_VABI __cxa_atexit_nid_postfix(void (*func)(void*), void* arg, void* dsoHandle) {
    static bool runnerRegistered = false;
    std::lock_guard lock(destructorsMutex);
    destructors().push_back({func, arg, dsoHandle});
    if (!runnerRegistered) {
        runnerRegistered = true;
        std::atexit([] { runDestructors(nullptr); });
    }
    return 0;
}

void APS5_VABI __cxa_finalize_nid_postfix(void* dsoHandle) { runDestructors(dsoHandle); }

unsigned int APS5_VABI _Atomic_fetch_add_4_nid_postfix(volatile unsigned int* target, unsigned int value, int memoryOrder) {
    (void)memoryOrder;
    return GccAtomicFetchAdd(target, value);
}

unsigned int APS5_VABI _Atomic_fetch_sub_4_nid_postfix(volatile unsigned int* target, unsigned int value, int memoryOrder) {
    (void)memoryOrder;
    return GccAtomicFetchSub(target, value);
}

unsigned long APS5_VABI _Stoul_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoul(str, endptr, base);
}

void APS5_VABI _Locksyslock_nid_postfix() {
    g_sysLock.lock();
}

void APS5_VABI _Unlocksyslock_nid_postfix() {
    g_sysLock.unlock();
}

}

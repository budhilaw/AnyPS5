#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

namespace {

using GuestEntry = int (*)(void*, void*);

struct GuestMain {
    void* args;
    GuestEntry entry;
};

constexpr std::size_t GuestMainStackSize = 32u << 20;
bool mainQueueServed = false;

void* runGuestMain(void* opaque) {
    const auto start = *static_cast<GuestMain*>(opaque);
    delete static_cast<GuestMain*>(opaque);
    if (const auto install = reinterpret_cast<void (*)()>(dlsym(RTLD_DEFAULT, "KernelInstallGuestThread_nid_no_patch"))) install();
    std::exit(start.entry(start.args, nullptr));
}

}

extern "C" {

bool LibcMainQueueServed_nid_no_patch() {
    return mainQueueServed;
}

int LibcRunGuestMain_nid_no_patch(void* args, GuestEntry entry) {
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0 || pthread_attr_setstacksize(&attributes, GuestMainStackSize) != 0) {
        std::fprintf(stderr, "cannot describe the guest main thread\n");
        std::abort();
    }
    pthread_t thread;
    auto* start = new GuestMain{args, entry};
    if (pthread_create(&thread, &attributes, runGuestMain, start) != 0) {
        std::fprintf(stderr, "cannot create the guest main thread\n");
        std::abort();
    }
    pthread_attr_destroy(&attributes);
    pthread_detach(thread);
    mainQueueServed = true;
    auto* keepAlive = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + 1.0e9, 1.0e9, 0, 0, [](CFRunLoopTimerRef, void*) {}, nullptr);
    CFRunLoopAddTimer(CFRunLoopGetMain(), keepAlive, kCFRunLoopCommonModes);
    for (;;) CFRunLoopRun();
}

}

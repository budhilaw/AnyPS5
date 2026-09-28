#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

// The guest executable's entry stub hands its main here (see relinker MachOPatcher). Cocoa only
// allows windows and event delivery on the process main thread, so the guest's main runs on a
// dedicated thread and the main thread serves the dispatch main queue for the display driver.
namespace {

using GuestEntry = int (*)(void*, void*);

struct GuestMain {
    void* args;
    GuestEntry entry;
};

constexpr std::size_t GuestMainStackSize = 32u << 20; // titles size their main stack generously
bool mainQueueServed = false;

void* runGuestMain(void* opaque) {
    const auto start = *static_cast<GuestMain*>(opaque);
    delete static_cast<GuestMain*>(opaque);
    // libkernel installs guest thread storage for threads it creates; the adopted main thread
    // needs the same, but libc cannot depend on libkernel, so the hook is looked up at runtime.
    if (const auto install = reinterpret_cast<void (*)()>(dlsym(RTLD_DEFAULT, "KernelInstallGuestThread_nid_no_patch"))) install();
    std::exit(start.entry(start.args, nullptr));
}

}

extern "C" {

// Whether the process main thread is serving the dispatch main queue, so drivers can hand it
// work; false when a host program (a test) runs guest code on the main thread itself.
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
    // The main run loop drains the dispatch main queue on the real main thread, which AppKit
    // requires (dispatch_main would park this thread and use a worker instead). A far-future
    // timer keeps the loop alive while no other source is installed; the guest exits the process.
    auto* keepAlive = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + 1.0e9, 1.0e9, 0, 0, [](CFRunLoopTimerRef, void*) {}, nullptr);
    CFRunLoopAddTimer(CFRunLoopGetMain(), keepAlive, kCFRunLoopCommonModes);
    for (;;) CFRunLoopRun();
}

}

#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_MAINTHREAD_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_MAINTHREAD_HPP

#include <functional>

// Runs window-system work where the platform requires it. On macOS, Cocoa accepts window
// creation and event delivery only on the process main thread, which serves the dispatch main
// queue while the guest runs elsewhere (see libc GuestMain); other platforms run it in place.
namespace MainThread {

void Run(const std::function<void()>& work);

}

#endif

#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_INCLUDE_GUESTTLS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_INCLUDE_GUESTTLS_HPP

namespace GuestTls {

// Installs the guest's static thread-local storage for the calling thread. On Linux and
// Windows the host runtime does this from the executable's TLS metadata; on macOS the relinked
// Mach-O carries the template in its __ANYPS5 segment and guest code reads the block pointer
// from a pthread TSD slot, so every thread that runs guest code calls this first. It is a no-op
// when the main executable is not a relinked guest.
void InstallCurrentThread();

}

#endif

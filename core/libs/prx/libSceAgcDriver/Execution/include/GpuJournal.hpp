#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GPUJOURNAL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GPUJOURNAL_HPP

#include <chrono>
#include <string>

namespace AgcDriver::GpuJournal {

// Records the last pieces of GPU work the driver issued (draws, dispatches, presents) so a
// device wait that never returns can name what hung.
void Record(std::string description);

// Prints the journal to stderr under `heading`.
void Dump(const char* heading);

// Runs `wait` and, if it has not returned after `limit`, prints the journal to stderr once with
// `what` naming the wait; the wait itself continues (Vulkan device waits cannot be interrupted).
void Watched(const char* what, std::chrono::seconds limit, void (*wait)(void*), void* context);

template <typename Wait>
void Watched(const char* what, std::chrono::seconds limit, Wait&& wait) {
    Watched(what, limit, [](void* context) { (*static_cast<Wait*>(context))(); }, &wait);
}

}

#endif

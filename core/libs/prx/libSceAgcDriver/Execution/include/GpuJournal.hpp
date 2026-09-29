#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GPUJOURNAL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GPUJOURNAL_HPP

#include <chrono>
#include <string>

namespace AgcDriver::GpuJournal {

void Record(std::string description);

void Dump(const char* heading);

void Watched(const char* what, std::chrono::seconds limit, void (*wait)(void*), void* context);

template <typename Wait>
void Watched(const char* what, std::chrono::seconds limit, Wait&& wait) {
    Watched(what, limit, [](void* context) { (*static_cast<Wait*>(context))(); }, &wait);
}

}

#endif

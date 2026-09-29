#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr std::uint16_t EV_ERROR_FLAG = 0x4000;
}

extern "C" {

intptr_t APS5_VABI sceKernelGetEventData(const KernelEvent* ev) {
    return ev == nullptr ? 0 : ev->data;
}

int APS5_VABI sceKernelGetEventError(const KernelEvent* ev) {
    if (ev == nullptr || (ev->flags & EV_ERROR_FLAG) == 0) return 0;
    return static_cast<int>(ev->data);
}

intptr_t APS5_VABI sceKernelGetEventFflags(const KernelEvent* ev) {
    return ev == nullptr ? 0 : static_cast<intptr_t>(ev->fflags);
}

int APS5_VABI sceKernelGetEventFilter(const KernelEvent* ev) {
    return ev == nullptr ? 0 : ev->filter;
}

uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent* ev) {
    return ev == nullptr ? 0 : ev->ident;
}

void* APS5_VABI sceKernelGetEventUserData(const KernelEvent* ev) {
    return ev == nullptr ? nullptr : ev->udata;
}

}

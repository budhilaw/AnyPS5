#include "prx/libSceAgcDriver/Eq/include/Event.hpp"

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"

// GPU interrupt events: a title registers (queue, id) pairs, and an end-of-pipe release that
// requests an interrupt wakes every registered pair with the release's context id as the
// event data (see Pm4 RELEASE_MEM execution in the driver).
namespace {

struct Registration {
    KernelEqueue eq;
    int id;
};

std::mutex registrationMutex;
std::vector<Registration> registrations;

}

namespace AgcDriver::Eq {

void Trigger(std::uint32_t contextId) {
    std::vector<Registration> targets;
    {
        std::lock_guard lock(registrationMutex);
        targets = registrations;
    }
    static int reported = 0;
    if (reported < 12) { ++reported; APS5_LOG_OUT("GPU interrupt context 0x%x -> %zu registered event(s)%s", contextId, targets.size(), targets.empty() ? "" : (std::string(" first id ") + std::to_string(targets.front().id)).c_str()); }
    for (const auto& target : targets) {
        const auto result = EqueueTriggerEvent_nid_postfix(target.eq, static_cast<uintptr_t>(target.id), EVFILT_GRAPHICS_CORE, reinterpret_cast<void*>(static_cast<std::uintptr_t>(contextId)));
        if (result == EQUEUE_ERROR_EBADF || result == EQUEUE_ERROR_ENOENT) {
            std::lock_guard lock(registrationMutex);
            std::erase_if(registrations, [&](const auto& entry) { return entry.eq == target.eq && entry.id == target.id; });
        }
    }
}

}

extern "C" {

int APS5_VABI sceAgcDriverAddEqEvent(KernelEqueue eq, int id, void* udata) {
    if (eq == 0) return EQUEUE_ERROR_EBADF;
    APS5_LOG_OUT("sceAgcDriverAddEqEvent eq=%llu id=%d udata=%p", static_cast<unsigned long long>(eq), id, udata);
    KernelEqueueEvent event{};
    event.event.ident = static_cast<uintptr_t>(id);
    event.event.filter = EVFILT_GRAPHICS_CORE;
    event.event.flags = EV_ADD | EV_CLEAR;
    event.event.udata = udata;
    event.event.data = id;
    event.filter.triggerFunc = [](KernelEqueueEvent* e, void* data) {
        e->triggered = true;
        e->event.data = static_cast<intptr_t>(reinterpret_cast<std::uintptr_t>(data)); // the release's context id
    };
    const auto result = EqueueAddEvent_nid_postfix(eq, event);
    if (result != EQUEUE_OK) return result;
    std::lock_guard lock(registrationMutex);
    std::erase_if(registrations, [&](const auto& entry) { return entry.eq == eq && entry.id == id; });
    registrations.push_back({eq, id});
    return EQUEUE_OK;
}

int APS5_VABI sceAgcDriverDeleteEqEvent(KernelEqueue eq, int id) {
    if (eq == 0) return EQUEUE_ERROR_EBADF;
    const auto result = EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_GRAPHICS_CORE);
    std::lock_guard lock(registrationMutex);
    std::erase_if(registrations, [&](const auto& entry) { return entry.eq == eq && entry.id == id; });
    return result;
}

}

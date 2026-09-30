#include <string>
#include <cstdlib>
#include "prx/libSceAgcDriver/Resource/include/Registration.hpp"

#include <atomic>
#include <map>
#include <mutex>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
std::atomic<std::uint32_t> nextOwner{1};
std::atomic<std::uint32_t> nextResource{1};
std::mutex workloadStreamMutex;
std::map<std::uint32_t, std::string> workloadStreams;
}

extern "C" {

int APS5_VABI sceAgcDriverRegisterOwner(void) {
    return static_cast<int>(nextOwner.fetch_add(1) & 0x7fffffffu);
}

int APS5_VABI sceAgcDriverRegisterResource(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    static const bool trace = std::getenv("ANYPS5_TRACE_RESOURCES") != nullptr;
    if (trace) {
        const auto text = [](uint64_t value) -> std::string {
            if (value < 0x100000000ull || value > 0x7fffffffffffull) return {};
            const auto* chars = reinterpret_cast<const char*>(value);
            std::string result;
            for (int i = 0; i < 48; ++i) { const char c = chars[i]; if (c == 0) break; if (c < 32 || c > 126) return {}; result += c; }
            return result.size() >= 3 ? " \"" + result + "\"" : std::string();
        };
        APS5_LOG_OUT("register resource: %llx %llx%s %llx%s %llx%s %llx%s %llx%s", (unsigned long long)a0, (unsigned long long)a1, text(a1).c_str(), (unsigned long long)a2, text(a2).c_str(), (unsigned long long)a3, text(a3).c_str(), (unsigned long long)a4, text(a4).c_str(), (unsigned long long)a5, text(a5).c_str());
    }
    return static_cast<int>(nextResource.fetch_add(1) & 0x7fffffffu);
}

int APS5_VABI sceAgcDriverRegisterWorkloadStream(uint32_t stream_id, const char* name) {
    std::lock_guard lock(workloadStreamMutex);
    workloadStreams[stream_id] = name != nullptr ? name : "";
    return 0;
}

int APS5_VABI sceAgcDriverUnregisterWorkloadStream(uint32_t stream_id) {
    std::lock_guard lock(workloadStreamMutex);
    workloadStreams.erase(stream_id);
    return 0;
}

int APS5_VABI sceAgcDriverUnregisterOwnerAndResources(uint32_t owner_handle) {
    (void)owner_handle;
    return 0;
}

int APS5_VABI sceAgcDriverUnregisterResource(void) {
    return 0;
}

}

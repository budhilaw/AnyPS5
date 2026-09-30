#include "prx/libc/include/SlowOperation.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>

namespace {

struct Statistics {
    std::uint64_t count = 0;
    std::uint64_t total = 0;
    std::uint64_t maximum = 0;
    std::uint64_t slow = 0;
};

struct Recorder {
    std::mutex mutex;
    std::map<std::string, Statistics> statistics;
    std::chrono::steady_clock::time_point reported = std::chrono::steady_clock::now();
};

Recorder& recorder() {
    static auto* value = new Recorder;
    return *value;
}

}

extern "C" bool SlowOperationEnabled_nid_no_patch() {
    static const bool enabled = std::getenv("ANYPS5_TRACE_SLOW_OPS") != nullptr;
    return enabled;
}

extern "C" void SlowOperationRecord_nid_no_patch(const char* name, std::uint64_t nanoseconds) {
    if (!SlowOperationEnabled_nid_no_patch() || name == nullptr) return;
    auto& state = recorder();
    std::lock_guard lock(state.mutex);
    auto& entry = state.statistics[name];
    ++entry.count;
    entry.total += nanoseconds;
    entry.maximum = std::max(entry.maximum, nanoseconds);
    if (nanoseconds >= 5000000) ++entry.slow;
    const auto now = std::chrono::steady_clock::now();
    if (now - state.reported < std::chrono::seconds(5)) return;
    state.reported = now;
    for (const auto& [key, value] : state.statistics) {
        std::fprintf(stderr, "[slow-ops] %s: n=%llu total=%.1f ms max=%.1f ms over5ms=%llu\n", key.c_str(), static_cast<unsigned long long>(value.count), static_cast<double>(value.total) / 1e6, static_cast<double>(value.maximum) / 1e6, static_cast<unsigned long long>(value.slow));
    }
    state.statistics.clear();
}

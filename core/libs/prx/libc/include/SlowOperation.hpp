#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SLOWOPERATION_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SLOWOPERATION_HPP

#include <chrono>
#include <cstdint>

extern "C" {

bool SlowOperationEnabled_nid_no_patch();
void SlowOperationRecord_nid_no_patch(const char* name, std::uint64_t nanoseconds);

}

class SlowOperationTimer {
public:
    explicit SlowOperationTimer(const char* name) : name(name), enabled(SlowOperationEnabled_nid_no_patch()) {
        if (enabled) last = std::chrono::steady_clock::now();
    }
    ~SlowOperationTimer() { Split(name); }
    SlowOperationTimer(const SlowOperationTimer&) = delete;
    SlowOperationTimer& operator=(const SlowOperationTimer&) = delete;
    void Split(const char* stage) {
        if (!enabled) return;
        const auto now = std::chrono::steady_clock::now();
        SlowOperationRecord_nid_no_patch(stage, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - last).count()));
        last = now;
    }

private:
    const char* name;
    bool enabled;
    std::chrono::steady_clock::time_point last{};
};

#endif

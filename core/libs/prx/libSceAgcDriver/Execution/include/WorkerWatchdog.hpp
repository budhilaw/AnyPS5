#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_WORKERWATCHDOG_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_WORKERWATCHDOG_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>

namespace AgcDriver {

class WorkerWatchdog {
public:
    using Clock = std::chrono::steady_clock;

    struct Stall {
        std::uint32_t queue;
        std::uint32_t header;
        const char* stage;
        Clock::duration elapsed;
    };

    explicit WorkerWatchdog(bool enabled, Clock::duration threshold = std::chrono::seconds(1), Clock::duration repeat = std::chrono::seconds(5)) : enabled(enabled), threshold(threshold), repeat(repeat) {}

    WorkerWatchdog(const WorkerWatchdog&) = delete;
    WorkerWatchdog& operator=(const WorkerWatchdog&) = delete;

    bool Enabled() const {
        return enabled;
    }

    void Enter(std::uint32_t queue, std::uint32_t header, const char* stage) {
        if (!enabled) return;
        packet.store((static_cast<std::uint64_t>(queue) << 32u) | header, std::memory_order_relaxed);
        current.store(stage, std::memory_order_relaxed);
        busy.store(true, std::memory_order_relaxed);
    }

    const char* Stage(const char* stage) {
        if (!enabled) return nullptr;
        const auto previous = current.load(std::memory_order_relaxed);
        current.store(stage, std::memory_order_relaxed);
        return previous;
    }

    void Progress() {
        if (!enabled) return;
        completed.store(completed.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        current.store(nullptr, std::memory_order_relaxed);
    }

    void Idle() {
        if (!enabled) return;
        busy.store(false, std::memory_order_relaxed);
    }

    std::optional<Stall> Check(Clock::time_point now) {
        if (!enabled) return std::nullopt;
        const auto done = completed.load(std::memory_order_relaxed);
        if (!watching || !busy.load(std::memory_order_relaxed) || done != observed) {
            watching = true;
            observed = done;
            since = now;
            reported = Clock::duration::zero();
            return std::nullopt;
        }
        const auto elapsed = now - since;
        if (elapsed < threshold || (reported != Clock::duration::zero() && elapsed < reported + repeat)) return std::nullopt;
        reported = elapsed;
        const auto value = packet.load(std::memory_order_relaxed);
        return Stall{static_cast<std::uint32_t>(value >> 32u), static_cast<std::uint32_t>(value), current.load(std::memory_order_relaxed), elapsed};
    }

private:
    const bool enabled;
    const Clock::duration threshold;
    const Clock::duration repeat;
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> packet{0};
    std::atomic<const char*> current{nullptr};
    std::atomic<bool> busy{false};
    bool watching = false;
    std::uint64_t observed = 0;
    Clock::time_point since{};
    Clock::duration reported{};
};

}

#endif

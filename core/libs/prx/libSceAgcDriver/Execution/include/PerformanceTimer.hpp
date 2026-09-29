#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PERFORMANCETIMER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PERFORMANCETIMER_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <functional>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif

namespace AgcDriver {

struct TimingClock {
    using rep = std::int64_t;
    using period = std::nano;
    using duration = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<TimingClock>;
    static constexpr bool is_steady = true;
    static time_point now() noexcept {
#if defined(__APPLE__)
        static const mach_timebase_info_data_t timebase = [] {
            mach_timebase_info_data_t info{};
            mach_timebase_info(&info);
            return info;
        }();
        const auto ticks = static_cast<unsigned __int128>(mach_absolute_time()) * timebase.numer / timebase.denom;
        return time_point(duration(static_cast<rep>(ticks)));
#else
        return time_point(std::chrono::duration_cast<duration>(std::chrono::steady_clock::now().time_since_epoch()));
#endif
    }
};

class FrameTiming {
public:
    using Clock = TimingClock;

    struct Metric {
        std::atomic<std::int64_t> total{0};
        std::atomic<std::int64_t> maximum{0};
        std::atomic<std::uint64_t> count{0};
        std::atomic<std::uint64_t> bytes{0};
    };

    explicit FrameTiming(std::uint64_t id) : id(id) {}

    Metric* Get(const char* scope, const char* stage) {
        struct Cached { std::uint64_t frame; const FrameTiming* owner; const char* scope; const char* stage; Metric* metric; };
        thread_local std::array<Cached, 256> cache{};
        const auto slot = (std::hash<const void*>{}(scope) * 31u ^ std::hash<const void*>{}(stage)) % cache.size();
        auto& cached = cache[slot];
        if (cached.owner == this && cached.frame == id && cached.scope == scope && cached.stage == stage) return cached.metric;
        std::lock_guard lock(mutex);
        const PointerKey key{scope, stage};
        Metric* metric = nullptr;
        if (const auto it = byPointer.find(key); it != byPointer.end()) metric = it->second;
        else {
            metric = &metrics[{scope, stage}];
            byPointer.emplace(key, metric);
        }
        cached = {id, this, scope, stage, metric};
        return metric;
    }

    void Add(Metric* metric, Clock::duration elapsed, std::uint64_t bytes = 0) {
        const auto nanoseconds = static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
        metric->total.fetch_add(nanoseconds, std::memory_order_relaxed);
        auto maximum = metric->maximum.load(std::memory_order_relaxed);
        while (nanoseconds > maximum && !metric->maximum.compare_exchange_weak(maximum, nanoseconds, std::memory_order_relaxed)) {}
        metric->count.fetch_add(1, std::memory_order_relaxed);
        if (bytes != 0) metric->bytes.fetch_add(bytes, std::memory_order_relaxed);
    }

    void IncludeSubmission(std::uint64_t serial, Clock::time_point received, Clock::time_point enqueued, Clock::time_point dequeued, bool firstSegment) {
        if (serial == 0 || received > enqueued || enqueued > dequeued) throw std::runtime_error("Frame timing: invalid submission timestamps");
        if (firstSerial == 0) {
            firstSerial = serial;
            start = received;
            executionStart = dequeued;
        }
        start = std::min(start, received);
        lastSerial = serial;
        if (firstSegment) {
            Add(Get("Submission", "accept"), enqueued - received);
            Add(Get("Submission", "queue"), dequeued - enqueued);
        }
    }

    void SetFlip(std::uint64_t serial, std::size_t offset, Clock::time_point received, Clock::time_point reached) {
        if (serial != lastSerial || received > reached || executionStart > reached) throw std::runtime_error("Frame timing: invalid flip lineage");
        flipSerial = serial;
        flipOffset = offset;
        flipReceived = received;
        flipReached = reached;
    }

    void Print(std::uint32_t outputHandle, std::int32_t buffer, std::int64_t argument, Clock::time_point finished, Clock::duration interval) {
        std::lock_guard lock(mutex);
        if (firstSerial == 0 || flipSerial == 0) throw std::runtime_error("Frame timing: incomplete submission lineage");
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::fixed << std::setprecision(3);
        output << "[FrameTiming] frame=" << id << " submissions=" << firstSerial << ':' << lastSerial;
        output << " output=" << outputHandle;
        output << " flip=" << flipSerial << ':' << flipOffset << " buffer=" << buffer << " argument=" << argument;
        output << " endpoint=flip_complete display_confirmed=0";
        output << " first_submit_to_flip_ms=" << milliseconds(finished - start);
        output << " flip_submit_to_complete_ms=" << milliseconds(finished - flipReceived);
        output << " first_submit_to_execute_ms=" << milliseconds(executionStart - start);
        output << " execute_to_flip_packet_ms=" << milliseconds(flipReached - executionStart);
        output << " flip_packet_to_complete_ms=" << milliseconds(finished - flipReached);
        auto accounted = Clock::duration::zero();
        for (const auto scope : {"Driver.Packet", "Driver.Suspend", "Driver.Worker", "Driver.Completion"}) {
            const auto it = metrics.find({scope, "total"});
            if (it != metrics.end()) accounted += std::chrono::nanoseconds(it->second.total.load());
        }
        output << " worker_unattributed_ms=" << milliseconds(flipReached - executionStart - accounted);
        if (interval != Clock::duration::zero()) output << " flip_interval_ms=" << milliseconds(interval);
        output << " metrics=inclusive(count,sum_ms,max_ms[,bytes])";
        for (const auto& [key, metric] : metrics) {
            const auto count = metric.count.load();
            if (count == 0) continue;
            output << ' ' << key.first << '.' << key.second << "=(" << count << ',' << milliseconds(std::chrono::nanoseconds(metric.total.load())) << ',' << milliseconds(std::chrono::nanoseconds(metric.maximum.load()));
            if (const auto bytes = metric.bytes.load(); bytes != 0) output << ',' << bytes;
            output << ')';
        }
        output << '\n';
        const auto text = output.str();
        if (std::fwrite(text.data(), 1, text.size(), stdout) != text.size() || std::fflush(stdout) != 0) throw std::runtime_error("Frame timing: report write failed");
    }

    void PrintPartial(const char* reason) {
        std::lock_guard lock(mutex);
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::fixed << std::setprecision(3);
        output << "[FrameTiming] partial frame=" << id << " reason=" << reason << " submissions=" << firstSerial << ':' << lastSerial;
        output << " metrics=inclusive(count,sum_ms,max_ms[,bytes])";
        for (const auto& [key, metric] : metrics) {
            const auto count = metric.count.load();
            if (count == 0) continue;
            output << ' ' << key.first << '.' << key.second << "=(" << count << ',' << milliseconds(std::chrono::nanoseconds(metric.total.load())) << ',' << milliseconds(std::chrono::nanoseconds(metric.maximum.load()));
            if (const auto bytes = metric.bytes.load(); bytes != 0) output << ',' << bytes;
            output << ')';
        }
        output << '\n';
        const auto text = output.str();
        std::fwrite(text.data(), 1, text.size(), stderr);
    }

private:
    static double milliseconds(Clock::duration elapsed) {
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

    std::mutex mutex;
    std::map<std::pair<std::string_view, std::string_view>, Metric> metrics;
    using PointerKey = std::pair<const char*, const char*>;
    struct PointerKeyHash {
        std::size_t operator()(const PointerKey& key) const noexcept {
            return std::hash<const void*>{}(key.first) * 31u ^ std::hash<const void*>{}(key.second);
        }
    };
    std::unordered_map<PointerKey, Metric*, PointerKeyHash> byPointer;
    std::uint64_t id;
    std::uint64_t firstSerial = 0;
    std::uint64_t lastSerial = 0;
    std::uint64_t flipSerial = 0;
    std::size_t flipOffset = 0;
    Clock::time_point start{};
    Clock::time_point executionStart{};
    Clock::time_point flipReceived{};
    Clock::time_point flipReached{};
};

class PerformanceContext {
public:
    explicit PerformanceContext(FrameTiming* frame) : previous(current) {
        current = frame;
    }

    PerformanceContext(const PerformanceContext&) = delete;
    PerformanceContext& operator=(const PerformanceContext&) = delete;

    ~PerformanceContext() {
        current = previous;
    }

    static FrameTiming* Current() {
        return current;
    }

private:
    inline static thread_local FrameTiming* current = nullptr;
    FrameTiming* previous;
};

class PerformanceTimer {
public:
    explicit PerformanceTimer(const char* scope) : frame(PerformanceContext::Current()), scope(scope) {
        if (frame == nullptr) return;
        total = frame->Get(scope, "total");
        start = Clock::now();
        previous = start;
    }

    PerformanceTimer(const PerformanceTimer&) = delete;
    PerformanceTimer& operator=(const PerformanceTimer&) = delete;

    ~PerformanceTimer() {
        if (frame == nullptr) return;
        const auto end = Clock::now();
        if (tail != nullptr) frame->Add(tail, end - previous);
        frame->Add(total, end - start);
    }

    void Mark(const char* stage, std::uint64_t bytes = 0) {
        if (frame == nullptr) return;
        const auto now = Clock::now();
        if (tail == nullptr) tail = frame->Get(scope, "tail");
        frame->Add(frame->Get(scope, stage), now - previous, bytes);
        previous = now;
    }

private:
    using Clock = FrameTiming::Clock;

    FrameTiming* frame;
    const char* scope;
    FrameTiming::Metric* total = nullptr;
    FrameTiming::Metric* tail = nullptr;
    Clock::time_point start;
    Clock::time_point previous;
};

}

#endif

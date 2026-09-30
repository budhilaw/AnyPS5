#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVERTHREAD_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVERTHREAD_HPP

#include <chrono>
#include <cstdint>

namespace AgcDriver::DriverThread {

void RaisePriority();

class CpuUsage {
public:
    struct Window {
        std::uint64_t cpuNanoseconds;
        std::uint64_t wallNanoseconds;
    };

    explicit CpuUsage(const char* name);

    void Sample() {
        if (enabled) report();
    }

    Window Take();

private:
    void report();

    const char* name;
    bool enabled;
    std::chrono::steady_clock::time_point wall;
    std::uint64_t cpu;
    std::uint64_t reference;
};

}

#endif

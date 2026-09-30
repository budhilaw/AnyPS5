#include "prx/libSceAgcDriver/Execution/include/GpuJournal.hpp"
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

namespace AgcDriver::GpuJournal {
namespace {

std::mutex mutex;
std::deque<std::string> entries;
constexpr std::size_t Capacity = 2048;

}

void Record(std::string description) {
    std::lock_guard lock(mutex);
    entries.push_back(std::move(description));
    while (entries.size() > Capacity) entries.pop_front();
}

void Dump(const char* heading) {
    std::lock_guard lock(mutex);
    std::fprintf(stderr, "%s\n", heading);
    for (const auto& entry : entries) std::fprintf(stderr, "  %s\n", entry.c_str());
    std::fflush(stderr);
}

void Watched(const char* what, std::chrono::seconds limit, void (*wait)(void*), void* context) {
    std::mutex doneMutex;
    std::condition_variable doneChanged;
    bool done = false;
    std::thread watchdog([&] {
        std::unique_lock lock(doneMutex);
        if (doneChanged.wait_for(lock, limit, [&] { return done; })) return;
        char heading[160];
        std::snprintf(heading, sizeof(heading), "AGC driver: %s has not completed after %lld s; last GPU work, oldest first:", what, static_cast<long long>(limit.count()));
        Dump(heading);
    });
    wait(context);
    {
        std::lock_guard lock(doneMutex);
        done = true;
    }
    doneChanged.notify_all();
    watchdog.join();
}

}

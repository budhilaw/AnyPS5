#include "prx/libSceAgcDriver/Execution/include/ThreadSampler.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include "prx/libc/include/PreciseSleep.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#endif

namespace AgcDriver::ThreadSampler {

#ifdef _WIN32
namespace {

struct Module {
    std::uintptr_t base;
    std::uintptr_t end;
    std::string name;
};

struct Target {
    HANDLE thread;
    std::string name;
    std::uint64_t samples = 0;
    std::unordered_map<std::uintptr_t, std::uint64_t> self;
    std::unordered_map<std::uintptr_t, std::uint64_t> inclusive;
};

struct Sampler {
    std::mutex mutex;
    std::vector<std::unique_ptr<Target>> targets;
    bool started = false;
};

Sampler& sampler() {
    static auto* value = new Sampler;
    return *value;
}

std::vector<Module> loadedModules() {
    std::vector<HMODULE> handles(2048);
    DWORD needed = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), handles.data(), static_cast<DWORD>(handles.size() * sizeof(HMODULE)), &needed)) return {};
    handles.resize(std::min<std::size_t>(handles.size(), needed / sizeof(HMODULE)));
    std::vector<Module> result;
    for (const auto handle : handles) {
        MODULEINFO info{};
        char name[MAX_PATH] = "?";
        if (!K32GetModuleInformation(GetCurrentProcess(), handle, &info, sizeof(info))) continue;
        K32GetModuleBaseNameA(GetCurrentProcess(), handle, name, sizeof(name));
        const auto base = reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);
        result.push_back({base, base + info.SizeOfImage, name});
    }
    std::sort(result.begin(), result.end(), [](const Module& left, const Module& right) { return left.base < right.base; });
    return result;
}

const Module* moduleOf(const std::vector<Module>& modules, std::uintptr_t address) {
    auto found = std::upper_bound(modules.begin(), modules.end(), address, [](std::uintptr_t value, const Module& module) { return value < module.base; });
    if (found == modules.begin()) return nullptr;
    --found;
    return address < found->end ? &*found : nullptr;
}

bool followsCall(const std::vector<Module>& modules, std::uintptr_t address) {
    const auto* module = moduleOf(modules, address);
    if (module == nullptr || address < module->base + 0x1000 + 7) return false;
    const auto* code = reinterpret_cast<const unsigned char*>(address);
    return code[-5] == 0xe8 || (code[-2] == 0xff && (code[-1] & 0x38) == 0x10) || (code[-3] == 0xff && (code[-2] & 0x38) == 0x10) || (code[-6] == 0xff && (code[-5] & 0x38) == 0x10) || (code[-7] == 0xff && (code[-6] & 0x38) == 0x10);
}

void describe(const std::vector<Module>& modules, std::uintptr_t address, char* text, std::size_t size) {
    const auto* module = moduleOf(modules, address);
    if (module == nullptr) std::snprintf(text, size, "?+0x%llx", static_cast<unsigned long long>(address));
    else std::snprintf(text, size, "%s+0x%llx", module->name.c_str(), static_cast<unsigned long long>(address - module->base));
}

void report(Target& target, const std::vector<Module>& modules) {
    const auto print = [&](const char* kind, const std::unordered_map<std::uintptr_t, std::uint64_t>& counts) {
        std::vector<std::pair<std::uint64_t, std::uintptr_t>> sorted;
        sorted.reserve(counts.size());
        for (const auto& [address, count] : counts) sorted.emplace_back(count, address);
        std::sort(sorted.rbegin(), sorted.rend());
        for (std::size_t i = 0; i < sorted.size() && i < 200; ++i) {
            char where[160];
            describe(modules, sorted[i].second, where, sizeof(where));
            std::fprintf(stderr, "[sample] %s %s %llu %s\n", target.name.c_str(), kind, static_cast<unsigned long long>(sorted[i].first), where);
        }
    };
    std::fprintf(stderr, "[sample] %s window %llu samples\n", target.name.c_str(), static_cast<unsigned long long>(target.samples));
    print("self", target.self);
    print("incl", target.inclusive);
    target.samples = 0;
    target.self.clear();
    target.inclusive.clear();
}

void run() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    auto& state = sampler();
    auto modules = loadedModules();
    auto refreshed = std::chrono::steady_clock::now();
    auto reported = refreshed;
    std::array<std::uintptr_t, 1024> stack{};
    std::array<std::uintptr_t, 24> seen{};
    for (;;) {
        PreciseSleepNanos_nid_no_patch(1000000);
        std::lock_guard lock(state.mutex);
        for (auto& target : state.targets) {
            if (SuspendThread(target->thread) == static_cast<DWORD>(-1)) continue;
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            std::size_t words = 0;
            const bool captured = GetThreadContext(target->thread, &context) != 0;
            if (captured) {
                MEMORY_BASIC_INFORMATION memory{};
                if (VirtualQuery(reinterpret_cast<const void*>(context.Rsp), &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_COMMIT) {
                    const auto end = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
                    words = std::min<std::size_t>(stack.size(), (end - context.Rsp) / sizeof(std::uintptr_t));
                    std::memcpy(stack.data(), reinterpret_cast<const void*>(context.Rsp), words * sizeof(std::uintptr_t));
                }
            }
            ResumeThread(target->thread);
            if (!captured) continue;
            ++target->samples;
            ++target->self[context.Rip];
            std::size_t unique = 0;
            for (std::size_t i = 0; i < words && unique < seen.size(); ++i) {
                const auto value = stack[i];
                if (!followsCall(modules, value) || std::find(seen.begin(), seen.begin() + unique, value) != seen.begin() + unique) continue;
                seen[unique++] = value;
                ++target->inclusive[value];
            }
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - refreshed > std::chrono::seconds(5)) {
            modules = loadedModules();
            refreshed = now;
        }
        if (now - reported > std::chrono::seconds(30)) {
            reported = now;
            for (auto& target : state.targets) report(*target, modules);
        }
    }
}

}

void Register(const char* name) {
    static const bool enabled = std::getenv("ANYPS5_SAMPLE_THREADS") != nullptr;
    if (!enabled) return;
    HANDLE handle = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0)) return;
    auto& state = sampler();
    std::lock_guard lock(state.mutex);
    auto target = std::make_unique<Target>();
    target->thread = handle;
    target->name = name;
    state.targets.push_back(std::move(target));
    if (!state.started) {
        state.started = true;
        std::thread(run).detach();
    }
}
#else
void Register(const char*) {}
#endif

}

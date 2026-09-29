#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_MEMORYACCESSSCOPE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_MEMORYACCESSSCOPE_HPP

#include <cstddef>
#include <cstdint>

namespace AgcDriver::GuestMemory {

class MemoryAccessScope {
public:
    using Resolver = void (*)(void*, std::uint64_t, std::size_t, bool);
    // Whether no queued GPU work touches the range, so later reads there need no resolving.
    using Query = bool (*)(void*, std::uint64_t, std::size_t);
    MemoryAccessScope(void* context, Resolver resolver, Query quiet = nullptr) : previousContext(currentContext), previousResolver(currentResolver), previousQuiet(currentQuiet) {
        currentContext = context;
        currentResolver = resolver;
        currentQuiet = quiet;
    }
    ~MemoryAccessScope() {
        currentContext = previousContext;
        currentResolver = previousResolver;
        currentQuiet = previousQuiet;
    }
    MemoryAccessScope(const MemoryAccessScope&) = delete;
    MemoryAccessScope& operator=(const MemoryAccessScope&) = delete;
    static void Resolve(std::uint64_t address, std::size_t bytes, bool writable) {
        const auto resolver = currentResolver;
        const auto context = currentContext;
        if (resolver == nullptr || bytes == 0) return;
        const MemoryAccessScope suspended(nullptr, nullptr);
        resolver(context, address, bytes, writable);
    }
    static bool Quiet(std::uint64_t address, std::size_t bytes) {
        if (currentResolver == nullptr) return true;
        const auto quiet = currentQuiet;
        return quiet != nullptr && quiet(currentContext, address, bytes);
    }

private:
    inline static thread_local void* currentContext = nullptr;
    inline static thread_local Resolver currentResolver = nullptr;
    inline static thread_local Query currentQuiet = nullptr;
    void* previousContext;
    Resolver previousResolver;
    Query previousQuiet;
};

}

#endif

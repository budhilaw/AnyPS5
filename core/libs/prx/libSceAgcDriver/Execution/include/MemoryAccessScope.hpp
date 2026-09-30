#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_MEMORYACCESSSCOPE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_MEMORYACCESSSCOPE_HPP

#include <cstddef>
#include <cstdint>

namespace AgcDriver::GuestMemory {

class MemoryAccessScope {
public:
    using Resolver = void (*)(void*, std::uint64_t, std::size_t, bool);
    using Query = bool (*)(void*, std::uint64_t, std::size_t);
    MemoryAccessScope(void* context, Resolver resolver, Query quiet = nullptr) : previous(current()) {
        current() = {context, resolver, quiet};
    }
    ~MemoryAccessScope() {
        current() = previous;
    }
    MemoryAccessScope(const MemoryAccessScope&) = delete;
    MemoryAccessScope& operator=(const MemoryAccessScope&) = delete;
    static void Resolve(std::uint64_t address, std::size_t bytes, bool writable) {
        const auto active = current();
        if (active.resolver == nullptr || bytes == 0) return;
        const MemoryAccessScope suspended(nullptr, nullptr);
        active.resolver(active.context, address, bytes, writable);
    }
    static bool Quiet(std::uint64_t address, std::size_t bytes) {
        const auto active = current();
        if (active.resolver == nullptr) return true;
        return active.quiet != nullptr && active.quiet(active.context, address, bytes);
    }

private:
    struct State {
        void* context = nullptr;
        Resolver resolver = nullptr;
        Query quiet = nullptr;
    };
    static State& current();
    State previous;
};

}

#endif

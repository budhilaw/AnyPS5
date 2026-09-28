#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <stdexcept>

// SceLibcMallocManagedSize, as titles fill it for malloc_stats_fast (size 0x30, version 1).
struct MallocManagedSize {
    std::uint16_t size;
    std::uint16_t version;
    std::uint32_t reserved;
    std::size_t maxSystemSize;
    std::size_t currentSystemSize;
    std::size_t maxInuseSize;
    std::size_t currentInuseSize;
};

extern "C" {

// Reports the libc heap. Titles that reach this runtime register their own allocator through the
// process parameters (see ApplicationHeap), so the libc heap itself holds nothing: all four
// figures are zero, which is what the console reports for an unused libc heap as well.
int APS5_VABI malloc_stats_fast_nid_postfix(MallocManagedSize* managed) {
    if (managed == nullptr) throw std::invalid_argument("malloc_stats_fast: null output");
    if (managed->size != sizeof(MallocManagedSize) || managed->version != 1) throw std::runtime_error("malloc_stats_fast: unsupported SceLibcMallocManagedSize layout");
    managed->maxSystemSize = 0;
    managed->currentSystemSize = 0;
    managed->maxInuseSize = 0;
    managed->currentInuseSize = 0;
    return 0;
}

}

#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <stdexcept>

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

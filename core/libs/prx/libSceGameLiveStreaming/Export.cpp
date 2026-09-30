#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>
#include <string>

namespace {

struct GameLiveStreamingStatusHead {
    std::int32_t userId;
    bool isOnAir;
};

}

extern "C" {

int APS5_VABI sceGameLiveStreamingInitialize(size_t heap_size) {
 (void)heap_size;
 return 0;
}

int APS5_VABI sceGameLiveStreamingTerminate(void) {
 return 0;
}

int APS5_VABI sceGameLiveStreamingGetCurrentStatus2(GameLiveStreamingStatusHead* status) {
    if (status == nullptr) throw std::invalid_argument(std::string(__func__) + ": status is null");
    status->userId = -1;
    status->isOnAir = false;
    return 0;
}

}

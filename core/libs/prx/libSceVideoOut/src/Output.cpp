#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <cmath>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"

static constexpr int TracedCalls = 20;

static bool traceCall(std::atomic<int>& calls) {
    return calls.fetch_add(1, std::memory_order_relaxed) < TracedCalls;
}

static int validateOutputConfig(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) {
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    if (reservedPtr != nullptr || reserved != 0) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    if (options != nullptr) {
        for (auto v : options->internalData) {
            if (v != 0) {
                throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_OPTION");
            }
        }
    }
    if (mode != VIDEO_OUT_OUTPUT_MODE_DEFAULT && mode != VIDEO_OUT_OUTPUT_MODE_119_88HZ) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE");
    }
    return 0;
}

static int outputModeSupported(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) {
    const int result = validateOutputConfig(handle, mode, options, reservedPtr, reserved);
    if (result != 0) {
        return result;
    }
    return (mode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) ? 0 : 1;
}

extern "C" {

int APS5_VABI sceVideoOutOpen(int userId, int busType, int index, const void* param) {
    if (param != nullptr) {
        const auto* bytes = static_cast<const unsigned char*>(param);
        char text[3 * 32 + 1] = {};
        for (int i = 0; i < 32; ++i) std::snprintf(text + 3 * i, 4, "%02x ", bytes[i]);
        APS5_LOG_OUT("sceVideoOutOpen: param bytes %s", text);
    }
    if (userId != 255 && userId != 0) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    if (busType != VIDEO_OUT_BUS_TYPE_MAIN && busType != VIDEO_OUT_BUS_TYPE_OVERLAY && busType != VIDEO_OUT_BUS_TYPE_SUB) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    if (index != 0) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    const int handle = VideoOutDriver::Get().Open(busType);
    if (handle < 0) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_RESOURCE_BUSY");
    }
    return handle;
}

int APS5_VABI sceVideoOutClose(int handle) {
    return VideoOutDriver::Get().Close(handle) ? 0 : VIDEO_OUT_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceVideoOutSetFlipRate(int handle, int rate) {
    static std::atomic<int> calls{0};
    if (traceCall(calls)) APS5_LOG_OUT("sceVideoOutSetFlipRate handle=%d rate=%d", handle, rate);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    if (rate < 0 || rate > 2) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    std::lock_guard lock(cfg->mutex);
    cfg->Check();
    cfg->flipRate = rate;
    return 0;
}

int APS5_VABI sceVideoOutGetFlipStatus(int handle, VideoOutFlipStatus* status) {
    if (status == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    *status = cfg->flipStatus;
    { static int reported = 0; if (reported < 40) { ++reported; APS5_LOG_OUT("sceVideoOutGetFlipStatus handle=%d count=%llu pending=%d current=%d gcQueue=%d arg=%lld", handle, static_cast<unsigned long long>(status->count), status->flipPendingNum, status->currentBuffer, status->gcQueueNum, static_cast<long long>(status->flipArg)); } return 0; }
}

int APS5_VABI sceVideoOutGetVblankStatus(int handle, VideoOutVblankStatus* status) {
    if (status == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    *status = cfg->vblankStatus;
    return 0;
}

int APS5_VABI sceVideoOutGetOutputStatus(int handle, VideoOutOutputStatus* status) {
    if (status == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    status->resolution = (cfg->width >= 3840 || cfg->height >= 2160) ? 2u : 1u;
    status->dynamicRange = 1;
    status->refreshRate = (cfg->outputMode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) ? VIDEO_OUT_REFRESH_RATE_119_88HZ : VIDEO_OUT_REFRESH_RATE_59_94HZ;
    status->flags = 0;
    status->reserved[0] = 0;
    status->reserved[1] = 0;
    status->reserved[2] = 0;
    return 0;
}

int APS5_VABI sceVideoOutIsFlipPending(int handle) {
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    return cfg->flipStatus.flipPendingNum;
}

int APS5_VABI sceVideoOutWaitVblank(int handle) {
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    const uint64_t count = cfg->vblankStatus.count;
    while (cfg->opened && !cfg->failure && cfg->vblankStatus.count == count) {
        cfg->vblankCond.wait(lock);
    }
    if (cfg->failure) std::rethrow_exception(cfg->failure);
    if (!cfg->opened || cfg->closing) throw std::runtime_error("sceVideoOutWaitVblank: port closed during wait");
    return 0;
}

int APS5_VABI sceVideoOutInitializeOutputOptions(VideoOutOutputOptions* options) {
    if (options == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    std::memset(options, 0, sizeof(VideoOutOutputOptions));
    return 0;
}

int APS5_VABI sceVideoOutIsOutputSupported(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) {
    const int result = outputModeSupported(handle, mode, options, reservedPtr, reserved);
    static std::atomic<int> calls{0};
    if (traceCall(calls)) APS5_LOG_OUT("sceVideoOutIsOutputSupported handle=%d mode=0x%llx result=%d", handle, static_cast<unsigned long long>(mode), result);
    return result;
}

int APS5_VABI sceVideoOutConfigureOutput(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) {
    const int supported = outputModeSupported(handle, mode, options, reservedPtr, reserved);
    if (supported < 0) {
        return supported;
    }
    int result = 0;
    if (supported == 0 && mode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) {
        result = VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE;
    } else {
        auto cfg = VideoOutDriver::Get().GetConfig(handle);
        if (cfg == nullptr) {
            throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
        }
        std::unique_lock lock(cfg->mutex);
        cfg->Check();
        cfg->outputMode = mode;
    }
    static std::atomic<int> calls{0};
    if (traceCall(calls)) APS5_LOG_OUT("sceVideoOutConfigureOutput handle=%d mode=0x%llx result=0x%08x", handle, static_cast<unsigned long long>(mode), static_cast<unsigned>(result));
    static std::atomic<bool> explained{false};
    if (result == VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE && !explained.exchange(true)) APS5_LOG_OUT("119.88 Hz output refused on handle %d: the emulated display runs at 59.94 Hz, so the output stays at 59.94 Hz and the call returns VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE. The title asks for 119.88 Hz when it restores a 120 Hz display mode (Performance+, Fidelity+ or Variable Framerate), most likely one saved under another ANYPS5_DISPLAY profile; re-select the display mode in Options > Display", handle);
    return result;
}

int APS5_VABI sceVideoOutSetWindowModeMargins(int handle, int top, int bottom) {
    (void)top;
    (void)bottom;
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    throw std::runtime_error(std::string(__func__) + " not implemented");
}

int APS5_VABI sceVideoOutLatencyControlWaitBeforeInput(int handle) {
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    throw std::runtime_error(std::string(__func__) + " not implemented");
}

int APS5_VABI sceVideoOutLatencyMeasureSetStartPoint(int handle, uint32_t point) {
    (void)point;
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    throw std::runtime_error(std::string(__func__) + " not implemented");
}

int APS5_VABI sceVideoOutColorSettingsSetGamma(VideoOutColorSettings* settings, float gamma) {
    if (settings == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    if (!std::isfinite(gamma) || gamma < 0.1f || gamma > 2.0f) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    settings->gamma = gamma;
    return 0;
}

int APS5_VABI sceVideoOutAdjustColor(int handle, const VideoOutColorSettings* settings) {
    if (settings == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::lock_guard lock(cfg->mutex);
    cfg->Check();
    if (settings->gamma != 1.0f && settings->gamma != cfg->gamma) APS5_LOG_OUT("display gamma %.3f is recorded but not applied to presentation", settings->gamma);
    cfg->gamma = settings->gamma;
    return 0;
}

int APS5_VABI sceVideoOutColorSettingsSetGamma_(VideoOutColorSettings* settings, float gamma, uint32_t size) {
    if (size != sizeof(VideoOutColorSettings)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    return sceVideoOutColorSettingsSetGamma(settings, gamma);
}

int APS5_VABI sceVideoOutAdjustColor_(int handle, const VideoOutColorSettings* settings, uint32_t size) {
    if (size != sizeof(VideoOutColorSettings)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    return sceVideoOutAdjustColor(handle, settings);
}

int APS5_VABI sceVideoOutVrrPegToFixedRate(int handle, uint64_t first, uint64_t second) {
    static std::atomic<int> calls{0};
    if (traceCall(calls)) APS5_LOG_OUT("sceVideoOutVrrPegToFixedRate handle=%d first=0x%llx second=0x%llx", handle, static_cast<unsigned long long>(first), static_cast<unsigned long long>(second));
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    return 0;
}

int APS5_VABI sceVideoOutVrrUnpegFromFixedRate(int handle) {
    static std::atomic<int> calls{0};
    if (traceCall(calls)) APS5_LOG_OUT("sceVideoOutVrrUnpegFromFixedRate handle=%d", handle);
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    return 0;
}

}

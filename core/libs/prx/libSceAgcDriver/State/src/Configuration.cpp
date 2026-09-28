#include "prx/libSceAgcDriver/State/include/Configuration.hpp"

#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Tessellation configuration a title hands the driver before drawing: the tessellation factor
// ring and the hull-shader off-chip parameters. The executor does not run hardware tessellation
// stages yet, so the values are only recorded and validated.
namespace {

constexpr int SCE_AGC_ERROR_INVALID_ARGS = static_cast<int>(0x80D30001);

std::mutex configurationMutex;
const volatile void* tfRingBase = nullptr;
std::uint32_t tfRingSize = 0;
std::uint64_t hsOffchipParameters[3] = {};

}

extern "C" {

int APS5_VABI sceAgcDriverSetHsOffchipParam(uint64_t value0, uint64_t value1, uint64_t value2) {
    std::lock_guard lock(configurationMutex);
    hsOffchipParameters[0] = value0;
    hsOffchipParameters[1] = value1;
    hsOffchipParameters[2] = value2;
    return 0;
}

int APS5_VABI sceAgcDriverSetTFRing(const volatile void* base, uint32_t size) {
    if ((base == nullptr) != (size == 0)) return SCE_AGC_ERROR_INVALID_ARGS;
    if (base != nullptr && (reinterpret_cast<std::uintptr_t>(base) & 0xffu) != 0) return SCE_AGC_ERROR_INVALID_ARGS; // 256-byte aligned ring
    std::lock_guard lock(configurationMutex);
    tfRingBase = base;
    tfRingSize = size;
    return 0;
}

}

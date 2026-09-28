#include <cstdint>
#include <unistd.h>
#include <functional>
#include <thread>
#include <cstring>
#include <random>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"


extern "C" {

int APS5_VABI getargc_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

const char** APS5_VABI getargv_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI getpagesize_nid_postfix(void) {
 return PS5_PAGE_SIZE;
}

int APS5_VABI getpid_nid_postfix(void) {
    return static_cast<int>(::getpid());
}

void APS5_VABI exit_nid_postfix(int code) {
 (void)code;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetCurrentCpu(void) {
    // The host does not expose the executing core; spread callers over the console's seven cores.
    return static_cast<int>(std::hash<std::thread::id>{}(std::this_thread::get_id()) % 7);
}

uint64_t APS5_VABI sceKernelGetGPI(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI sceKernelSetGPO(uint32_t bits) {
 (void)bits;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetOpenPsId(void* open_ps_id) {
 (void)open_ps_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* APS5_VABI sceKernelGetProcParam(void) {
    return const_cast<void*>(ApplicationProcessParameters_nid_no_patch());
}

int APS5_VABI sceKernelUuidCreate(uint32_t* uuid) {
    if (uuid == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    // Version 4 (random) UUID in the console's 16-byte layout.
    std::random_device device;
    std::uint32_t words[4];
    for (auto& word : words) word = device();
    auto* bytes = reinterpret_cast<std::uint8_t*>(words);
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3f) | 0x80);
    std::memcpy(uuid, words, sizeof(words));
    return 0;
}

void APS5_VABI sceKernelSync(void) {
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sched_get_priority_max_nid_postfix(int policy) {
    if (policy < 0 || policy > 3) return -1;
    return 767; // SCE_KERNEL_PRIO_FIFO_LOWEST
}

int APS5_VABI sched_get_priority_min_nid_postfix(int policy) {
    if (policy < 0 || policy > 3) return -1;
    return 256; // SCE_KERNEL_PRIO_FIFO_HIGHEST
}

}

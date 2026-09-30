#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <unordered_set>
#include <unistd.h>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/GuestBufferAccess.hpp"

namespace {

constexpr int SCE_KERNEL_ERROR_ESRCH = static_cast<int>(0x80020003);
constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
constexpr std::uint32_t AioStateCompleted = 3;
constexpr std::int32_t AioMaximumRequests = 128;

std::mutex requestMutex;
std::unordered_set<std::int32_t> requests;
std::int32_t nextRequest = 1;

std::int64_t hostError() { return static_cast<std::int32_t>(0x80020000u | static_cast<unsigned>(errno & 0xff)); }

void execute(KernelAioRwRequest& request, bool write) {
    std::int64_t done = 0;
    auto* cursor = static_cast<std::byte*>(request.buf);
    PrepareGuestBuffer(request.buf, request.nbyte, !write);
    while (done < static_cast<std::int64_t>(request.nbyte)) {
        const auto remaining = request.nbyte - static_cast<std::size_t>(done);
        const auto offset = static_cast<off_t>(request.offset + done);
        const auto result = write ? ::pwrite(request.fd, cursor + done, remaining, offset) : ::pread(request.fd, cursor + done, remaining, offset);
        if (result < 0) {
            if (errno == EINTR) continue;
            done = hostError();
            break;
        }
        if (result == 0) break;
        done += result;
    }
    if (request.result != nullptr) {
        request.result->return_value = done;
        request.result->state = AioStateCompleted;
    }
}

int submit(KernelAioRwRequest* req, std::int32_t size, std::int32_t* id, bool write) {
    if (req == nullptr || id == nullptr || size <= 0 || size > AioMaximumRequests) return SCE_KERNEL_ERROR_EINVAL;
    for (std::int32_t index = 0; index < size; ++index)
        if (req[index].buf == nullptr && req[index].nbyte != 0) return SCE_KERNEL_ERROR_EINVAL;
    for (std::int32_t index = 0; index < size; ++index) execute(req[index], write);
    std::lock_guard lock(requestMutex);
    *id = nextRequest++;
    if (nextRequest <= 0) nextRequest = 1;
    requests.insert(*id);
    return 0;
}

bool known(std::int32_t id) {
    std::lock_guard lock(requestMutex);
    return requests.count(id) != 0;
}

}

extern "C" {

int APS5_VABI sceKernelAioDeleteRequest(int32_t id, int32_t* ret) {
    std::lock_guard lock(requestMutex);
    if (requests.erase(id) == 0) return SCE_KERNEL_ERROR_ESRCH;
    if (ret != nullptr) *ret = 0;
    return 0;
}

int APS5_VABI sceKernelAioInitializeImpl(void* param, int32_t size) {
    (void)param;
    (void)size;
    return 0;
}

void APS5_VABI sceKernelAioInitializeParam(void* param) {
    (void)param;
}

int APS5_VABI sceKernelAioSubmitReadCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    (void)prio;
    return submit(req, size, id, false);
}

int APS5_VABI sceKernelAioSubmitWriteCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    (void)prio;
    return submit(req, size, id, true);
}

int APS5_VABI sceKernelAioPollRequest(int32_t id, int32_t* state) {
    if (state == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    if (!known(id)) return SCE_KERNEL_ERROR_ESRCH;
    *state = static_cast<int32_t>(AioStateCompleted);
    return 0;
}

int APS5_VABI sceKernelAioWaitRequest(int32_t id, int32_t* state, uint32_t* usec) {
    (void)usec;
    if (state == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    if (!known(id)) return SCE_KERNEL_ERROR_ESRCH;
    *state = static_cast<int32_t>(AioStateCompleted);
    return 0;
}

}

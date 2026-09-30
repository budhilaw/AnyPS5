#include <atomic>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::mutex contextMutex;
std::set<AcmContextId> contexts;
AcmContextId nextContext = 1;
std::atomic<AcmBatchId> nextBatch{1};

void RequireContext(AcmContextId context, const char* function) {
    std::lock_guard lock(contextMutex);
    if (contexts.count(context) == 0) throw std::invalid_argument(std::string(function) + ": unknown ACM context");
}

void ReportSkippedJobs() {
    static std::atomic<bool> reported{false};
    if (!reported.exchange(true)) {
        APS5_LOG_CHARS_OUT("ACM batches are accepted but not executed; convolution output buffers are left unchanged");
    }
}

int StartBatch(AcmContextId context, AcmBatchId* batch, const char* function) {
    RequireContext(context, function);
    ReportSkippedJobs();
    if (batch != nullptr) *batch = nextBatch.fetch_add(1);
    return 0;
}

}

extern "C" {

int APS5_VABI sceAcmBatchStartBuffer(AcmContextId context, const void* batch_commands, size_t batch_size, AcmBatchError* batch_error, AcmBatchId* batch) {
 (void)batch_commands;
 (void)batch_size;
 (void)batch_error;
 return StartBatch(context, batch, __func__);
}

int APS5_VABI sceAcmBatchStartBuffers(AcmContextId context, uint32_t batch_info_count, const AcmBatchInfo* const batch_info[], AcmBatchError* batch_error, AcmBatchId* batch) {
 (void)batch_info_count;
 (void)batch_info;
 (void)batch_error;
 return StartBatch(context, batch, __func__);
}

int APS5_VABI sceAcmBatchWait(AcmContextId context, AcmBatchId batch, uint32_t timeout) {
 (void)batch;
 (void)timeout;
 RequireContext(context, __func__);
 return 0;
}

int APS5_VABI sceAcmContextCreate(AcmContextId* context) {
 if (context == nullptr) throw std::invalid_argument("sceAcmContextCreate: context is null");
 std::lock_guard lock(contextMutex);
 *context = nextContext++;
 contexts.insert(*context);
 return 0;
}

int APS5_VABI sceAcmContextDestroy(AcmContextId context) {
 std::lock_guard lock(contextMutex);
 if (contexts.erase(context) == 0) throw std::invalid_argument("sceAcmContextDestroy: unknown ACM context");
 return 0;
}

int APS5_VABI sceAcm_ConvReverb_SharedInput(AcmBatchInfo* batch, uint32_t reverb_count, const void* reverbs, uint32_t input_count, const void* inputs, const float* gains, const void* outputs) {
 (void)reverb_count;
 (void)reverbs;
 (void)input_count;
 (void)inputs;
 (void)gains;
 (void)outputs;
 if (batch == nullptr) throw std::invalid_argument("sceAcm_ConvReverb_SharedInput: batch is null");
 ReportSkippedJobs();
 return 0;
}

}

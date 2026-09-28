#include <map>
// The audio job manager, shared by libSceAjm, libSceAjm.native and libSceAjmi (the same API
// under three module names). The host has no codec hardware: batches execute synchronously at
// start and every decode job produces silence while reporting its whole input as consumed, so a
// title's audio pipeline keeps streaming without sound. Real decoding is tracked in
// docs/TechnicalDebt.md.
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <set>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_AJM_ERROR_INVALID_CONTEXT = static_cast<int>(0x80930002);
constexpr int SCE_AJM_ERROR_INVALID_INSTANCE = static_cast<int>(0x80930003);
constexpr int SCE_AJM_ERROR_INVALID_PARAMETER = static_cast<int>(0x80930004);
constexpr int SCE_AJM_ERROR_OUT_OF_MEMORY = static_cast<int>(0x80930006);
constexpr int SCE_AJM_ERROR_INVALID_BATCH = static_cast<int>(0x80930008);
constexpr std::uint64_t SCE_AJM_FLAG_SIDEBAND_STREAM = 1ull << 46;
constexpr std::uint64_t SCE_AJM_FLAG_SIDEBAND_FORMAT = 1ull << 47;
constexpr std::uint64_t SCE_AJM_FLAG_SIDEBAND_GAPLESS_DECODE = 1ull << 45;

// A job appended to a batch buffer by the sceAjmBatchJob* writers.
struct Job {
    enum Kind : std::uint32_t { Control, Run, Initialize, Other, Clear } kind;
    std::uint32_t instance;
    std::uint64_t flags;
    const void* input; std::size_t inputSize;
    void* output; std::size_t outputSize;
    void* sideband; std::size_t sidebandSize;
    std::vector<AjmBuffer> inputs, outputs;
};

struct Batch { std::vector<Job> jobs; };

std::mutex mutex;
std::set<std::uint32_t> contexts;
std::set<std::uint32_t> instances;
// Per decoder instance: the channel count (from an ATRAC9 initialization) and the samples per
// channel decoded so far, which stream sidebands report and titles track progress with.
struct InstanceState { std::uint32_t channels = 1; std::uint64_t decoded = 0; };
std::map<std::uint32_t, InstanceState> instanceStates;
std::uint32_t nextContext = 1, nextInstance = 1, nextBatch = 1;

// Batches are keyed by their buffer: the job list lives on the host side and the guest buffer
// only carries the key so that any batch buffer size works.
std::map<void*, Batch>& batches() { static auto* value = new std::map<void*, Batch>; return *value; }

int append(AjmBatchInfo* info, Job job) {
    if (info == nullptr || info->p_buffer == nullptr) return SCE_AJM_ERROR_INVALID_PARAMETER;
    std::lock_guard lock(mutex);
    if (instances.count(job.instance) == 0 && job.kind != Job::Other && job.kind != Job::Clear) return SCE_AJM_ERROR_INVALID_INSTANCE;
    batches()[info->p_buffer].jobs.push_back(std::move(job));
    info->offset = std::min<std::uint64_t>(info->size, info->offset + 64);
    return 0;
}

void complete(const Job& job) {
    if (job.kind == Job::Initialize) {
        // ATRAC9 initialization parameters start with the 4-byte config word (sync byte 0xFE).
        const auto* config = static_cast<const std::uint8_t*>(job.input);
        auto& state = instanceStates[job.instance];
        state.decoded = 0;
        if (config != nullptr && job.inputSize >= 4 && config[0] == 0xFE) {
            static constexpr std::uint32_t channels[8] = {1, 2, 2, 6, 8, 4, 4, 2};
            state.channels = channels[(config[1] >> 1) & 0x7];
        }
    }
    if (job.kind == Job::Clear) instanceStates[job.instance].decoded = 0; // a new stream starts
    if (job.kind == Job::Control && (job.flags & 0x1ull) != 0) instanceStates[job.instance].decoded = 0; // control reset
    if (job.output != nullptr && job.outputSize != 0) std::memset(job.output, 0, job.outputSize);
    std::size_t outputTotal = job.outputSize, inputTotal = job.inputSize;
    for (const auto& buffer : job.outputs) { if (buffer.ptr != nullptr) std::memset(buffer.ptr, 0, buffer.size); outputTotal += buffer.size; }
    for (const auto& buffer : job.inputs) inputTotal += buffer.size;
    if (job.sideband == nullptr || job.sidebandSize == 0) return;
    std::memset(job.sideband, 0, job.sidebandSize); // AjmSidebandResult {result, internal} = success
    auto* cursor = static_cast<std::uint8_t*>(job.sideband) + 8;
    const auto* end = static_cast<std::uint8_t*>(job.sideband) + job.sidebandSize;
    if (job.kind == Job::Run && (job.flags & SCE_AJM_FLAG_SIDEBAND_STREAM) && cursor + 24 <= end) {
        // AjmSidebandStream {int32 input_consumed; int32 output_written; uint64 total_decoded_samples}
        const auto consumed = static_cast<std::int32_t>(inputTotal), written = static_cast<std::int32_t>(outputTotal);
        auto& state = instanceStates[job.instance];
        // 16-bit PCM output: the samples per channel this job produced.
        state.decoded += outputTotal / (static_cast<std::size_t>(state.channels) * 2u);
        std::memcpy(cursor, &consumed, 4);
        std::memcpy(cursor + 4, &written, 4);
        static const bool noTotal = std::getenv("ANYPS5_AJM_NO_TOTAL") != nullptr; // diagnostics
        if (!noTotal) std::memcpy(cursor + 8, &state.decoded, 8);
        cursor += 24;
    }
    (void)SCE_AJM_FLAG_SIDEBAND_FORMAT; (void)SCE_AJM_FLAG_SIDEBAND_GAPLESS_DECODE; // zeroed above
}

}

extern "C" {

int APS5_VABI sceAjmInitialize(int64_t reserved, uint32_t* context) {
    (void)reserved;
    if (context == nullptr) return SCE_AJM_ERROR_INVALID_PARAMETER;
    std::lock_guard lock(mutex);
    *context = nextContext++;
    contexts.insert(*context);
    return 0;
}

int APS5_VABI sceAjmFinalize(uint32_t context) {
    std::lock_guard lock(mutex);
    return contexts.erase(context) != 0 ? 0 : SCE_AJM_ERROR_INVALID_CONTEXT;
}

int APS5_VABI sceAjmModuleRegister(uint32_t context, uint32_t codec, int64_t reserved) {
    (void)codec; (void)reserved;
    std::lock_guard lock(mutex);
    return contexts.count(context) != 0 ? 0 : SCE_AJM_ERROR_INVALID_CONTEXT;
}

int APS5_VABI sceAjmModuleUnregister(uint32_t context, uint32_t codec) {
    (void)codec;
    std::lock_guard lock(mutex);
    return contexts.count(context) != 0 ? 0 : SCE_AJM_ERROR_INVALID_CONTEXT;
}

int APS5_VABI sceAjmMemoryRegister(uint32_t context, void* ptr, size_t pages) {
    if (ptr == nullptr || pages == 0) return SCE_AJM_ERROR_INVALID_PARAMETER;
    std::lock_guard lock(mutex);
    return contexts.count(context) != 0 ? 0 : SCE_AJM_ERROR_INVALID_CONTEXT;
}

int APS5_VABI sceAjmMemoryUnregister(uint32_t context, void* ptr) {
    if (ptr == nullptr) return SCE_AJM_ERROR_INVALID_PARAMETER;
    std::lock_guard lock(mutex);
    return contexts.count(context) != 0 ? 0 : SCE_AJM_ERROR_INVALID_CONTEXT;
}

int APS5_VABI sceAjmInstanceCreate(uint32_t context, uint32_t codec, uint64_t flags, uint32_t* instance) {
    (void)codec; (void)flags;
    if (instance == nullptr) return SCE_AJM_ERROR_INVALID_PARAMETER;
    std::lock_guard lock(mutex);
    if (contexts.count(context) == 0) return SCE_AJM_ERROR_INVALID_CONTEXT;
    *instance = nextInstance++;
    instances.insert(*instance);
    return 0;
}

int APS5_VABI sceAjmInstanceDestroy(uint32_t context, uint32_t instance) {
    std::lock_guard lock(mutex);
    if (contexts.count(context) == 0) return SCE_AJM_ERROR_INVALID_CONTEXT;
    instanceStates.erase(instance);
    return instances.erase(instance) != 0 ? 0 : SCE_AJM_ERROR_INVALID_INSTANCE;
}

int APS5_VABI sceAjmBatchInitialize(void* buffer, size_t size, AjmBatchInfo* info) {
    if (buffer == nullptr || size == 0 || info == nullptr) return SCE_AJM_ERROR_INVALID_PARAMETER;
    info->p_buffer = buffer;
    info->offset = 0;
    info->size = size;
    std::lock_guard lock(mutex);
    batches()[buffer].jobs.clear();
    return 0;
}

int APS5_VABI sceAjmBatchJobControl(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* sideband_input, size_t sideband_input_size, void* sideband_output, size_t sideband_output_size) {
    return append(info, Job{Job::Control, instance, flags, sideband_input, sideband_input_size, nullptr, 0, sideband_output, sideband_output_size, {}, {}});
}

int APS5_VABI sceAjmBatchJobInitialize(AjmBatchInfo* info, uint32_t instance, const void* codec_parameters, size_t codec_parameters_size, void* result) {
    return append(info, Job{Job::Initialize, instance, 0, codec_parameters, codec_parameters_size, nullptr, 0, result, result != nullptr ? 8u : 0u, {}, {}});
}

int APS5_VABI sceAjmBatchJobClearContext(AjmBatchInfo* info, uint32_t instance, void* result) {
    return append(info, Job{Job::Clear, instance, 0, nullptr, 0, nullptr, 0, result, result != nullptr ? 8u : 0u, {}, {}});
}

int APS5_VABI sceAjmBatchJobRun(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size) {
    return append(info, Job{Job::Run, instance, flags, data_input, data_input_size, data_output, data_output_size, sideband_output, sideband_output_size, {}, {}});
}

int APS5_VABI sceAjmBatchJobRunSplit(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size) {
    if ((input_buffers == nullptr && input_buffers_num != 0) || (output_buffers == nullptr && output_buffers_num != 0)) return SCE_AJM_ERROR_INVALID_PARAMETER;
    Job job{Job::Run, instance, flags, nullptr, 0, nullptr, 0, sideband_output, sideband_output_size, {}, {}};
    job.inputs.assign(input_buffers, input_buffers + input_buffers_num);
    job.outputs.assign(output_buffers, output_buffers + output_buffers_num);
    return append(info, std::move(job));
}

int APS5_VABI sceAjmBatchJobDecode(AjmBatchInfo* info, uint32_t instance, const void* bitstream_input, size_t bitstream_input_size, void* pcm_output, size_t pcm_output_size, void* result) {
    return sceAjmBatchJobRun(info, instance, SCE_AJM_FLAG_SIDEBAND_STREAM, bitstream_input, bitstream_input_size, pcm_output, pcm_output_size, result, result != nullptr ? 32u : 0u);
}

int APS5_VABI sceAjmBatchJobDecodeSingle(AjmBatchInfo* info, uint32_t instance, const void* bitstream_input, size_t bitstream_input_size, void* pcm_output, size_t pcm_output_size, void* result) {
    return sceAjmBatchJobDecode(info, instance, bitstream_input, bitstream_input_size, pcm_output, pcm_output_size, result);
}

int APS5_VABI sceAjmBatchJobDecodeSplit(AjmBatchInfo* info, uint32_t instance, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* result) {
    return sceAjmBatchJobRunSplit(info, instance, SCE_AJM_FLAG_SIDEBAND_STREAM, input_buffers, input_buffers_num, output_buffers, output_buffers_num, result, result != nullptr ? 32u : 0u);
}

int APS5_VABI sceAjmBatchJobEncode(AjmBatchInfo* info, uint32_t instance, const void* pcm_input, size_t pcm_input_size, void* bitstream_output, size_t bitstream_output_size, void* result) {
    return sceAjmBatchJobRun(info, instance, SCE_AJM_FLAG_SIDEBAND_STREAM, pcm_input, pcm_input_size, bitstream_output, bitstream_output_size, result, result != nullptr ? 32u : 0u);
}

int APS5_VABI sceAjmBatchJobGetCodecInfo(AjmBatchInfo* info, uint32_t instance, void* result, size_t result_size) {
    return append(info, Job{Job::Other, instance, 0, nullptr, 0, nullptr, 0, result, result_size, {}, {}});
}

int APS5_VABI sceAjmBatchJobGetGaplessDecode(AjmBatchInfo* info, uint32_t instance, void* result) { return append(info, Job{Job::Other, instance, 0, nullptr, 0, nullptr, 0, result, result != nullptr ? 24u : 0u, {}, {}}); }
int APS5_VABI sceAjmBatchJobGetInfo(AjmBatchInfo* info, uint32_t instance, void* result) { return append(info, Job{Job::Other, instance, 0, nullptr, 0, nullptr, 0, result, result != nullptr ? 8u : 0u, {}, {}}); }
int APS5_VABI sceAjmBatchJobGetResampleInfo(AjmBatchInfo* info, uint32_t instance, void* result) { return append(info, Job{Job::Other, instance, 0, nullptr, 0, nullptr, 0, result, result != nullptr ? 8u : 0u, {}, {}}); }
int APS5_VABI sceAjmBatchJobGetStatistics(AjmBatchInfo* info, float interval, void* result) { (void)interval; return append(info, Job{Job::Other, 0, 0, nullptr, 0, nullptr, 0, result, result != nullptr ? 8u : 0u, {}, {}}); }
int APS5_VABI sceAjmBatchJobSetGaplessDecode(AjmBatchInfo* info, uint32_t instance, const void* gapless_decode, int reset, void* result) { (void)reset; return append(info, Job{Job::Other, instance, 0, gapless_decode, 0, nullptr, 0, result, result != nullptr ? 8u : 0u, {}, {}}); }
int APS5_VABI sceAjmBatchJobSetResampleParameters(AjmBatchInfo* info, uint32_t instance, float ratio, uint32_t flags, void* result) { (void)ratio; (void)flags; return append(info, Job{Job::Other, instance, 0, nullptr, 0, nullptr, 0, result, result != nullptr ? 8u : 0u, {}, {}}); }
int APS5_VABI sceAjmBatchJobSetResampleParametersEx(AjmBatchInfo* info, uint32_t instance, float ratio_start, float ratio_change_per_sample, uint32_t flags, void* result) { (void)ratio_start; (void)ratio_change_per_sample; (void)flags; return append(info, Job{Job::Other, instance, 0, nullptr, 0, nullptr, 0, result, result != nullptr ? 8u : 0u, {}, {}}); }

int APS5_VABI sceAjmBatchStart(uint32_t context, const AjmBatchInfo* info, int priority, AjmBatchError* error, uint32_t* batch) {
    (void)priority;
    if (info == nullptr || batch == nullptr) return SCE_AJM_ERROR_INVALID_PARAMETER;
    std::vector<Job> jobs;
    {
        std::lock_guard lock(mutex);
        if (contexts.count(context) == 0) return SCE_AJM_ERROR_INVALID_CONTEXT;
        const auto found = batches().find(info->p_buffer);
        if (found == batches().end()) return SCE_AJM_ERROR_INVALID_BATCH;
        jobs.swap(found->second.jobs);
        *batch = nextBatch++;
    }
    {
        std::lock_guard lock(mutex);
        for (const auto& job : jobs) complete(job);
    }
    if (error != nullptr) *error = AjmBatchError{};
    {
        // Diagnostics: how busy the (silent) decoder is.
        static std::uint64_t started = 0, jobsRun = 0;
        ++started; jobsRun += jobs.size();
        if (started <= 4) for (const auto& job : jobs) {
            std::string bytes;
            const auto* in = static_cast<const std::uint8_t*>(job.input);
            for (std::size_t i = 0; in != nullptr && i < std::min<std::size_t>(job.inputSize, 16); ++i) { char item[4]; std::snprintf(item, sizeof(item), "%02x", in[i]); bytes += item; }
            APS5_LOG_OUT("ajm job: kind %u instance %u flags 0x%llx input %zu [%s] output %zu sideband %zu", static_cast<unsigned>(job.kind), job.instance, static_cast<unsigned long long>(job.flags), job.inputSize, bytes.c_str(), job.outputSize, job.sidebandSize);
        }
        if ((started & (started - 1)) == 0) APS5_LOG_OUT("ajm: %llu batches, %llu jobs (last batch: %zu jobs, first kind %u flags 0x%llx in %zu out %zu sideband %zu)", static_cast<unsigned long long>(started), static_cast<unsigned long long>(jobsRun), jobs.size(), jobs.empty() ? 0u : static_cast<unsigned>(jobs.front().kind), jobs.empty() ? 0ull : static_cast<unsigned long long>(jobs.front().flags), jobs.empty() ? 0 : jobs.front().inputSize, jobs.empty() ? 0 : jobs.front().outputSize, jobs.empty() ? 0 : jobs.front().sidebandSize);
    }
    return 0;
}

int APS5_VABI sceAjmBatchWait(uint32_t context, uint32_t batch, uint32_t timeout, AjmBatchError* error) {
    (void)batch; (void)timeout;
    std::lock_guard lock(mutex);
    if (contexts.count(context) == 0) return SCE_AJM_ERROR_INVALID_CONTEXT;
    if (error != nullptr) *error = AjmBatchError{};
    return 0; // batches complete at start
}

int APS5_VABI sceAjmBatchCancel(uint32_t context, uint32_t batch) {
    (void)batch;
    std::lock_guard lock(mutex);
    return contexts.count(context) != 0 ? 0 : SCE_AJM_ERROR_INVALID_CONTEXT;
}

int APS5_VABI sceAjmBatchErrorDump(const AjmBatchInfo* info, AjmBatchError* error) {
    if (info == nullptr || error == nullptr) return SCE_AJM_ERROR_INVALID_PARAMETER;
    *error = AjmBatchError{};
    return 0;
}

int APS5_VABI sceAjmDecAt9ParseConfigData(const void* config_data, AjmDecAt9ConfigDataInfo* config_info) {
    if (config_data == nullptr || config_info == nullptr) return SCE_AJM_ERROR_INVALID_PARAMETER;
    // ATRAC9 config data: sync byte, sample rate index, channel config index, frame size, superframe index.
    const auto* c = static_cast<const std::uint8_t*>(config_data);
    if (c[0] != 0xFE) return SCE_AJM_ERROR_INVALID_PARAMETER;
    static constexpr std::uint32_t sampleRates[16] = {11025, 22050, 44100, 88200, 176400, 12000, 24000, 48000, 96000, 192000, 16000, 32000, 64000, 128000, 256000, 0};
    static constexpr std::uint32_t frameSamplesPower[16] = {6, 6, 7, 8, 8, 6, 6, 7, 8, 8, 7, 7, 8, 9, 9, 9};
    static constexpr std::uint32_t channels[8] = {1, 2, 2, 6, 8, 4, 4, 2};
    const auto rateIndex = (c[1] >> 4) & 0xF;
    const auto channelIndex = (c[1] >> 1) & 0x7;
    const auto frameBytes = (((c[1] & 1u) << 7) | (c[2] >> 1)) + 1u;
    const auto framesPerSuperframe = 1u << ((c[3] >> 5) & 0x3);
    if (sampleRates[rateIndex] == 0) return SCE_AJM_ERROR_INVALID_PARAMETER;
    config_info->channels = channels[channelIndex];
    config_info->sample_rate = sampleRates[rateIndex];
    config_info->frame_samples_per_channel = 1u << frameSamplesPower[rateIndex];
    config_info->superframe_samples_per_channel = config_info->frame_samples_per_channel * framesPerSuperframe;
    config_info->superframe_size = frameBytes * framesPerSuperframe;
    return 0;
}

}

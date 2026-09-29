#include <map>
// The audio job manager, shared by libSceAjm, libSceAjm.native and libSceAjmi (the same API
// under three module names). Batches run synchronously at start; ATRAC9 decodes with LibAtrac9.
#include <array>
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
#include "libatrac9.h"

namespace {

constexpr int SCE_AJM_ERROR_INVALID_CONTEXT = static_cast<int>(0x80930002);
constexpr int SCE_AJM_ERROR_INVALID_INSTANCE = static_cast<int>(0x80930003);
constexpr int SCE_AJM_ERROR_INVALID_PARAMETER = static_cast<int>(0x80930004);
constexpr int SCE_AJM_ERROR_OUT_OF_MEMORY = static_cast<int>(0x80930006);
constexpr int SCE_AJM_ERROR_INVALID_BATCH = static_cast<int>(0x80930008);
constexpr std::uint64_t SCE_AJM_FLAG_SIDEBAND_STREAM = 1ull << 47;
constexpr std::uint64_t SCE_AJM_FLAG_SIDEBAND_FORMAT = 1ull << 46;
constexpr std::uint64_t SCE_AJM_FLAG_SIDEBAND_GAPLESS_DECODE = 1ull << 45;

// A job appended to a batch buffer by the sceAjmBatchJob* writers.
struct Job {
    enum Kind : std::uint32_t { Control, Run, Initialize, Other, Clear, Gapless } kind;
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
// Per decoder instance: the ATRAC9 decoder and the samples per channel decoded so far, which
// stream sidebands report and titles track progress with.
struct InstanceState {
    std::uint32_t channels = 1;
    std::uint64_t decoded = 0;
    std::uint32_t format = 0;  // output encoding from the instance flags: 0 s16, 1 s32, 2 float
    void* decoder = nullptr;
    Atrac9CodecInfo info{};
    std::uint32_t skip = 0;  // gapless: samples per channel still to drop at the stream start
    std::uint32_t gaplessTotal = 0;
    std::uint16_t gaplessSkip = 0;
    // Decoded samples waiting for output room (their input was reported consumed).
    std::vector<std::uint8_t> pendingPcm;
    std::uint32_t id = 0;
    // The last jobs, printed when a superframe fails to decode (diagnostics).
    std::array<std::string, 8> recent;
    std::size_t nextRecent = 0;
};
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

// Restarts the stream with the configuration the decoder was initialized with.
void resetDecoder(InstanceState& state) {
    state.pendingPcm.clear();
    if (state.decoder != nullptr) Atrac9InitDecoder(state.decoder, state.info.configData);
}

void restart(InstanceState& state) {
    state.decoded = 0;
    resetDecoder(state);
}

void initialize(InstanceState& state, const std::uint8_t* config, std::size_t bytes) {
    state.decoded = 0;
    state.skip = 0;
    state.pendingPcm.clear();
    if (config == nullptr || bytes < ATRAC9_CONFIG_DATA_SIZE || config[0] != 0xFE) return;
    if (state.decoder == nullptr) state.decoder = Atrac9GetHandle();
    unsigned char data[ATRAC9_CONFIG_DATA_SIZE];
    std::memcpy(data, config, sizeof(data));
    if (state.decoder == nullptr || Atrac9InitDecoder(state.decoder, data) != 0 || Atrac9GetCodecInfo(state.decoder, &state.info) != 0) {
        APS5_LOG_OUT("ajm: ATRAC9 config %02x%02x%02x%02x rejected", config[0], config[1], config[2], config[3]);
        state.info = {};
        return;
    }
    state.channels = static_cast<std::uint32_t>(state.info.channels);
    static int reported = 0;
    if (reported++ < 64) APS5_LOG_OUT("ajm: ATRAC9 %d channels %d Hz, superframe %d bytes of %d frames (config %02x%02x%02x%02x)", state.info.channels, state.info.samplingRate, state.info.superframeSize, state.info.framesInSuperframe, config[0], config[1], config[2], config[3]);
}

// Decodes the whole superframes at the start of `input` while their samples find room, as the
// hardware does: the title keeps a cut superframe and sends it again with the bytes that follow.
std::pair<std::size_t, std::size_t> decode(InstanceState& state, const std::vector<std::uint8_t>& input, std::size_t bytes, const std::vector<std::pair<std::uint8_t*, std::size_t>>& output) {
    const std::size_t sampleBytes = state.format == 0 ? 2 : 4;
    const auto& info = state.info;
    std::size_t room = 0;
    for (const auto& [pointer, size] : output) room += pointer != nullptr ? size : 0;
    std::size_t consumed = 0;
    if (state.decoder != nullptr && info.superframeSize > 0 && info.framesInSuperframe > 0) {
        const auto superframeBytes = static_cast<std::size_t>(info.superframeSize);
        const auto frameBytes = static_cast<std::size_t>(info.frameSamples) * info.channels * sampleBytes;
        std::vector<std::uint8_t> frame(frameBytes);
        // LibAtrac9's Huffman reader peeks a few bytes past the frame it decodes.
        std::vector<std::uint8_t> superframe(superframeBytes + 16, 0);
        while (bytes - consumed >= superframeBytes && state.pendingPcm.size() < room) {
            std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(consumed), superframeBytes, superframe.begin());
            consumed += superframeBytes;
            std::size_t used = 0;
            for (int index = 0; index < info.framesInSuperframe; ++index) {
                int frameUsed = 0;
                const auto* bits = superframe.data() + used;
                const int status = used >= superframeBytes ? -1
                    : state.format == 0 ? Atrac9Decode(state.decoder, bits, reinterpret_cast<short*>(frame.data()), &frameUsed, 0)
                    : state.format == 1 ? Atrac9DecodeS32(state.decoder, bits, reinterpret_cast<int*>(frame.data()), &frameUsed, 0)
                    : Atrac9DecodeF32(state.decoder, bits, reinterpret_cast<float*>(frame.data()), &frameUsed, 0);
                if (status != 0 || frameUsed <= 0) {
                    static int reported = 0;
                    if (reported++ < 16) {
                        APS5_LOG_OUT("ajm: ATRAC9 frame %d of a superframe failed (%d) on instance %u at byte %zu of %zu; the decoder restarts. Its last jobs:", index, status, state.id, consumed - superframeBytes, bytes);
                        for (std::size_t i = 0; i < state.recent.size(); ++i) {
                            const auto& line = state.recent[(state.nextRecent + i) % state.recent.size()];
                            if (!line.empty()) APS5_LOG_OUT("ajm:   %s", line.c_str());
                        }
                    }
                    Atrac9InitDecoder(state.decoder, state.info.configData);
                    break;
                }
                used += static_cast<std::size_t>(frameUsed);
                const auto dropped = std::min<std::uint32_t>(state.skip, static_cast<std::uint32_t>(info.frameSamples));
                state.skip -= dropped;
                state.pendingPcm.insert(state.pendingPcm.end(), frame.begin() + dropped * info.channels * sampleBytes, frame.end());
            }
        }
    }
    std::size_t cursor = 0;
    for (const auto& [pointer, size] : output) {
        if (pointer == nullptr) continue;
        const auto copied = std::min(size, state.pendingPcm.size() - cursor);
        if (copied != 0) std::memcpy(pointer, state.pendingPcm.data() + cursor, copied);
        std::memset(pointer + copied, 0, size - copied);
        cursor += copied;
    }
    state.pendingPcm.erase(state.pendingPcm.begin(), state.pendingPcm.begin() + static_cast<std::ptrdiff_t>(cursor));
    return {consumed, cursor};
}

void complete(const Job& job) {
    auto& state = instanceStates[job.instance];
    state.id = job.instance;
    {
        std::size_t inputTotal = job.inputSize;
        for (const auto& buffer : job.inputs) inputTotal += buffer.size;
        char line[160];
        const auto* in = static_cast<const std::uint8_t*>(job.input);
        std::snprintf(line, sizeof(line), "kind %u flags 0x%llx input %zu (%zu buffers) head %02x%02x%02x%02x output %zu, %zu samples bytes waiting", static_cast<unsigned>(job.kind), static_cast<unsigned long long>(job.flags), inputTotal, job.inputs.size(),
            in != nullptr && job.inputSize >= 4 ? in[0] : 0, in != nullptr && job.inputSize >= 4 ? in[1] : 0, in != nullptr && job.inputSize >= 4 ? in[2] : 0, in != nullptr && job.inputSize >= 4 ? in[3] : 0, job.outputSize, state.pendingPcm.size());
        state.recent[state.nextRecent++ % state.recent.size()] = line;
    }
    // ANYPS5_TRACE_AJM=1: every job but decodes, and decode sizes that are not whole superframes.
    static const bool trace = std::getenv("ANYPS5_TRACE_AJM") != nullptr;
    if (trace) {
        std::string bytes;
        const auto* in = static_cast<const std::uint8_t*>(job.input);
        for (std::size_t i = 0; in != nullptr && i < std::min<std::size_t>(job.inputSize, 16); ++i) { char item[4]; std::snprintf(item, sizeof(item), "%02x", in[i]); bytes += item; }
        APS5_LOG_OUT("ajm trace: instance %u kind %u flags 0x%llx input %zu [%s] split %zu/%zu output %zu superframe %d", job.instance, static_cast<unsigned>(job.kind), static_cast<unsigned long long>(job.flags), job.inputSize, bytes.c_str(), job.inputs.size(), job.outputs.size(), job.outputSize, state.info.superframeSize);
    }
    if (job.kind == Job::Initialize) initialize(state, static_cast<const std::uint8_t*>(job.input), job.inputSize);
    if (job.kind == Job::Clear) restart(state); // a new stream starts
    if (job.kind == Job::Control && (job.flags & 0x1ull) != 0) restart(state); // control reset
    if (job.kind == Job::Gapless && job.input != nullptr && job.inputSize >= 8) {
        // AjmSidebandGaplessDecode {uint32 total_samples; uint16 skip_samples; uint16 skipped_samples}
        std::uint16_t skip = 0, skipped = 0;
        std::memcpy(&skip, static_cast<const std::uint8_t*>(job.input) + 4, 2);
        std::memcpy(&skipped, static_cast<const std::uint8_t*>(job.input) + 6, 2);
        std::memcpy(&state.gaplessTotal, job.input, 4);
        state.gaplessSkip = skip;
        state.skip = skip > skipped ? skip - skipped : 0;
    }
    std::size_t inputTotal = job.inputSize;
    for (const auto& buffer : job.inputs) inputTotal += buffer.size;
    std::pair<std::size_t, std::size_t> progress{0, 0};
    if (job.kind == Job::Run) {
        // LibAtrac9's Huffman reader peeks a few bytes past the frame it decodes.
        std::vector<std::uint8_t> input;
        input.reserve(inputTotal + 16);
        if (job.input != nullptr) input.insert(input.end(), static_cast<const std::uint8_t*>(job.input), static_cast<const std::uint8_t*>(job.input) + job.inputSize);
        for (const auto& buffer : job.inputs) if (buffer.ptr != nullptr) input.insert(input.end(), static_cast<const std::uint8_t*>(buffer.ptr), static_cast<const std::uint8_t*>(buffer.ptr) + buffer.size);
        const auto data = input.size();
        input.resize(data + 16, 0);
        std::vector<std::pair<std::uint8_t*, std::size_t>> output;
        if (job.output != nullptr) output.emplace_back(static_cast<std::uint8_t*>(job.output), job.outputSize);
        for (const auto& buffer : job.outputs) output.emplace_back(static_cast<std::uint8_t*>(buffer.ptr), buffer.size);
        progress = decode(state, input, data, output);
        // ANYPS5_DUMP_AUDIO=<directory>: each instance's decoded PCM, raw (diagnostics).
        if (static const char* directory = std::getenv("ANYPS5_DUMP_AUDIO"); directory != nullptr) {
            static std::map<std::uint32_t, FILE*> files, inputs;
            auto& file = files[job.instance];
            if (file == nullptr) file = std::fopen((std::string(directory) + "/ajm_" + std::to_string(job.instance) + ".raw").c_str(), "wb");
            auto& consumed = inputs[job.instance];
            if (consumed == nullptr) consumed = std::fopen((std::string(directory) + "/ajm_" + std::to_string(job.instance) + ".in").c_str(), "wb");
            if (consumed != nullptr) std::fwrite(input.data(), 1, progress.first, consumed);
            std::size_t left = progress.second;
            for (const auto& [pointer, size] : output) {
                if (pointer == nullptr || left == 0) continue;
                const auto count = std::min(size, left);
                if (file != nullptr) std::fwrite(pointer, 1, count, file);
                left -= count;
            }
        }
    } else {
        if (job.output != nullptr && job.outputSize != 0) std::memset(job.output, 0, job.outputSize);
        for (const auto& buffer : job.outputs) if (buffer.ptr != nullptr) std::memset(buffer.ptr, 0, buffer.size);
    }
    if (job.sideband == nullptr || job.sidebandSize == 0) return;
    std::memset(job.sideband, 0, job.sidebandSize); // AjmSidebandResult {result, internal} = success
    auto* cursor = static_cast<std::uint8_t*>(job.sideband) + 8;
    const auto* end = static_cast<std::uint8_t*>(job.sideband) + job.sidebandSize;
    if (job.kind != Job::Run) return;
    const std::size_t sampleBytes = state.format == 0 ? 2 : 4;
    state.decoded += progress.second / (static_cast<std::size_t>(state.channels) * sampleBytes);
    if ((job.flags & SCE_AJM_FLAG_SIDEBAND_STREAM) && cursor + 16 <= end) {
        // AjmSidebandStream {int32 input_consumed; int32 output_written; uint64 total_decoded_samples}
        const auto consumed = static_cast<std::int32_t>(progress.first), written = static_cast<std::int32_t>(progress.second);
        std::memcpy(cursor, &consumed, 4);
        std::memcpy(cursor + 4, &written, 4);
        std::memcpy(cursor + 8, &state.decoded, 8);
        cursor += 16;
    }
    if ((job.flags & SCE_AJM_FLAG_SIDEBAND_FORMAT) && cursor + 24 <= end) {
        // AjmSidebandFormat {channels, channel_mask, sample_rate, sample_encoding, bitrate, reserved}
        const auto& info = state.info;
        const std::uint32_t mask = state.channels == 1 ? 0x4u : state.channels == 2 ? 0x3u : (1u << state.channels) - 1u;
        const std::uint32_t samples = static_cast<std::uint32_t>(info.frameSamples * info.framesInSuperframe);
        const std::uint32_t bitrate = samples != 0 ? static_cast<std::uint32_t>(static_cast<std::uint64_t>(info.superframeSize) * 8u * info.samplingRate / samples) : 0u;
        const std::uint32_t fields[6] = {state.channels, mask, static_cast<std::uint32_t>(info.samplingRate), state.format, bitrate, 0};
        std::memcpy(cursor, fields, sizeof(fields));
        cursor += sizeof(fields);
    }
    if ((job.flags & SCE_AJM_FLAG_SIDEBAND_GAPLESS_DECODE) && cursor + 8 <= end) {
        // AjmSidebandGaplessDecode {uint32 total_samples; uint16 skip_samples; uint16 skipped_samples}
        const std::uint16_t skipped = static_cast<std::uint16_t>(state.gaplessSkip - std::min<std::uint32_t>(state.skip, state.gaplessSkip));
        std::memcpy(cursor, &state.gaplessTotal, 4);
        std::memcpy(cursor + 4, &state.gaplessSkip, 2);
        std::memcpy(cursor + 6, &skipped, 2);
    }
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
    if (instance == nullptr) return SCE_AJM_ERROR_INVALID_PARAMETER;
    std::lock_guard lock(mutex);
    if (contexts.count(context) == 0) return SCE_AJM_ERROR_INVALID_CONTEXT;
    *instance = nextInstance++;
    instances.insert(*instance);
    // AjmInstanceFlags: version (3 bits), channels (4), output format (3), ...
    instanceStates[*instance].format = static_cast<std::uint32_t>((flags >> 7) & 0x7) <= 2 ? static_cast<std::uint32_t>((flags >> 7) & 0x7) : 0;
    static std::set<std::uint64_t> reportedFlags;
    if (reportedFlags.insert((static_cast<std::uint64_t>(codec) << 48) ^ flags).second) APS5_LOG_OUT("ajm: instance %u codec %u flags 0x%llx", *instance, codec, static_cast<unsigned long long>(flags));
    return 0;
}

int APS5_VABI sceAjmInstanceDestroy(uint32_t context, uint32_t instance) {
    std::lock_guard lock(mutex);
    if (contexts.count(context) == 0) return SCE_AJM_ERROR_INVALID_CONTEXT;
    if (const auto found = instanceStates.find(instance); found != instanceStates.end()) {
        if (found->second.decoder != nullptr) Atrac9ReleaseHandle(found->second.decoder);
        instanceStates.erase(found);
    }
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
int APS5_VABI sceAjmBatchJobSetGaplessDecode(AjmBatchInfo* info, uint32_t instance, const void* gapless_decode, int reset, void* result) { (void)reset; return append(info, Job{Job::Gapless, instance, 0, gapless_decode, gapless_decode != nullptr ? 8u : 0u, nullptr, 0, result, result != nullptr ? 8u : 0u, {}, {}}); }
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
    unsigned char data[ATRAC9_CONFIG_DATA_SIZE];
    std::memcpy(data, config_data, sizeof(data));
    void* decoder = Atrac9GetHandle();
    Atrac9CodecInfo codec{};
    const bool valid = decoder != nullptr && Atrac9InitDecoder(decoder, data) == 0 && Atrac9GetCodecInfo(decoder, &codec) == 0;
    if (decoder != nullptr) Atrac9ReleaseHandle(decoder);
    if (!valid) return SCE_AJM_ERROR_INVALID_PARAMETER;
    config_info->channels = static_cast<std::uint32_t>(codec.channels);
    config_info->sample_rate = static_cast<std::uint32_t>(codec.samplingRate);
    config_info->frame_samples_per_channel = static_cast<std::uint32_t>(codec.frameSamples);
    config_info->superframe_samples_per_channel = static_cast<std::uint32_t>(codec.frameSamples * codec.framesInSuperframe);
    config_info->superframe_size = static_cast<std::uint32_t>(codec.superframeSize);
    return 0;
}

}

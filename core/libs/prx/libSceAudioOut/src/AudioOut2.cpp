#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
#include <prx/libc/include/General.hpp>
#include "SDL.h"
#include "SceTypes.hpp"

// The PS5 audio output API (AudioOut2). A context owns ports; each grain (256 frames) the title
// sets every port's data attribute and pushes the context. All ports are mixed to one stereo
// float stream on the host's default SDL audio device; pushes pace themselves against the
// device queue so the title's mixer runs in real time.
namespace {

constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_PARAM = static_cast<int>(0x80260001);
constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE = static_cast<int>(0x80260003);
constexpr int SCE_AUDIO_OUT2_ERROR_OUT_OF_MEMORY = static_cast<int>(0x80260006);
constexpr int SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED = static_cast<int>(0x80260007);
constexpr int SCE_AUDIO_OUT2_ERROR_PORT_FULL = static_cast<int>(0x8026000a);
constexpr std::uint32_t GrainFrames = 256;
constexpr std::uint32_t OutputRate = 48000;
constexpr std::uint32_t OutputChannels = 2;
constexpr std::uint32_t TargetLatencyGrains = 8;
constexpr std::uint32_t PORT_TYPE_VIBRATION = 10;
constexpr std::uint32_t PORT_TYPE_PADSPK = 4;
constexpr std::uint32_t PORT_TYPE_HAPTICS = 6;  // DualSense haptics as two-channel audio: not for speakers

// Port attribute ids of the console's API: the data attribute's value is the address of a
// pointer variable holding the grain buffer.
constexpr std::uint32_t ATTRIBUTE_DATA = 0;
constexpr std::uint32_t ATTRIBUTE_VOLUME = 1;

struct Port {
    AudioOut2ContextHandle context;
    std::uint16_t type;
    std::uint32_t format;
    std::uint32_t rate;
    std::uint32_t channels;
    bool isFloat;
    const void* data = nullptr;
    std::size_t dataSize = 0;
    float volume = 1.0f;
    bool typeSettled = false;
};

struct Context {
    AudioOut2ContextParam param;
    std::set<AudioOut2PortHandle> ports;
    std::vector<float> mix;
};

std::mutex mutex;
bool initialized = false;
std::map<AudioOut2ContextHandle, Context> contexts;
std::map<AudioOut2PortHandle, Port> ports;
std::uint64_t nextHandle = 0x10;
SDL_AudioDeviceID device = 0;
std::set<std::uint32_t> reportedAttributes;

bool decodeFormat(std::uint32_t format, std::uint32_t& channels, bool& isFloat) {
    // The PS5 encoding carries the channel count in bits 8-11 (0x800: eight channels); the sample
    // type is settled from the data itself. Older single-byte codes are the PS4 enumeration.
    if ((format >> 8) != 0) {
        channels = (format >> 8) & 0xfu;
        isFloat = true;
        return channels == 1 || channels == 2 || channels == 8;
    }
    switch (format & 0xffu) {
    case 0: channels = 1; isFloat = false; return true;
    case 1: channels = 2; isFloat = false; return true;
    case 2: channels = 8; isFloat = false; return true;
    case 3: channels = 1; isFloat = true; return true;
    case 4: channels = 2; isFloat = true; return true;
    case 5: channels = 8; isFloat = true; return true;
    case 6: channels = 8; isFloat = false; return true;
    case 7: channels = 8; isFloat = true; return true;
    default: return false;
    }
}

bool openDevice() {
    if (device != 0) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) { APS5_LOG_OUT("SDL audio unavailable: %s", SDL_GetError()); return false; }
    SDL_AudioSpec desired{};
    desired.freq = static_cast<int>(OutputRate);
    desired.format = AUDIO_F32SYS;
    desired.channels = static_cast<Uint8>(OutputChannels);
    desired.samples = static_cast<Uint16>(GrainFrames * 2);
    SDL_AudioSpec obtained{};
    device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (device == 0) { APS5_LOG_OUT("cannot open the audio device: %s", SDL_GetError()); return false; }
    SDL_PauseAudioDevice(device, 0);
    return true;
}

// Reads frame `frame`, channel `channel` of a port's grain as a float sample.
float sample(const Port& port, std::uint32_t frame, std::uint32_t channel) {
    const auto index = frame * port.channels + channel;
    if (port.isFloat) {
        if ((index + 1) * sizeof(float) > port.dataSize) return 0.0f;
        float value; std::memcpy(&value, static_cast<const std::uint8_t*>(port.data) + index * sizeof(float), sizeof(value)); return value;
    }
    if ((index + 1) * sizeof(std::int16_t) > port.dataSize) return 0.0f;
    std::int16_t value; std::memcpy(&value, static_cast<const std::uint8_t*>(port.data) + index * sizeof(std::int16_t), sizeof(value));
    return static_cast<float>(value) / 32768.0f;
}

// Mixes a port's grain into a stereo buffer: 7.1 layouts fold their surround channels down.
void mixPort(Port& port, std::vector<float>& mix) {
    if (port.data == nullptr || port.type == PORT_TYPE_VIBRATION || port.type == PORT_TYPE_HAPTICS) return;
    if (!port.typeSettled) {
        // Float samples of a mix stay within a few units; the same bytes read as floats from 16-bit
        // data are mostly denormals, huge values or NaNs. A fade-in has some tiny floats too: count.
        const auto count = GrainFrames * port.channels;
        std::uint32_t plausible = 0, garbage = 0;
        for (std::uint32_t index = 0; index < count; ++index) {
            float value; std::memcpy(&value, static_cast<const std::uint8_t*>(port.data) + index * sizeof(float), sizeof(value));
            if (value == 0.0f) continue;
            if (!(value == value) || value > 16.0f || value < -16.0f || (value < 1e-20f && value > -1e-20f)) ++garbage;
            else ++plausible;
        }
        if (plausible + garbage >= 64) {
            port.isFloat = plausible > garbage;
            port.typeSettled = true;
            if (port.format >= 0x100) APS5_LOG_OUT("port format 0x%x: %s samples (%u plausible, %u not)", port.format, port.isFloat ? "float" : "int16", plausible, garbage);
        }
    }
    for (std::uint32_t frame = 0; frame < GrainFrames; ++frame) {
        float left = 0.0f, right = 0.0f;
        if (port.channels == 1) { left = right = sample(port, frame, 0); }
        else if (port.channels == 2) { left = sample(port, frame, 0); right = sample(port, frame, 1); }
        else {
            // L R C LFE Ls Rs Lb Rb (standard order; the console's own order swaps the pairs, both fold alike)
            const float center = sample(port, frame, 2) * 0.7071f, lfe = sample(port, frame, 3) * 0.5f;
            left = sample(port, frame, 0) + center + lfe + (sample(port, frame, 4) + sample(port, frame, 6)) * 0.7071f;
            right = sample(port, frame, 1) + center + lfe + (sample(port, frame, 5) + sample(port, frame, 7)) * 0.7071f;
        }
        mix[frame * 2] += left * port.volume;
        mix[frame * 2 + 1] += right * port.volume;
    }
}

}

extern "C" {

int APS5_VABI sceAudioOut2Initialize(void) {
    std::lock_guard lock(mutex);
    initialized = true;
    return 0;
}

int APS5_VABI sceAudioOut2GetSystemState(AudioOut2SystemState* state) {
    if (state == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    *state = AudioOut2SystemState{};
    state->loudness = -24.0f; // LKFS of the system mix, the console's default target
    return 0;
}

int APS5_VABI sceAudioOut2SetSystemDebugState(const AudioOut2SystemDebugStateParam* param) {
    return param != nullptr ? 0 : SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
}

int APS5_VABI sceAudioOut2UserCreate(uint32_t user_id, AudioOut2UserHandle* handle) {
    if (handle == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    std::lock_guard lock(mutex);
    if (!initialized) return SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED;
    *handle = static_cast<AudioOut2UserHandle>(user_id);
    return 0;
}

int APS5_VABI sceAudioOut2UserDestroy(AudioOut2UserHandle handle) { (void)handle; return 0; }

int APS5_VABI sceAudioOut2ContextResetParam(AudioOut2ContextParam* params) {
    if (params == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    *params = AudioOut2ContextParam{};
    params->max_ports = 8;
    params->max_object_ports = 0;
    params->guarantee_object_ports = 0;
    params->queue_depth = 2;
    params->num_grains = 1;
    return 0;
}

int APS5_VABI sceAudioOut2ContextQueryMemory(const AudioOut2ContextParam* params, size_t* memory_size) {
    if (params == nullptr || memory_size == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    *memory_size = 65536 + static_cast<std::size_t>(params->max_ports + params->max_object_ports) * 16384;
    return 0;
}

int APS5_VABI sceAudioOut2ContextCreate(const AudioOut2ContextParam* params, void* buffer, size_t buffer_size, AudioOut2ContextHandle* ctx) {
    if (params == nullptr || buffer == nullptr || ctx == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    std::size_t required = 0;
    sceAudioOut2ContextQueryMemory(params, &required);
    if (buffer_size < required) return SCE_AUDIO_OUT2_ERROR_OUT_OF_MEMORY;
    std::lock_guard lock(mutex);
    if (!initialized) return SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED;
    APS5_LOG_OUT("context: ports=%u object_ports=%u queue_depth=%u grains=%u flags=0x%x", params->max_ports, params->max_object_ports, params->queue_depth, params->num_grains, params->flags);
    *ctx = nextHandle++;
    auto& context = contexts[*ctx];
    context.param = *params;
    context.mix.assign(GrainFrames * OutputChannels, 0.0f);
    openDevice();
    return 0;
}

int APS5_VABI sceAudioOut2ContextDestroy(AudioOut2ContextHandle ctx) {
    std::lock_guard lock(mutex);
    const auto found = contexts.find(ctx);
    if (found == contexts.end()) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    for (const auto port : found->second.ports) ports.erase(port);
    contexts.erase(found);
    return 0;
}

int APS5_VABI sceAudioOut2ContextSetAttributes(AudioOut2ContextHandle ctx, const AudioOut2Attribute* attributes, uint32_t num) {
    if (attributes == nullptr && num != 0) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    std::lock_guard lock(mutex);
    if (contexts.count(ctx) == 0) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    for (std::uint32_t index = 0; index < num; ++index)
        if (reportedAttributes.insert(0x1000u | attributes[index].attribute_id).second) APS5_LOG_OUT("context attribute id=%u size=%zu", attributes[index].attribute_id, attributes[index].value_size);
    return 0;
}

int APS5_VABI sceAudioOut2ContextGetQueueLevel(AudioOut2ContextHandle ctx, uint32_t* queue_level, uint32_t* available_queues) {
    std::lock_guard lock(mutex);
    const auto found = contexts.find(ctx);
    if (found == contexts.end()) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    const auto depth = std::max(1u, found->second.param.queue_depth);
    std::uint32_t queued = 0;
    if (device != 0) queued = static_cast<std::uint32_t>(SDL_GetQueuedAudioSize(device) / (GrainFrames * OutputChannels * sizeof(float)));
    const auto level = std::min(depth, queued);
    if (queue_level != nullptr) *queue_level = level;
    if (available_queues != nullptr) *available_queues = depth - level;
    return 0;
}

int APS5_VABI sceAudioOut2ContextPush(AudioOut2ContextHandle ctx, uint32_t blocking) {
    std::vector<float>* mix = nullptr;
    {
        std::lock_guard lock(mutex);
        const auto found = contexts.find(ctx);
        if (found == contexts.end()) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
        auto& context = found->second;
        std::fill(context.mix.begin(), context.mix.end(), 0.0f);
        for (const auto handle : context.ports) {
            auto port = ports.find(handle);
            if (port != ports.end()) mixPort(port->second, context.mix);
        }
        for (auto& value : context.mix) value = std::clamp(value, -1.0f, 1.0f);
        mix = &context.mix;
        if (device != 0) SDL_QueueAudio(device, mix->data(), static_cast<Uint32>(mix->size() * sizeof(float)));
    }
    // Pace the caller: keep about TargetLatencyGrains grains queued on the device.
    const auto grainBytes = GrainFrames * OutputChannels * sizeof(float);
    if (device != 0) {
        while (SDL_GetQueuedAudioSize(device) > TargetLatencyGrains * grainBytes) {
            if (blocking == 0) break;
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    } else if (blocking != 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(1000000ull * GrainFrames / OutputRate));
    }
    return 0;
}

int APS5_VABI sceAudioOut2ContextAdvance(AudioOut2ContextHandle ctx) {
    std::lock_guard lock(mutex);
    return contexts.count(ctx) != 0 ? 0 : SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceAudioOut2PortCreate(AudioOut2ContextHandle ctx, const AudioOut2PortParam* params, AudioOut2PortHandle* port) {
    if (params == nullptr || port == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    std::uint32_t channels = 0; bool isFloat = false;
    if (!decodeFormat(params->data_format, channels, isFloat)) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    std::lock_guard lock(mutex);
    const auto found = contexts.find(ctx);
    if (found == contexts.end()) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    if (found->second.ports.size() >= found->second.param.max_ports + found->second.param.max_object_ports) return SCE_AUDIO_OUT2_ERROR_PORT_FULL;
    APS5_LOG_OUT("port: type=%u format=0x%x rate=%u flags=0x%x", params->port_type, params->data_format, params->sampling_freq, params->flags);
    *port = nextHandle++;
    ports[*port] = Port{ctx, params->port_type, params->data_format, params->sampling_freq != 0 ? params->sampling_freq : OutputRate, channels, isFloat};
    ports[*port].typeSettled = params->data_format < 0x100;
    found->second.ports.insert(*port);
    return 0;
}

int APS5_VABI sceAudioOut2PortDestroy(AudioOut2PortHandle port) {
    std::lock_guard lock(mutex);
    const auto found = ports.find(port);
    if (found == ports.end()) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    if (const auto context = contexts.find(found->second.context); context != contexts.end()) context->second.ports.erase(port);
    ports.erase(found);
    return 0;
}

int APS5_VABI sceAudioOut2PortGetState(AudioOut2PortHandle port, AudioOut2PortState* state) {
    if (state == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    std::lock_guard lock(mutex);
    const auto found = ports.find(port);
    if (found == ports.end()) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    *state = AudioOut2PortState{};
    state->output = found->second.type == PORT_TYPE_PADSPK ? 0 : 1; // 1: the primary output (TV / headphones)
    state->num_channels = static_cast<std::uint8_t>(found->second.channels);
    state->volume = 32767;
    return 0;
}

int APS5_VABI sceAudioOut2PortSetAttributes(AudioOut2PortHandle port, const AudioOut2Attribute* attributes, uint32_t num) {
    if (attributes == nullptr && num != 0) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    std::lock_guard lock(mutex);
    const auto found = ports.find(port);
    if (found == ports.end()) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    auto& state = found->second;
    const auto grainBytes = GrainFrames * state.channels * (state.isFloat ? sizeof(float) : sizeof(std::int16_t));
    for (std::uint32_t index = 0; index < num; ++index) {
        const auto& attribute = attributes[index];
        if (reportedAttributes.insert(attribute.attribute_id).second) APS5_LOG_OUT("port attribute id=%u size=%zu (grain %zu bytes)", attribute.attribute_id, attribute.value_size, grainBytes);
        if (attribute.attribute_id == ATTRIBUTE_DATA && attribute.value_size == sizeof(void*)) {
            const void* buffer = nullptr;
            if (attribute.value != nullptr) std::memcpy(&buffer, attribute.value, sizeof(buffer));
            state.data = buffer;
            state.dataSize = grainBytes;
        } else if (attribute.attribute_id == ATTRIBUTE_DATA || attribute.value_size == grainBytes) {
            state.data = attribute.value;
            state.dataSize = attribute.value_size;
        } else if (attribute.attribute_id == ATTRIBUTE_VOLUME && attribute.value != nullptr && attribute.value_size >= sizeof(float)) {
            float volume; std::memcpy(&volume, attribute.value, sizeof(volume));
            state.volume = std::clamp(volume, 0.0f, 1.0f);
        }
    }
    return 0;
}

size_t APS5_VABI sceAudioOut2GetSpeakerArrayMemorySize(uint32_t num_speakers, uint8_t is_3d, uint8_t is_ambisonics) {
    (void)is_3d; (void)is_ambisonics;
    return 4096 + static_cast<std::size_t>(num_speakers) * 256;
}

int APS5_VABI sceAudioOut2SpeakerArrayCreate(AudioOut2SpeakerArrayHandle* handle, const void* vbap_params, const void* ambi_params) {
    (void)ambi_params;
    if (handle == nullptr || vbap_params == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    *handle = const_cast<void*>(vbap_params);
    return 0;
}

int APS5_VABI sceAudioOut2SpeakerArrayDestroy(AudioOut2SpeakerArrayHandle handle) { return handle != nullptr ? 0 : SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE; }

int APS5_VABI sceAudioOut2GetSpeakerArrayCoefficients(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, uint32_t num_coefficients, uint8_t height_aware, float downmix_spread_radius) {
    (void)spread; (void)height_aware; (void)downmix_spread_radius;
    if (handle == nullptr || coefficients == nullptr || num_coefficients == 0) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    // Constant-power pan across the first two coefficients (left, right); the rest are silent.
    const auto length = std::sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
    const float x = length > 0.0001f ? pos.x / length : 0.0f;
    const float angle = (x + 1.0f) * 0.25f * 3.14159265f;
    std::fill(coefficients, coefficients + num_coefficients, 0.0f);
    coefficients[0] = std::cos(angle);
    if (num_coefficients > 1) coefficients[1] = std::sin(angle);
    return 0;
}

int APS5_VABI sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(AudioOut2SpeakerArrayHandle handle, uint32_t ambisonics_channel, float* coefficients, uint32_t num_coefficients) {
    (void)ambisonics_channel;
    if (handle == nullptr || coefficients == nullptr || num_coefficients == 0) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    std::fill(coefficients, coefficients + num_coefficients, 1.0f / static_cast<float>(num_coefficients));
    return 0;
}

int APS5_VABI sceAudioOut2GetSpeakerInfo(AudioOut2SpeakerInfo* info, uint32_t flags) {
    (void)flags;
    if (info == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    *info = AudioOut2SpeakerInfo{};
    info->type = 1; // stereo
    info->available_bits = 0x3; // left and right
    info->speaker_angle[0] = AudioOut2SpeakerAngle{-30, 0};
    info->speaker_angle[1] = AudioOut2SpeakerAngle{30, 0};
    return 0;
}

int APS5_VABI sceAudioOut2MasteringInit(uint32_t flags) { (void)flags; return 0; }
int APS5_VABI sceAudioOut2MasteringTerm(void) { return 0; }
int APS5_VABI sceAudioOut2MasteringGetState(AudioOut2MasteringStatesHeader* state, uint32_t output, AudioOut2UserHandle user) { (void)output; (void)user; return state != nullptr ? 0 : SCE_AUDIO_OUT2_ERROR_INVALID_PARAM; }
int APS5_VABI sceAudioOut2MasteringSetParam(const AudioOut2MasteringParamsHeader* param, uint32_t output, uint32_t flags) { (void)output; (void)flags; return param != nullptr ? 0 : SCE_AUDIO_OUT2_ERROR_INVALID_PARAM; }

}

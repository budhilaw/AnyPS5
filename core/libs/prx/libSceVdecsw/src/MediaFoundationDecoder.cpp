#include "prx/libSceVdecsw/include/AvcParser.hpp"
#include "prx/libSceVdecsw/include/PictureDecoder.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseSleep.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <codecapi.h>
#include <mfapi.h>
#include <mferror.h>
#include <mftransform.h>
#include <wmcodecdsp.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Vdecsw {

namespace {

using Microsoft::WRL::ComPtr;

constexpr std::uint8_t StartCode[] = {0, 0, 0, 1};
constexpr unsigned LoggedFailures = 8;
constexpr unsigned OutputAttempts = 8;
constexpr auto OutputWait = std::chrono::milliseconds(50);
constexpr std::uint64_t OutputPollNanos = 200000;
constexpr std::size_t PooledSamples = 4;
constexpr LONGLONG NoPicture = -1;

struct MediaFoundation {
    decltype(&MFCreateMediaType) createMediaType;
    decltype(&MFCreateSample) createSample;
    decltype(&MFCreateMemoryBuffer) createMemoryBuffer;
};

template<typename TFunction>
TFunction Resolve(HMODULE module, const char* name) {
    return reinterpret_cast<TFunction>(reinterpret_cast<void*>(GetProcAddress(module, name)));
}

const MediaFoundation* LoadMediaFoundation() {
    static const auto api = []() -> std::optional<MediaFoundation> {
        const HMODULE module = LoadLibraryExW(L"mfplat.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (module == nullptr) return std::nullopt;
        const auto startup = Resolve<decltype(&MFStartup)>(module, "MFStartup");
        const MediaFoundation result{Resolve<decltype(&MFCreateMediaType)>(module, "MFCreateMediaType"), Resolve<decltype(&MFCreateSample)>(module, "MFCreateSample"), Resolve<decltype(&MFCreateMemoryBuffer)>(module, "MFCreateMemoryBuffer")};
        if (startup == nullptr || result.createMediaType == nullptr || result.createSample == nullptr || result.createMemoryBuffer == nullptr) return std::nullopt;
        if (FAILED(startup(MF_VERSION, MFSTARTUP_NOSOCKET))) return std::nullopt;
        return result;
    }();
    return api ? &*api : nullptr;
}

bool EnterApartment() {
    thread_local const HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    return SUCCEEDED(status) || status == RPC_E_CHANGED_MODE;
}

struct FrameFormat {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    DWORD sampleBytes = 0;
    bool providedSamples = false;

    std::size_t Bytes() const { return static_cast<std::size_t>(stride) * (height + (height + 1) / 2); }
};

class SamplePool {
public:
    SamplePool() { samples.reserve(PooledSamples); }

    ComPtr<IMFSample> Take(const MediaFoundation& api, DWORD bytes) {
        {
            std::lock_guard lock(mutex);
            while (!samples.empty()) {
                auto pooled = std::move(samples.back());
                samples.pop_back();
                if (pooled.bytes >= bytes) return std::move(pooled.sample);
            }
        }
        ComPtr<IMFMediaBuffer> buffer;
        ComPtr<IMFSample> sample;
        if (FAILED(api.createMemoryBuffer(bytes, &buffer)) || FAILED(api.createSample(&sample)) || FAILED(sample->AddBuffer(buffer.Get()))) return nullptr;
        return sample;
    }

    void Give(ComPtr<IMFSample> sample) {
        ComPtr<IMFMediaBuffer> buffer;
        DWORD bytes = 0;
        if (sample == nullptr || FAILED(sample->GetBufferByIndex(0, &buffer)) || FAILED(buffer->GetMaxLength(&bytes)) || FAILED(buffer->SetCurrentLength(0))) return;
        std::lock_guard lock(mutex);
        if (samples.size() < PooledSamples) samples.push_back({std::move(sample), bytes});
    }

private:
    struct Pooled {
        ComPtr<IMFSample> sample;
        DWORD bytes;
    };

    std::mutex mutex;
    std::vector<Pooled> samples;
};

class SampleImage final : public IDecodedImage {
public:
    SampleImage(ComPtr<IMFSample> sample, const FrameFormat& format, std::shared_ptr<SamplePool> pool) : sample(std::move(sample)), format(format), pool(std::move(pool)) {}

    ~SampleImage() override {
        if (pool != nullptr) pool->Give(std::move(sample));
    }

    SampleImage(const SampleImage&) = delete;
    SampleImage& operator=(const SampleImage&) = delete;

    void CopyTo(const Nv12Layout& layout) const override {
        ComPtr<IMFMediaBuffer> buffer;
        BYTE* data = nullptr;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer)) || FAILED(buffer->Lock(&data, nullptr, nullptr))) throw std::runtime_error("Media Foundation: cannot lock a decoded picture");
        const auto bytes = std::min(layout.width, format.width);
        const auto copyPlane = [&](const std::uint8_t* source, std::uint8_t* destination, std::uint32_t rows) {
            for (std::uint32_t row = 0; row < rows; ++row) std::memcpy(destination + static_cast<std::size_t>(row) * layout.pitch, source + static_cast<std::size_t>(row) * format.stride, bytes);
        };
        copyPlane(data, layout.luma, std::min(layout.height, format.height));
        copyPlane(data + static_cast<std::size_t>(format.stride) * format.height, layout.chroma, std::min((layout.height + 1) / 2, (format.height + 1) / 2));
        buffer->Unlock();
    }

private:
    ComPtr<IMFSample> sample;
    FrameFormat format;
    std::shared_ptr<SamplePool> pool;
};

class MediaFoundationDecoder final : public IPictureDecoder {
public:
    MediaFoundationDecoder() : pool(std::make_shared<SamplePool>()) {}
    ~MediaFoundationDecoder() override = default;
    MediaFoundationDecoder(const MediaFoundationDecoder&) = delete;
    MediaFoundationDecoder& operator=(const MediaFoundationDecoder&) = delete;

    std::unique_ptr<IDecodedImage> Decode(std::span<const std::uint8_t> sps, std::span<const std::uint8_t> pps, std::span<const std::span<const std::uint8_t>> units, bool) override {
        if (!std::ranges::equal(sps, currentSps)) open(sps);
        if (transform == nullptr || !EnterApartment()) return nullptr;
        const bool feedSequence = !sequenceFed;
        const bool feedPicture = feedSequence || !std::ranges::equal(pps, currentPps);
        std::size_t bytes = 0;
        if (feedSequence) bytes += sizeof(StartCode) + streamSps.size();
        if (feedPicture) bytes += sizeof(StartCode) + pps.size();
        for (const auto unit : units) bytes += sizeof(StartCode) + unit.size();
        ComPtr<IMFMediaBuffer> buffer;
        BYTE* data = nullptr;
        auto status = api->createMemoryBuffer(static_cast<DWORD>(bytes), &buffer);
        if (SUCCEEDED(status)) status = buffer->Lock(&data, nullptr, nullptr);
        if (FAILED(status)) return fail("cannot allocate an input sample", status);
        const auto append = [&](std::span<const std::uint8_t> nal) {
            std::memcpy(data, StartCode, sizeof(StartCode));
            std::memcpy(data + sizeof(StartCode), nal.data(), nal.size());
            data += sizeof(StartCode) + nal.size();
        };
        if (feedSequence) append(streamSps);
        if (feedPicture) append(pps);
        for (const auto unit : units) append(unit);
        buffer->Unlock();
        const auto picture = pictures++;
        ComPtr<IMFSample> sample;
        status = buffer->SetCurrentLength(static_cast<DWORD>(bytes));
        if (SUCCEEDED(status)) status = api->createSample(&sample);
        if (SUCCEEDED(status)) status = sample->AddBuffer(buffer.Get());
        if (SUCCEEDED(status)) status = sample->SetSampleTime(picture);
        if (SUCCEEDED(status)) status = transform->ProcessInput(0, sample.Get(), 0);
        if (status == MF_E_NOTACCEPTING) {
            receive(NoPicture);
            status = transform->ProcessInput(0, sample.Get(), 0);
        }
        if (FAILED(status)) return fail("rejected a picture", status);
        sequenceFed = true;
        if (feedPicture) currentPps.assign(pps.begin(), pps.end());
        return receive(picture);
    }

    void Reset() override { release(); }

private:
    void open(std::span<const std::uint8_t> sps) {
        release();
        currentSps.assign(sps.begin(), sps.end());
        const auto parsed = Avc::ParseSps(sps);
        streamSps = Avc::WithoutReordering(sps);
        api = LoadMediaFoundation();
        if (api == nullptr || !EnterApartment() || FAILED(CoCreateInstance(CLSID_CMSH264DecoderMFT, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&transform)))) {
            transform.Reset();
            static std::once_flag once;
            std::call_once(once, [] { APS5_LOG_CHARS_OUT("Media Foundation H.264 decoding is not available: movies play as black frames"); });
            return;
        }
        ComPtr<IMFAttributes> attributes;
        ComPtr<IMFMediaType> input;
        auto status = transform->GetAttributes(&attributes);
        if (SUCCEEDED(status)) status = attributes->SetUINT32(CODECAPI_AVLowLatencyMode, TRUE);
        if (SUCCEEDED(status)) status = api->createMediaType(&input);
        if (SUCCEEDED(status)) status = input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (SUCCEEDED(status)) status = input->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        if (SUCCEEDED(status)) status = MFSetAttributeSize(input.Get(), MF_MT_FRAME_SIZE, parsed.CodedWidth(), parsed.CodedHeight());
        if (SUCCEEDED(status)) status = transform->SetInputType(0, input.Get(), 0);
        if (SUCCEEDED(status)) status = negotiate();
        if (SUCCEEDED(status)) status = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        if (SUCCEEDED(status)) status = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        if (FAILED(status)) {
            fail("cannot be configured", status);
            transform.Reset();
        }
    }

    HRESULT negotiate() {
        for (DWORD index = 0;; ++index) {
            ComPtr<IMFMediaType> type;
            auto status = transform->GetOutputAvailableType(0, index, &type);
            if (FAILED(status)) return status;
            GUID subtype{};
            if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &subtype)) || subtype != MFVideoFormat_NV12) continue;
            UINT32 width = 0;
            UINT32 height = 0;
            MFT_OUTPUT_STREAM_INFO info{};
            status = transform->SetOutputType(0, type.Get(), 0);
            if (SUCCEEDED(status)) status = MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height);
            if (SUCCEEDED(status)) status = transform->GetOutputStreamInfo(0, &info);
            if (FAILED(status)) return status;
            if (width == 0 || height == 0) return MF_E_INVALIDMEDIATYPE;
            const auto stride = static_cast<std::int32_t>(MFGetAttributeUINT32(type.Get(), MF_MT_DEFAULT_STRIDE, width));
            format = {width, height, stride >= static_cast<std::int32_t>(width) ? static_cast<std::uint32_t>(stride) : width, info.cbSize, (info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) != 0};
            return S_OK;
        }
    }

    HRESULT processOutput(MFT_OUTPUT_DATA_BUFFER& output, bool waiting) {
        const auto deadline = std::chrono::steady_clock::now() + OutputWait;
        for (;;) {
            DWORD flags = 0;
            const auto status = transform->ProcessOutput(0, 1, &output, &flags);
            if (output.pEvents != nullptr) {
                output.pEvents->Release();
                output.pEvents = nullptr;
            }
            if (status != MF_E_TRANSFORM_NEED_MORE_INPUT || !waiting || std::chrono::steady_clock::now() >= deadline) return status;
            PreciseSleepNanos_nid_no_patch(OutputPollNanos);
        }
    }

    std::unique_ptr<IDecodedImage> receive(LONGLONG picture) {
        for (unsigned attempt = 0; attempt < OutputAttempts; ++attempt) {
            const bool pooled = !format.providedSamples;
            ComPtr<IMFSample> sample;
            if (pooled) {
                sample = pool->Take(*api, static_cast<DWORD>(std::max<std::size_t>(format.sampleBytes, format.Bytes())));
                if (sample == nullptr) return fail("cannot allocate an output sample", E_OUTOFMEMORY);
            }
            MFT_OUTPUT_DATA_BUFFER output{0, sample.Get(), 0, nullptr};
            const auto status = processOutput(output, picture != NoPicture);
            if (!pooled) sample.Attach(output.pSample);
            LONGLONG produced = NoPicture;
            DWORD length = 0;
            if (SUCCEEDED(status) && sample != nullptr && SUCCEEDED(sample->GetSampleTime(&produced)) && produced == picture && SUCCEEDED(sample->GetTotalLength(&length)) && length >= format.Bytes()) {
                return std::make_unique<SampleImage>(std::move(sample), format, pooled ? pool : nullptr);
            }
            if (pooled) pool->Give(std::move(sample));
            if (status == MF_E_TRANSFORM_STREAM_CHANGE) {
                if (const auto changed = negotiate(); FAILED(changed)) return fail("cannot follow a format change", changed);
                continue;
            }
            if (status == MF_E_TRANSFORM_NEED_MORE_INPUT) return picture == NoPicture ? nullptr : fail("produced no picture", status);
            if (FAILED(status)) return fail("could not decode a picture", status);
            if (picture != NoPicture) fail("returned another picture", MF_E_UNEXPECTED);
        }
        return picture == NoPicture ? nullptr : fail("did not finish a picture", MF_E_UNEXPECTED);
    }

    std::unique_ptr<IDecodedImage> fail(const char* what, HRESULT status) {
        if (++failures <= LoggedFailures) APS5_LOG_OUT("Media Foundation: the H.264 decoder %s (0x%08lx)", what, static_cast<unsigned long>(status));
        return nullptr;
    }

    void release() {
        transform.Reset();
        currentSps.clear();
        currentPps.clear();
        streamSps.clear();
        sequenceFed = false;
    }

    const MediaFoundation* api = nullptr;
    ComPtr<IMFTransform> transform;
    std::shared_ptr<SamplePool> pool;
    FrameFormat format;
    std::vector<std::uint8_t> currentSps;
    std::vector<std::uint8_t> currentPps;
    std::vector<std::uint8_t> streamSps;
    bool sequenceFed = false;
    LONGLONG pictures = 0;
    unsigned failures = 0;
};

}

std::unique_ptr<IPictureDecoder> CreatePlatformDecoder() {
    return std::make_unique<MediaFoundationDecoder>();
}

}

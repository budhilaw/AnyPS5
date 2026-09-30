#include "prx/libSceVdecsw/include/AvcParser.hpp"
#include "prx/libSceVdecsw/include/PictureDecoder.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int ErrorFrameBufferSize = static_cast<int>(0x81510106);
constexpr int ErrorCodecType = static_cast<int>(0x81510204);
constexpr int ErrorOutputPending = static_cast<int>(0x81510115);
constexpr int ErrorInputQueueEmpty = static_cast<int>(0x81510116);
constexpr int ErrorDecodePending = static_cast<int>(0x81510117);
constexpr std::uint32_t CodecAvc = 1;
constexpr std::uint32_t PitchAlignment = 256;
constexpr std::uint64_t ComputeMemoryBytes = 0x4000;
constexpr std::uint64_t DecoderCpuMemoryBytes = 0x10000;
constexpr std::uint64_t DecoderGpuMemoryBytes = 0x200000;
constexpr std::uint32_t FrameBufferAlignment = 0x10000;

struct ComputeMemoryInfo {
    std::uint64_t thisSize;
    std::uint64_t memorySize;
    void* memory;
};

struct ComputeConfigInfo {
    std::uint64_t thisSize;
    std::uint16_t computePipeId;
    std::uint16_t computeQueueId;
    std::uint8_t checkMemoryType;
    std::uint8_t reserved0;
    std::uint16_t reserved1;
};

struct DecoderConfigInfo {
    std::uint64_t thisSize;
    std::uint32_t resourceType;
    std::uint32_t codecType;
    std::uint32_t profile;
    std::uint32_t maxLevel;
    std::int32_t maxFrameWidth;
    std::int32_t maxFrameHeight;
    std::int32_t maxDpbFrameCount;
    std::uint32_t decodePipelineDepth;
    std::uint64_t computeQueue;
    std::uint64_t cpuAffinityMask;
    std::int32_t cpuThreadPriority;
    std::uint8_t optimizeProgressiveVideo;
    std::uint8_t checkMemoryType;
    std::uint8_t reserved0;
    std::uint8_t reserved1;
    void* extraConfigInfo;
    std::uint8_t reserved2;
    std::uint32_t reserved3;
};

struct DecoderMemoryInfo {
    std::uint64_t thisSize;
    std::uint64_t cpuMemorySize;
    void* cpuMemory;
    std::uint64_t gpuMemorySize;
    void* gpuMemory;
    std::uint64_t cpuGpuMemorySize;
    void* cpuGpuMemory;
    std::uint64_t maxFrameBufferSize;
    std::uint32_t frameBufferAlignment;
    std::uint32_t reserved0;
};

struct InputData {
    std::uint64_t thisSize;
    const std::uint8_t* auData;
    std::uint64_t auSize;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
};

struct InputSyncInfo {
    std::uint64_t thisSize;
    std::uint64_t attachedData;
    std::uint32_t reserved;
};

struct FrameBuffer {
    std::uint64_t thisSize;
    std::uint8_t* frameBuffer;
    std::uint64_t frameBufferSize;
};

struct OutputInfo {
    std::uint64_t thisSize;
    std::uint8_t isValid;
    std::uint8_t isLastFrame;
    std::uint8_t isDiscardedFrame;
    std::uint8_t pictureCount;
    std::uint32_t codecType;
    std::uint32_t frameWidth;
    std::uint32_t framePitch;
    std::uint32_t frameHeight;
    std::uint8_t* frameBuffer;
    std::uint64_t frameBufferSize;
    std::uint32_t frameFormat;
    std::uint32_t framePitchInBytes;
};

struct AvcPictureInfo {
    std::uint64_t thisSize;
    std::uint8_t isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint8_t idrPictureFlag;
    std::uint8_t profileIdc;
    std::uint8_t levelIdc;
    std::uint32_t picWidthInMbsMinus1;
    std::uint32_t picHeightInMapUnitsMinus1;
    std::uint8_t frameMbsOnlyFlag;
    std::uint8_t frameCroppingFlag;
    std::uint32_t frameCropLeftOffset;
    std::uint32_t frameCropRightOffset;
    std::uint32_t frameCropTopOffset;
    std::uint32_t frameCropBottomOffset;
    std::uint8_t aspectRatioInfoPresentFlag;
    std::uint8_t aspectRatioIdc;
    std::uint16_t sarWidth;
    std::uint16_t sarHeight;
    std::uint8_t videoSignalTypePresentFlag;
    std::uint8_t videoFormat;
    std::uint8_t videoFullRangeFlag;
    std::uint8_t colourDescriptionPresentFlag;
    std::uint8_t colourPrimaries;
    std::uint8_t transferCharacteristics;
    std::uint8_t matrixCoefficients;
    std::uint8_t timingInfoPresentFlag;
    std::uint32_t numUnitsInTick;
    std::uint32_t timeScale;
    std::uint8_t fixedFrameRateFlag;
    std::uint8_t bitstreamRestrictionFlag;
    std::uint8_t maxDecFrameBuffering;
    std::uint8_t picStructPresentFlag;
    std::uint8_t picStruct;
    std::uint8_t fieldPicFlag;
    std::uint8_t bottomFieldFlag;
    std::uint8_t sequenceParameterSetPresentFlag;
    std::uint8_t pictureParameterSetPresentFlag;
    std::uint8_t auDelimiterPresentFlag;
    std::uint8_t endOfSequencePresentFlag;
    std::uint8_t endOfStreamPresentFlag;
    std::uint8_t fillerDataPresentFlag;
    std::uint8_t pictureTimingSeiPresentFlag;
    std::uint8_t bufferingPeriodSeiPresentFlag;
    std::uint8_t constraintSetFlags[6];
};

static_assert(sizeof(ComputeMemoryInfo) == 0x18 && sizeof(ComputeConfigInfo) == 0x10 && sizeof(DecoderConfigInfo) == 0x50 && sizeof(DecoderMemoryInfo) == 0x48);
static_assert(sizeof(InputData) == 0x30 && sizeof(InputSyncInfo) == 0x18 && sizeof(FrameBuffer) == 0x18 && sizeof(OutputInfo) == 0x38 && sizeof(AvcPictureInfo) == 0x78);
static_assert(offsetof(OutputInfo, pictureCount) == 0xb && offsetof(OutputInfo, frameBuffer) == 0x20 && offsetof(AvcPictureInfo, picWidthInMbsMinus1) == 0x2c && offsetof(AvcPictureInfo, numUnitsInTick) == 0x58);

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

template<typename T>
T& Require(T* pointer, const char* function, std::uint64_t minimumSize = sizeof(T)) {
    if (pointer == nullptr) throw std::invalid_argument(std::string(function) + ": null argument");
    if (pointer->thisSize < minimumSize) throw std::invalid_argument(std::string(function) + ": structure size " + std::to_string(pointer->thisSize) + " is too small");
    return *pointer;
}

class DeliveredPictures {
public:
    void Record(const std::uint8_t* buffer, const AvcPictureInfo& info) {
        std::lock_guard lock(mutex);
        pictures[buffer] = info;
    }

    void Forget(const std::uint8_t* buffer) {
        std::lock_guard lock(mutex);
        pictures.erase(buffer);
    }

    AvcPictureInfo Find(const std::uint8_t* buffer) {
        std::lock_guard lock(mutex);
        const auto found = pictures.find(buffer);
        if (found == pictures.end()) throw std::invalid_argument("sceVdecswGetAvcPictureInfo: output holds no decoded picture");
        return found->second;
    }

private:
    std::mutex mutex;
    std::unordered_map<const std::uint8_t*, AvcPictureInfo> pictures;
};

DeliveredPictures& Delivered() {
    static DeliveredPictures delivered;
    return delivered;
}

struct Picture {
    std::int64_t order = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::unique_ptr<Vdecsw::IDecodedImage> image;
    AvcPictureInfo info{};
};

class Decoder {
public:
    Decoder(std::uint32_t maxWidth, std::uint32_t maxHeight) : maxWidth(maxWidth), maxHeight(maxHeight), backend(Vdecsw::CreatePlatformDecoder()) {}

    ~Decoder() {
        for (const auto* buffer : delivered) Delivered().Forget(buffer);
    }

    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    int SetInput(const InputData& input) {
        if (input.auData == nullptr || input.auSize == 0) throw std::invalid_argument("sceVdecswSetDecodeInput: empty access unit");
        std::lock_guard lock(mutex);
        finalized = false;
        const auto units = Vdecsw::Avc::SplitAnnexB({input.auData, static_cast<std::size_t>(input.auSize)});
        std::vector<std::span<const std::uint8_t>> picture;
        const Vdecsw::Avc::NalUnit* firstSlice = nullptr;
        bool sequenceSeen = false;
        bool pictureSeen = false;
        AvcPictureInfo info{};
        for (const auto& unit : units) {
            switch (static_cast<Vdecsw::Avc::NalType>(unit.type)) {
                case Vdecsw::Avc::NalType::Sps: {
                    const auto sps = Vdecsw::Avc::ParseSps(unit.bytes);
                    spsById[sps.id] = {std::vector<std::uint8_t>(unit.bytes.begin(), unit.bytes.end()), sps};
                    sequenceSeen = true;
                    break;
                }
                case Vdecsw::Avc::NalType::Pps: {
                    const auto pps = Vdecsw::Avc::ParsePps(unit.bytes);
                    ppsById[pps.id] = {std::vector<std::uint8_t>(unit.bytes.begin(), unit.bytes.end()), pps.spsId};
                    pictureSeen = true;
                    break;
                }
                case Vdecsw::Avc::NalType::Slice:
                case Vdecsw::Avc::NalType::IdrSlice:
                    if (firstSlice == nullptr) firstSlice = &unit;
                    picture.push_back(unit.bytes);
                    break;
                case Vdecsw::Avc::NalType::Sei:
                    info.pictureTimingSeiPresentFlag = 1;
                    picture.push_back(unit.bytes);
                    break;
                case Vdecsw::Avc::NalType::AccessUnitDelimiter: info.auDelimiterPresentFlag = 1; break;
                case Vdecsw::Avc::NalType::EndOfSequence: info.endOfSequencePresentFlag = 1; break;
                case Vdecsw::Avc::NalType::EndOfStream: info.endOfStreamPresentFlag = 1; break;
                case Vdecsw::Avc::NalType::Filler: info.fillerDataPresentFlag = 1; break;
                default: break;
            }
        }
        consumed.push_back(input.attachedData);
        if (firstSlice == nullptr) return 0;
        const auto pps = ppsById.find(Vdecsw::Avc::SlicePpsId(firstSlice->bytes));
        if (pps == ppsById.end()) throw std::runtime_error("sceVdecswSetDecodeInput: slice refers to an unknown picture parameter set");
        const auto sequence = spsById.find(pps->second.spsId);
        if (sequence == spsById.end()) throw std::runtime_error("sceVdecswSetDecodeInput: picture parameter set refers to an unknown sequence parameter set");
        const auto& sps = sequence->second.parsed;
        if (sps.CodedWidth() > maxWidth || sps.CodedHeight() > maxHeight) throw std::runtime_error("sceVdecswSetDecodeInput: stream exceeds the decoder's maximum frame size");
        const auto slice = Vdecsw::Avc::ParseSliceHeader(firstSlice->bytes, sps);
        const bool idr = firstSlice->type == static_cast<std::uint8_t>(Vdecsw::Avc::NalType::IdrSlice);
        if (idr) drain();
        Picture decoded;
        decoded.order = order.Next(*firstSlice, slice, sps);
        decoded.width = sps.CodedWidth();
        decoded.height = sps.CodedHeight();
        decoded.image = backend->Decode(sequence->second.bytes, pps->second.bytes, picture, sps.videoFullRange);
        if (decoded.image == nullptr) decoded.image = Vdecsw::CreateBlankImage(sps.videoFullRange);
        describe(sps, slice, input, idr, sequenceSeen, pictureSeen, info);
        decoded.info = info;
        const auto position = std::upper_bound(reorder.begin(), reorder.end(), decoded.order, [](std::int64_t key, const Picture& entry) { return key < entry.order; });
        reorder.insert(position, std::move(decoded));
        while (reorder.size() > sps.ReorderDepth()) {
            ready.push_back(std::move(reorder.front()));
            reorder.erase(reorder.begin());
        }
        return 0;
    }

    int SyncInput(InputSyncInfo& info) {
        std::lock_guard lock(mutex);
        if (consumed.empty()) return ErrorInputQueueEmpty;
        info.attachedData = consumed.front();
        info.reserved = 0;
        consumed.pop_front();
        return 0;
    }

    int SetOutput(const FrameBuffer& buffer) {
        if (buffer.frameBuffer == nullptr || buffer.frameBufferSize == 0) throw std::invalid_argument("sceVdecswSetDecodeOutput: empty frame buffer");
        std::lock_guard lock(mutex);
        outputs.push_back(buffer);
        return 0;
    }

    int SyncOutput(OutputInfo& output) {
        std::lock_guard lock(mutex);
        output.isValid = 0;
        output.pictureCount = 0;
        if (ready.empty()) return finalized && reorder.empty() ? ErrorDecodePending : ErrorOutputPending;
        const bool last = reorder.empty() && ready.size() == 1;
        if ((last && !finalized) || outputs.empty()) return ErrorOutputPending;
        const auto buffer = outputs.front();
        auto picture = std::move(ready.front());
        const auto pitch = AlignUp(picture.width, PitchAlignment);
        const auto bytes = static_cast<std::uint64_t>(pitch) * picture.height * 3 / 2;
        if (bytes > buffer.frameBufferSize) return ErrorFrameBufferSize;
        outputs.pop_front();
        ready.pop_front();
        picture.image->CopyTo({buffer.frameBuffer, buffer.frameBuffer + static_cast<std::size_t>(pitch) * picture.height, pitch, picture.width, picture.height});
        output.isValid = 1;
        output.isLastFrame = last ? 1 : 0;
        output.isDiscardedFrame = 0;
        output.pictureCount = 1;
        output.codecType = CodecAvc;
        output.frameWidth = picture.width;
        output.framePitch = pitch;
        output.frameHeight = picture.height;
        output.frameBuffer = buffer.frameBuffer;
        output.frameBufferSize = buffer.frameBufferSize;
        output.frameFormat = 0;
        output.framePitchInBytes = pitch;
        Delivered().Record(buffer.frameBuffer, picture.info);
        delivered.insert(buffer.frameBuffer);
        return 0;
    }

    int Finalize() {
        std::lock_guard lock(mutex);
        drain();
        order.Reset();
        finalized = true;
        return 0;
    }

    int Reset() {
        std::lock_guard lock(mutex);
        reorder.clear();
        ready.clear();
        consumed.clear();
        outputs.clear();
        order.Reset();
        backend->Reset();
        finalized = false;
        return 0;
    }

private:
    struct StoredSps {
        std::vector<std::uint8_t> bytes;
        Vdecsw::Avc::SequenceParameterSet parsed;
    };

    struct StoredPps {
        std::vector<std::uint8_t> bytes;
        std::uint32_t spsId;
    };

    void drain() {
        for (auto& picture : reorder) ready.push_back(std::move(picture));
        reorder.clear();
    }

    static void describe(const Vdecsw::Avc::SequenceParameterSet& sps, const Vdecsw::Avc::SliceHeader& slice, const InputData& input, bool idr, bool sequenceSeen, bool pictureSeen, AvcPictureInfo& info) {
        info.thisSize = sizeof(AvcPictureInfo);
        info.isValid = 1;
        info.ptsData = input.ptsData;
        info.dtsData = input.dtsData;
        info.attachedData = input.attachedData;
        info.idrPictureFlag = idr ? 1 : 0;
        info.profileIdc = sps.profileIdc;
        info.levelIdc = sps.levelIdc;
        info.picWidthInMbsMinus1 = sps.picWidthInMbsMinus1;
        info.picHeightInMapUnitsMinus1 = sps.picHeightInMapUnitsMinus1;
        info.frameMbsOnlyFlag = sps.frameMbsOnly ? 1 : 0;
        info.frameCroppingFlag = sps.frameCropping ? 1 : 0;
        info.frameCropLeftOffset = sps.cropLeft;
        info.frameCropRightOffset = sps.cropRight;
        info.frameCropTopOffset = sps.cropTop;
        info.frameCropBottomOffset = sps.cropBottom;
        info.aspectRatioInfoPresentFlag = sps.aspectRatioInfoPresent ? 1 : 0;
        info.aspectRatioIdc = sps.aspectRatioIdc;
        info.sarWidth = sps.sarWidth;
        info.sarHeight = sps.sarHeight;
        info.videoSignalTypePresentFlag = sps.videoSignalTypePresent ? 1 : 0;
        info.videoFormat = sps.videoFormat;
        info.videoFullRangeFlag = sps.videoFullRange ? 1 : 0;
        info.colourDescriptionPresentFlag = sps.colourDescriptionPresent ? 1 : 0;
        info.colourPrimaries = sps.colourPrimaries;
        info.transferCharacteristics = sps.transferCharacteristics;
        info.matrixCoefficients = sps.matrixCoefficients;
        info.timingInfoPresentFlag = sps.timingInfoPresent ? 1 : 0;
        info.numUnitsInTick = sps.numUnitsInTick;
        info.timeScale = sps.timeScale;
        info.fixedFrameRateFlag = sps.fixedFrameRate ? 1 : 0;
        info.bitstreamRestrictionFlag = sps.bitstreamRestriction ? 1 : 0;
        info.maxDecFrameBuffering = static_cast<std::uint8_t>(sps.maxDecFrameBuffering);
        info.picStructPresentFlag = sps.picStructPresent ? 1 : 0;
        info.fieldPicFlag = slice.fieldPic ? 1 : 0;
        info.bottomFieldFlag = slice.bottomField ? 1 : 0;
        info.sequenceParameterSetPresentFlag = sequenceSeen ? 1 : 0;
        info.pictureParameterSetPresentFlag = pictureSeen ? 1 : 0;
        for (std::uint32_t bit = 0; bit < 6; ++bit) info.constraintSetFlags[bit] = (sps.constraintFlags >> (7 - bit)) & 1u;
    }

    std::mutex mutex;
    std::uint32_t maxWidth;
    std::uint32_t maxHeight;
    std::unique_ptr<Vdecsw::IPictureDecoder> backend;
    std::map<std::uint32_t, StoredSps> spsById;
    std::map<std::uint32_t, StoredPps> ppsById;
    Vdecsw::Avc::PictureOrder order;
    std::vector<Picture> reorder;
    std::deque<Picture> ready;
    std::deque<std::uint64_t> consumed;
    std::deque<FrameBuffer> outputs;
    std::set<const std::uint8_t*> delivered;
    bool finalized = false;
};

struct ComputeQueue {
    std::uint16_t pipe;
    std::uint16_t queue;
};

Decoder& From(std::uint64_t handle, const char* function) {
    if (handle == 0) throw std::invalid_argument(std::string(function) + ": null decoder");
    return *reinterpret_cast<Decoder*>(handle);
}

}

extern "C" {

int APS5_VABI sceVdecswQueryComputeMemoryInfo(ComputeMemoryInfo* info) {
    auto& memory = Require(info, __func__);
    memory.memorySize = ComputeMemoryBytes;
    memory.memory = nullptr;
    return 0;
}

int APS5_VABI sceVdecswAllocateComputeQueue(const ComputeConfigInfo* config, const ComputeMemoryInfo* memory, std::uint64_t* queue) {
    const auto& settings = Require(config, __func__);
    const auto& pool = Require(memory, __func__);
    if (queue == nullptr) throw std::invalid_argument("sceVdecswAllocateComputeQueue: null queue");
    if (pool.memory == nullptr || pool.memorySize < ComputeMemoryBytes) throw std::invalid_argument("sceVdecswAllocateComputeQueue: compute memory is too small");
    *queue = reinterpret_cast<std::uint64_t>(new ComputeQueue{settings.computePipeId, settings.computeQueueId});
    return 0;
}

int APS5_VABI sceVdecswReleaseComputeQueue(std::uint64_t queue) {
    if (queue == 0) throw std::invalid_argument("sceVdecswReleaseComputeQueue: null queue");
    delete reinterpret_cast<ComputeQueue*>(queue);
    return 0;
}

int APS5_VABI sceVdecswQueryDecoderMemoryInfo(const DecoderConfigInfo* config, DecoderMemoryInfo* memory) {
    const auto& settings = Require(config, __func__, offsetof(DecoderConfigInfo, reserved2));
    auto& info = Require(memory, __func__);
    if (settings.codecType != CodecAvc) return ErrorCodecType;
    if (settings.maxFrameWidth <= 0 || settings.maxFrameHeight <= 0) throw std::invalid_argument("sceVdecswQueryDecoderMemoryInfo: unbounded frame size");
    const auto pitch = AlignUp(static_cast<std::uint32_t>(settings.maxFrameWidth), PitchAlignment);
    const auto height = AlignUp(static_cast<std::uint32_t>(settings.maxFrameHeight), 16);
    info.cpuMemorySize = DecoderCpuMemoryBytes;
    info.gpuMemorySize = DecoderGpuMemoryBytes;
    info.cpuGpuMemorySize = DecoderGpuMemoryBytes;
    info.maxFrameBufferSize = (static_cast<std::uint64_t>(pitch) * height * 3 / 2 + FrameBufferAlignment - 1) / FrameBufferAlignment * FrameBufferAlignment;
    info.frameBufferAlignment = FrameBufferAlignment;
    info.reserved0 = 0;
    return 0;
}

int APS5_VABI sceVdecswCreateDecoder(const DecoderConfigInfo* config, const DecoderMemoryInfo* memory, std::uint64_t* decoder) {
    const auto& settings = Require(config, __func__, offsetof(DecoderConfigInfo, reserved2));
    Require(memory, __func__);
    if (decoder == nullptr) throw std::invalid_argument("sceVdecswCreateDecoder: null decoder");
    if (settings.codecType != CodecAvc) return ErrorCodecType;
    if (settings.maxFrameWidth <= 0 || settings.maxFrameHeight <= 0) throw std::invalid_argument("sceVdecswCreateDecoder: unbounded frame size");
    *decoder = reinterpret_cast<std::uint64_t>(new Decoder(AlignUp(static_cast<std::uint32_t>(settings.maxFrameWidth), 16), AlignUp(static_cast<std::uint32_t>(settings.maxFrameHeight), 16)));
    APS5_LOG_OUT("video decoder created: AVC profile %u level %u up to %dx%d", settings.profile, settings.maxLevel, settings.maxFrameWidth, settings.maxFrameHeight);
    return 0;
}

int APS5_VABI sceVdecswDeleteDecoder(std::uint64_t decoder) {
    delete &From(decoder, __func__);
    return 0;
}

int APS5_VABI sceVdecswResetDecoder(std::uint64_t decoder) {
    return From(decoder, __func__).Reset();
}

int APS5_VABI sceVdecswSetDecodeInput(std::uint64_t decoder, const InputData* input) {
    return From(decoder, __func__).SetInput(Require(input, __func__));
}

int APS5_VABI sceVdecswTrySyncDecodeInput(std::uint64_t decoder, InputSyncInfo* info) {
    return From(decoder, __func__).SyncInput(Require(info, __func__, offsetof(InputSyncInfo, reserved)));
}

int APS5_VABI sceVdecswSyncDecodeInput(std::uint64_t decoder, InputSyncInfo* info) {
    return sceVdecswTrySyncDecodeInput(decoder, info);
}

int APS5_VABI sceVdecswSetDecodeOutput(std::uint64_t decoder, const FrameBuffer* buffer) {
    return From(decoder, __func__).SetOutput(Require(buffer, __func__));
}

int APS5_VABI sceVdecswTrySyncDecodeOutput(std::uint64_t decoder, OutputInfo* output) {
    return From(decoder, __func__).SyncOutput(Require(output, __func__));
}

int APS5_VABI sceVdecswSyncDecodeOutput(std::uint64_t decoder, OutputInfo* output) {
    return sceVdecswTrySyncDecodeOutput(decoder, output);
}

int APS5_VABI sceVdecswFinalizeDecodeSequence(std::uint64_t decoder) {
    return From(decoder, __func__).Finalize();
}

int APS5_VABI sceVdecswGetAvcPictureInfo(const OutputInfo* output, AvcPictureInfo* first, AvcPictureInfo* second) {
    const auto& info = Require(output, __func__);
    if (info.codecType != CodecAvc) return ErrorCodecType;
    if (info.isValid == 0) throw std::invalid_argument("sceVdecswGetAvcPictureInfo: output holds no decoded picture");
    const auto picture = Delivered().Find(info.frameBuffer);
    if (first != nullptr) {
        Require(first, __func__);
        *first = picture;
    }
    if (second != nullptr) {
        Require(second, __func__);
        *second = AvcPictureInfo{};
        second->thisSize = sizeof(AvcPictureInfo);
    }
    return 0;
}

}

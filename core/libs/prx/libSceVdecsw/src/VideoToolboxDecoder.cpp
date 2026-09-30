#include "prx/libSceVdecsw/include/PictureDecoder.hpp"
#include "prx/libc/include/General.hpp"
#include <VideoToolbox/VideoToolbox.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace Vdecsw {

namespace {

class PixelBufferImage final : public IDecodedImage {
public:
    explicit PixelBufferImage(CVPixelBufferRef buffer) : buffer(CVPixelBufferRetain(buffer)) {}
    ~PixelBufferImage() override { CVPixelBufferRelease(buffer); }
    PixelBufferImage(const PixelBufferImage&) = delete;
    PixelBufferImage& operator=(const PixelBufferImage&) = delete;

    void CopyTo(const Nv12Layout& layout) const override {
        if (CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) throw std::runtime_error("VideoToolbox: cannot lock a decoded picture");
        const auto copyPlane = [&](std::size_t plane, std::uint8_t* destination, std::uint32_t rows) {
            const auto* source = static_cast<const std::uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(buffer, plane));
            const auto sourcePitch = CVPixelBufferGetBytesPerRowOfPlane(buffer, plane);
            const auto count = std::min<std::size_t>(rows, CVPixelBufferGetHeightOfPlane(buffer, plane));
            const auto bytes = std::min<std::size_t>({layout.width, sourcePitch, CVPixelBufferGetWidthOfPlane(buffer, plane) * (plane == 0 ? 1 : 2)});
            for (std::size_t row = 0; row < count; ++row) std::memcpy(destination + row * layout.pitch, source + row * sourcePitch, bytes);
        };
        copyPlane(0, layout.luma, layout.height);
        copyPlane(1, layout.chroma, (layout.height + 1) / 2);
        CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    }

private:
    CVPixelBufferRef buffer;
};

struct FrameResult {
    CVPixelBufferRef image = nullptr;
    OSStatus status = noErr;
};

void DecodedFrame(void*, void* frameReference, OSStatus status, VTDecodeInfoFlags, CVImageBufferRef image, CMTime, CMTime) {
    auto& result = *static_cast<FrameResult*>(frameReference);
    result.status = status;
    if (status == noErr && image != nullptr) result.image = CVPixelBufferRetain(image);
}

class VideoToolboxDecoder final : public IPictureDecoder {
public:
    VideoToolboxDecoder() = default;
    ~VideoToolboxDecoder() override { release(); }
    VideoToolboxDecoder(const VideoToolboxDecoder&) = delete;
    VideoToolboxDecoder& operator=(const VideoToolboxDecoder&) = delete;

    std::unique_ptr<IDecodedImage> Decode(std::span<const std::uint8_t> sps, std::span<const std::uint8_t> pps, std::span<const std::span<const std::uint8_t>> units, bool fullRange) override {
        if (session == nullptr || fullRange != sessionFullRange || !std::ranges::equal(sps, currentSps) || !std::ranges::equal(pps, currentPps)) createSession(sps, pps, fullRange);
        std::vector<std::uint8_t> sample;
        for (const auto unit : units) {
            const auto size = static_cast<std::uint32_t>(unit.size());
            const std::uint8_t length[4] = {static_cast<std::uint8_t>(size >> 24), static_cast<std::uint8_t>(size >> 16), static_cast<std::uint8_t>(size >> 8), static_cast<std::uint8_t>(size)};
            sample.insert(sample.end(), length, length + 4);
            sample.insert(sample.end(), unit.begin(), unit.end());
        }
        CMBlockBufferRef block = nullptr;
        check(CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, nullptr, sample.size(), kCFAllocatorDefault, nullptr, 0, sample.size(), kCMBlockBufferAssureMemoryNowFlag, &block), "CMBlockBufferCreateWithMemoryBlock");
        CMSampleBufferRef buffer = nullptr;
        const auto sampleSize = sample.size();
        auto status = CMBlockBufferReplaceDataBytes(sample.data(), block, 0, sample.size());
        if (status == noErr) status = CMSampleBufferCreateReady(kCFAllocatorDefault, block, format, 1, 0, nullptr, 1, &sampleSize, &buffer);
        CFRelease(block);
        check(status, "CMSampleBufferCreateReady");
        FrameResult result;
        VTDecodeInfoFlags info = 0;
        status = VTDecompressionSessionDecodeFrame(session, buffer, 0, &result, &info);
        CFRelease(buffer);
        if (status == noErr) status = result.status;
        if (status != noErr || result.image == nullptr) {
            if (result.image != nullptr) CVPixelBufferRelease(result.image);
            if (++failures <= 8) APS5_LOG_OUT("VideoToolbox: a picture failed to decode (status %d)", static_cast<int>(status));
            if (status == kVTInvalidSessionErr) release();
            return nullptr;
        }
        auto image = std::make_unique<PixelBufferImage>(result.image);
        CVPixelBufferRelease(result.image);
        return image;
    }

    void Reset() override { release(); }

private:
    static void check(OSStatus status, const char* operation) {
        if (status != noErr) throw std::runtime_error(std::string("VideoToolbox: ") + operation + " failed with status " + std::to_string(status));
    }

    void createSession(std::span<const std::uint8_t> sps, std::span<const std::uint8_t> pps, bool fullRange) {
        release();
        const std::uint8_t* sets[2] = {sps.data(), pps.data()};
        const std::size_t sizes[2] = {sps.size(), pps.size()};
        check(CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault, 2, sets, sizes, 4, &format), "CMVideoFormatDescriptionCreateFromH264ParameterSets");
        const std::int32_t pixelFormat = fullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
        auto* number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixelFormat);
        const void* keys[] = {kCVPixelBufferPixelFormatTypeKey};
        const void* values[] = {number};
        auto* attributes = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFRelease(number);
        const VTDecompressionOutputCallbackRecord callback{DecodedFrame, this};
        const auto status = VTDecompressionSessionCreate(kCFAllocatorDefault, format, nullptr, attributes, &callback, &session);
        CFRelease(attributes);
        if (status != noErr) {
            release();
            check(status, "VTDecompressionSessionCreate");
        }
        currentSps.assign(sps.begin(), sps.end());
        currentPps.assign(pps.begin(), pps.end());
        sessionFullRange = fullRange;
    }

    void release() {
        if (session != nullptr) {
            VTDecompressionSessionInvalidate(session);
            CFRelease(session);
            session = nullptr;
        }
        if (format != nullptr) {
            CFRelease(format);
            format = nullptr;
        }
        currentSps.clear();
        currentPps.clear();
    }

    CMVideoFormatDescriptionRef format = nullptr;
    VTDecompressionSessionRef session = nullptr;
    std::vector<std::uint8_t> currentSps;
    std::vector<std::uint8_t> currentPps;
    bool sessionFullRange = false;
    unsigned failures = 0;
};

}

std::unique_ptr<IPictureDecoder> CreatePlatformDecoder() {
    return std::make_unique<VideoToolboxDecoder>();
}

}

#include "prx/libSceVdecsw/include/AvcParser.hpp"
#ifdef _WIN32
#include "prx/libSceVdecsw/include/PictureDecoder.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

class BitWriter {
public:
    void Bit(std::uint32_t bit) {
        if (used == 0) bytes.push_back(0);
        if (bit != 0) bytes.back() |= static_cast<std::uint8_t>(0x80u >> used);
        used = (used + 1) % 8;
    }

    void Bits(std::uint64_t value, std::uint32_t count) {
        for (std::uint32_t i = count; i-- > 0;) Bit(static_cast<std::uint32_t>((value >> i) & 1u));
    }

    void Ue(std::uint32_t value) {
        const std::uint64_t code = std::uint64_t{value} + 1;
        std::uint32_t length = 0;
        while ((code >> (length + 1)) != 0) ++length;
        Bits(0, length);
        Bits(code, length + 1);
    }

    void Se(std::int32_t value) { Ue(value > 0 ? static_cast<std::uint32_t>(2 * value - 1) : static_cast<std::uint32_t>(-2 * value)); }

    void Align() {
        while (used != 0) Bit(0);
    }

    std::vector<std::uint8_t> Finish() {
        Bit(1);
        Align();
        return bytes;
    }

private:
    std::vector<std::uint8_t> bytes;
    std::uint32_t used = 0;
};

std::vector<std::uint8_t> Nal(std::uint8_t header, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> nal{header};
    std::size_t zeros = 0;
    for (const auto byte : payload) {
        if (zeros >= 2 && byte <= 3) {
            nal.push_back(3);
            zeros = 0;
        }
        zeros = byte == 0 ? zeros + 1 : 0;
        nal.push_back(byte);
    }
    return nal;
}

struct Sequence {
    std::uint32_t width = 64;
    std::uint32_t height = 48;
    std::uint8_t profile = 66;
    std::uint8_t level = 30;
    std::uint32_t references = 2;
    std::uint32_t pocType = 2;
    bool vui = false;
    bool fullRange = false;
    bool timing = false;
    bool restriction = false;
    std::uint32_t reorder = 0;
    std::uint32_t buffering = 2;
    std::uint32_t cropBottom = 0;
};

std::vector<std::uint8_t> Sps(const Sequence& sequence) {
    BitWriter writer;
    writer.Bits(sequence.profile, 8);
    writer.Bits(0, 8);
    writer.Bits(sequence.level, 8);
    writer.Ue(0);
    if (sequence.profile == 100) {
        writer.Ue(1);
        writer.Ue(0);
        writer.Ue(0);
        writer.Bit(0);
        writer.Bit(0);
    }
    writer.Ue(0);
    writer.Ue(sequence.pocType);
    if (sequence.pocType == 0) writer.Ue(4);
    writer.Ue(sequence.references);
    writer.Bit(0);
    writer.Ue(sequence.width / 16 - 1);
    writer.Ue(sequence.height / 16 - 1);
    writer.Bit(1);
    writer.Bit(1);
    writer.Bit(sequence.cropBottom != 0 ? 1 : 0);
    if (sequence.cropBottom != 0) {
        writer.Ue(0);
        writer.Ue(0);
        writer.Ue(0);
        writer.Ue(sequence.cropBottom / 2);
    }
    writer.Bit(sequence.vui ? 1 : 0);
    if (sequence.vui) {
        writer.Bit(0);
        writer.Bit(0);
        writer.Bit(sequence.fullRange ? 1 : 0);
        if (sequence.fullRange) {
            writer.Bits(5, 3);
            writer.Bit(1);
            writer.Bit(1);
            writer.Bits(1, 8);
            writer.Bits(1, 8);
            writer.Bits(1, 8);
        }
        writer.Bit(0);
        writer.Bit(sequence.timing ? 1 : 0);
        if (sequence.timing) {
            writer.Bits(1, 32);
            writer.Bits(120, 32);
            writer.Bit(0);
        }
        writer.Bit(0);
        writer.Bit(0);
        writer.Bit(0);
        writer.Bit(sequence.restriction ? 1 : 0);
        if (sequence.restriction) {
            writer.Bit(1);
            writer.Ue(0);
            writer.Ue(0);
            writer.Ue(11);
            writer.Ue(11);
            writer.Ue(sequence.reorder);
            writer.Ue(sequence.buffering);
        }
    }
    return Nal(0x67, writer.Finish());
}

std::vector<std::uint8_t> Pps(std::uint32_t id, std::int32_t chromaOffset) {
    BitWriter writer;
    writer.Ue(id);
    writer.Ue(0);
    writer.Bit(0);
    writer.Bit(0);
    writer.Ue(0);
    writer.Ue(0);
    writer.Ue(0);
    writer.Bit(0);
    writer.Bits(0, 2);
    writer.Se(0);
    writer.Se(0);
    writer.Se(chromaOffset);
    writer.Bit(1);
    writer.Bit(0);
    writer.Bit(0);
    return Nal(0x68, writer.Finish());
}

bool SameSequence(const Vdecsw::Avc::SequenceParameterSet& left, const Vdecsw::Avc::SequenceParameterSet& right) {
    return left.profileIdc == right.profileIdc && left.constraintFlags == right.constraintFlags && left.levelIdc == right.levelIdc && left.id == right.id &&
        left.chromaFormatIdc == right.chromaFormatIdc && left.log2MaxFrameNum == right.log2MaxFrameNum && left.picOrderCntType == right.picOrderCntType &&
        left.log2MaxPicOrderCntLsb == right.log2MaxPicOrderCntLsb && left.maxNumRefFrames == right.maxNumRefFrames && left.picWidthInMbsMinus1 == right.picWidthInMbsMinus1 &&
        left.picHeightInMapUnitsMinus1 == right.picHeightInMapUnitsMinus1 && left.frameMbsOnly == right.frameMbsOnly && left.frameCropping == right.frameCropping &&
        left.cropLeft == right.cropLeft && left.cropRight == right.cropRight && left.cropTop == right.cropTop && left.cropBottom == right.cropBottom &&
        left.videoSignalTypePresent == right.videoSignalTypePresent && left.videoFormat == right.videoFormat && left.videoFullRange == right.videoFullRange &&
        left.colourPrimaries == right.colourPrimaries && left.transferCharacteristics == right.transferCharacteristics && left.matrixCoefficients == right.matrixCoefficients &&
        left.timingInfoPresent == right.timingInfoPresent && left.numUnitsInTick == right.numUnitsInTick && left.timeScale == right.timeScale &&
        left.fixedFrameRate == right.fixedFrameRate && left.picStructPresent == right.picStructPresent;
}

void testRewriting() {
    struct Case {
        const char* name;
        Sequence sequence;
        std::uint32_t buffering;
    };
    Sequence plain;
    plain.width = 1920;
    plain.height = 1088;
    plain.level = 40;
    Sequence references = plain;
    references.references = 5;
    Sequence signalled;
    signalled.vui = true;
    signalled.fullRange = true;
    signalled.timing = true;
    Sequence restricted = signalled;
    restricted.profile = 100;
    restricted.level = 51;
    restricted.pocType = 0;
    restricted.references = 4;
    restricted.restriction = true;
    restricted.reorder = 2;
    restricted.buffering = 4;
    Sequence cropped = restricted;
    cropped.height = 64;
    cropped.cropBottom = 16;
    const Case cases[] = {{"no VUI", plain, 4}, {"no VUI with more references than the level's picture buffer", references, 5}, {"VUI without restriction", signalled, 16}, {"VUI with restriction", restricted, 4}, {"cropped frame", cropped, 4}};
    for (const auto& entry : cases) {
        const std::string name = entry.name;
        const auto nal = Sps(entry.sequence);
        const auto original = Vdecsw::Avc::ParseSps(nal);
        const auto rewritten = Vdecsw::Avc::WithoutReordering(nal);
        const auto parsed = Vdecsw::Avc::ParseSps(rewritten);
        check(rewritten.front() == nal.front(), name + ": the NAL header changed");
        check(parsed.vuiPresent && parsed.bitstreamRestriction && parsed.maxNumReorderFrames == 0 && parsed.ReorderDepth() == 0, name + ": the rewritten SPS still allows reordering");
        check(parsed.maxDecFrameBuffering == entry.buffering, name + ": the rewritten SPS has the wrong picture buffer size " + std::to_string(parsed.maxDecFrameBuffering));
        check(SameSequence(original, parsed), name + ": the rewrite changed other SPS fields");
        check(Vdecsw::Avc::WithoutReordering(rewritten) == rewritten, name + ": rewriting a rewritten SPS changed it");
        for (std::size_t i = 0; i + 2 < rewritten.size(); ++i) check(rewritten[i] != 0 || rewritten[i + 1] != 0 || rewritten[i + 2] > 2, name + ": the rewritten SPS emulates a start code");
        std::vector<std::uint8_t> stream{0, 0, 0, 1};
        stream.insert(stream.end(), rewritten.begin(), rewritten.end());
        const auto pps = Pps(0, 0);
        stream.insert(stream.end(), {0, 0, 1});
        stream.insert(stream.end(), pps.begin(), pps.end());
        const auto units = Vdecsw::Avc::SplitAnnexB(stream);
        check(units.size() == 2 && std::ranges::equal(units[0].bytes, rewritten) && units[0].type == 7 && std::ranges::equal(units[1].bytes, pps), name + ": Annex-B framing does not round-trip the rewritten SPS");
    }
    check(Vdecsw::Avc::ParseSps(Sps(plain)).DpbFrames() == 4, "1920x1088 at level 4.0 should hold four frames");
    check(Vdecsw::Avc::ParseSps(Sps(restricted)).ReorderDepth() == 2, "the original restricted SPS lost its reorder depth");
}

#ifdef _WIN32

enum class Coding { Idr, Skip, Predicted, Bidirectional };

struct Picture {
    Coding coding;
    std::uint32_t frameNum;
    std::uint32_t order;
    std::uint32_t content;
};

std::uint8_t Sample(std::uint32_t content, std::uint32_t plane, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint8_t>(1 + (content * 53 + plane * 29 + x * 7 + y * 13) % 255);
}

std::vector<std::uint8_t> Slice(const Sequence& sequence, std::uint32_t ppsId, const Picture& picture, std::uint32_t first, std::uint32_t last) {
    BitWriter writer;
    const auto columns = sequence.width / 16;
    const bool reference = picture.coding != Coding::Bidirectional;
    writer.Ue(first);
    writer.Ue(picture.coding == Coding::Idr ? 7 : picture.coding == Coding::Bidirectional ? 6 : 5);
    writer.Ue(ppsId);
    writer.Bits(picture.frameNum, 4);
    if (picture.coding == Coding::Idr) writer.Ue(0);
    if (sequence.pocType == 0) writer.Bits(picture.order, 8);
    if (picture.coding == Coding::Bidirectional) writer.Bit(1);
    if (picture.coding != Coding::Idr) {
        writer.Bit(0);
        writer.Bit(0);
        if (picture.coding == Coding::Bidirectional) writer.Bit(0);
    }
    if (picture.coding == Coding::Idr) {
        writer.Bit(0);
        writer.Bit(0);
    } else if (reference) {
        writer.Bit(0);
    }
    writer.Se(0);
    writer.Ue(1);
    if (picture.coding == Coding::Skip) {
        writer.Ue(last - first);
    } else {
        for (auto macroblock = first; macroblock < last; ++macroblock) {
            if (picture.coding != Coding::Idr) writer.Ue(0);
            writer.Ue(picture.coding == Coding::Idr ? 25 : picture.coding == Coding::Predicted ? 30 : 48);
            writer.Align();
            const auto left = macroblock % columns;
            const auto top = macroblock / columns;
            for (std::uint32_t y = 0; y < 16; ++y) for (std::uint32_t x = 0; x < 16; ++x) writer.Bits(Sample(picture.content, 0, left * 16 + x, top * 16 + y), 8);
            for (std::uint32_t plane = 1; plane <= 2; ++plane) for (std::uint32_t y = 0; y < 8; ++y) for (std::uint32_t x = 0; x < 8; ++x) writer.Bits(Sample(picture.content, plane, left * 8 + x, top * 8 + y), 8);
        }
    }
    return Nal(static_cast<std::uint8_t>((reference ? 0x60u : 0u) | (picture.coding == Coding::Idr ? 5u : 1u)), writer.Finish());
}

std::vector<std::uint8_t> Sei() {
    BitWriter writer;
    writer.Bits(5, 8);
    writer.Bits(17, 8);
    for (std::uint32_t byte = 0; byte < 17; ++byte) writer.Bits(0x40 + byte, 8);
    return Nal(0x06, writer.Finish());
}

class Stream {
public:
    explicit Stream(const Sequence& sequence, std::uint32_t ppsId = 0, std::int32_t chromaOffset = 0) : sequence(sequence), sps(Sps(sequence)), pps(Pps(ppsId, chromaOffset)), ppsId(ppsId) {}

    std::unique_ptr<Vdecsw::IDecodedImage> Decode(Vdecsw::IPictureDecoder& decoder, const Picture& picture, std::uint32_t slices = 1, bool sei = false) const {
        const auto macroblocks = (sequence.width / 16) * (sequence.height / 16);
        std::vector<std::vector<std::uint8_t>> nals;
        if (sei) nals.push_back(Sei());
        for (std::uint32_t slice = 0; slice < slices; ++slice) nals.push_back(Slice(sequence, ppsId, picture, macroblocks * slice / slices, macroblocks * (slice + 1) / slices));
        std::vector<std::span<const std::uint8_t>> units(nals.begin(), nals.end());
        return decoder.Decode(sps, pps, units, sequence.fullRange);
    }

    Sequence sequence;
    std::vector<std::uint8_t> sps;
    std::vector<std::uint8_t> pps;
    std::uint32_t ppsId;
};

std::size_t Mismatches(const Vdecsw::IDecodedImage& image, const Sequence& sequence, std::uint32_t content) {
    constexpr std::uint32_t pitch = 256;
    constexpr std::uint8_t untouched = 0xee;
    std::vector<std::uint8_t> frame(static_cast<std::size_t>(pitch) * sequence.height * 3 / 2, untouched);
    image.CopyTo({frame.data(), frame.data() + static_cast<std::size_t>(pitch) * sequence.height, pitch, sequence.width, sequence.height});
    std::size_t mismatches = 0;
    for (std::uint32_t y = 0; y < sequence.height; ++y) {
        for (std::uint32_t x = 0; x < pitch; ++x) {
            const auto expected = x < sequence.width ? Sample(content, 0, x, y) : untouched;
            if (frame[static_cast<std::size_t>(y) * pitch + x] != expected) ++mismatches;
        }
    }
    const auto* chroma = frame.data() + static_cast<std::size_t>(pitch) * sequence.height;
    for (std::uint32_t y = 0; y < sequence.height / 2; ++y) {
        for (std::uint32_t x = 0; x < pitch; ++x) {
            const auto expected = x < sequence.width ? Sample(content, 1 + x % 2, x / 2, y) : untouched;
            if (chroma[static_cast<std::size_t>(y) * pitch + x] != expected) ++mismatches;
        }
    }
    return mismatches;
}

void expectPicture(const std::unique_ptr<Vdecsw::IDecodedImage>& image, const Sequence& sequence, std::uint32_t content, const std::string& name) {
    check(image != nullptr, name + ": the decoder returned no picture");
    const auto mismatches = Mismatches(*image, sequence, content);
    check(mismatches == 0, name + ": " + std::to_string(mismatches) + " bytes differ from the coded picture");
}

void testProgressive() {
    auto decoder = Vdecsw::CreatePlatformDecoder();
    const Stream stream(Sequence{});
    expectPicture(stream.Decode(*decoder, {Coding::Idr, 0, 0, 0}), stream.sequence, 0, "progressive IDR picture");
    expectPicture(stream.Decode(*decoder, {Coding::Skip, 1, 0, 0}), stream.sequence, 0, "progressive skipped picture");
    expectPicture(stream.Decode(*decoder, {Coding::Predicted, 2, 0, 1}), stream.sequence, 1, "progressive predicted picture");
    expectPicture(stream.Decode(*decoder, {Coding::Predicted, 3, 0, 2}, 3, true), stream.sequence, 2, "progressive picture in three slices after an SEI");
}

void testReordered(bool restricted) {
    const std::string name = restricted ? "B pictures with a reorder restriction" : "B pictures without VUI";
    Sequence sequence;
    sequence.profile = 77;
    sequence.pocType = 0;
    sequence.vui = restricted;
    sequence.restriction = restricted;
    sequence.reorder = 1;
    sequence.buffering = 3;
    auto decoder = Vdecsw::CreatePlatformDecoder();
    const Stream stream(sequence);
    const Picture pictures[] = {{Coding::Idr, 0, 0, 0}, {Coding::Predicted, 1, 8, 1}, {Coding::Bidirectional, 2, 4, 2}, {Coding::Predicted, 2, 16, 3}, {Coding::Bidirectional, 3, 12, 4}};
    for (const auto& picture : pictures) expectPicture(stream.Decode(*decoder, picture), sequence, picture.content, name + ", picture " + std::to_string(picture.content));
}

void testFullRange() {
    Sequence sequence;
    sequence.vui = true;
    sequence.fullRange = true;
    auto decoder = Vdecsw::CreatePlatformDecoder();
    const Stream stream(sequence);
    expectPicture(stream.Decode(*decoder, {Coding::Idr, 0, 0, 3}), sequence, 3, "full-range picture");
}

void testCropped() {
    Sequence sequence;
    sequence.height = 64;
    sequence.cropBottom = 16;
    Sequence visible = sequence;
    visible.height = 48;
    auto decoder = Vdecsw::CreatePlatformDecoder();
    const Stream stream(sequence);
    for (std::uint32_t frame = 0; frame < 3; ++frame) {
        const auto image = stream.Decode(*decoder, {frame == 0 ? Coding::Idr : Coding::Predicted, frame, 0, frame});
        check(image != nullptr && Mismatches(*image, visible, frame) == 0, "cropped picture " + std::to_string(frame) + " differs in its visible rows");
    }
}

void testSequenceChange() {
    auto decoder = Vdecsw::CreatePlatformDecoder();
    const Stream first(Sequence{});
    Sequence larger;
    larger.width = 80;
    larger.height = 64;
    const Stream second(larger);
    expectPicture(first.Decode(*decoder, {Coding::Idr, 0, 0, 0}), first.sequence, 0, "picture before the sequence change");
    expectPicture(second.Decode(*decoder, {Coding::Idr, 0, 0, 5}), larger, 5, "IDR picture after the sequence change");
    expectPicture(second.Decode(*decoder, {Coding::Skip, 1, 0, 5}), larger, 5, "skipped picture after the sequence change");
    expectPicture(first.Decode(*decoder, {Coding::Idr, 0, 0, 6}), first.sequence, 6, "IDR picture after returning to the first sequence");
}

void testPictureParameterChange() {
    auto decoder = Vdecsw::CreatePlatformDecoder();
    const Stream first(Sequence{});
    const Stream second(Sequence{}, 1, 3);
    expectPicture(first.Decode(*decoder, {Coding::Idr, 0, 0, 0}), first.sequence, 0, "picture before the PPS change");
    expectPicture(second.Decode(*decoder, {Coding::Predicted, 1, 0, 6}), second.sequence, 6, "picture with the new PPS");
    expectPicture(second.Decode(*decoder, {Coding::Skip, 2, 0, 6}), second.sequence, 6, "skipped picture with the new PPS");
    expectPicture(first.Decode(*decoder, {Coding::Predicted, 3, 0, 7}), first.sequence, 7, "picture back on the first PPS");
}

void testReset() {
    auto decoder = Vdecsw::CreatePlatformDecoder();
    const Stream stream(Sequence{});
    expectPicture(stream.Decode(*decoder, {Coding::Idr, 0, 0, 0}), stream.sequence, 0, "picture before the reset");
    expectPicture(stream.Decode(*decoder, {Coding::Predicted, 1, 0, 1}), stream.sequence, 1, "second picture before the reset");
    decoder->Reset();
    const auto orphan = stream.Decode(*decoder, {Coding::Skip, 2, 0, 1});
    check(orphan == nullptr || Mismatches(*orphan, stream.sequence, 1) != 0, "a skipped picture after the reset still found its reference picture");
    expectPicture(stream.Decode(*decoder, {Coding::Idr, 0, 0, 2}), stream.sequence, 2, "IDR picture after the reset");
    expectPicture(stream.Decode(*decoder, {Coding::Skip, 1, 0, 2}), stream.sequence, 2, "skipped picture after the reset");
}

void testHeldPictures() {
    auto decoder = Vdecsw::CreatePlatformDecoder();
    const Stream stream(Sequence{});
    std::vector<std::unique_ptr<Vdecsw::IDecodedImage>> held;
    held.push_back(stream.Decode(*decoder, {Coding::Idr, 0, 0, 0}));
    for (std::uint32_t frame = 1; frame < 12; ++frame) held.push_back(stream.Decode(*decoder, {Coding::Predicted, frame, 0, frame}));
    for (std::uint32_t frame = 0; frame < held.size(); ++frame) expectPicture(held[frame], stream.sequence, frame, "held picture " + std::to_string(frame));
    held.clear();
    for (std::uint32_t frame = 12; frame < 16; ++frame) expectPicture(stream.Decode(*decoder, {Coding::Predicted, frame, 0, frame}), stream.sequence, frame, "picture after releasing held pictures");
}

bool DecoderInstalled() {
    return LoadLibraryExW(L"msmpeg2vdec.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) != nullptr;
}

#endif

}

int main() {
    try {
        testRewriting();
#ifdef _WIN32
        if (DecoderInstalled()) {
            testProgressive();
            testReordered(true);
            testReordered(false);
            testFullRange();
            testCropped();
            testSequenceChange();
            testPictureParameterChange();
            testReset();
            testHeldPictures();
        } else {
            std::puts("the Media Foundation H.264 decoder is not installed: decoding checks skipped");
        }
#endif
        std::puts("Vdecsw SPS rewriting and picture decoding tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

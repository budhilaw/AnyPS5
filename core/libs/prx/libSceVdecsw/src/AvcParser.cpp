#include "prx/libSceVdecsw/include/AvcParser.hpp"
#include <algorithm>
#include <stdexcept>

namespace Vdecsw::Avc {

namespace {

class BitReader {
public:
    explicit BitReader(std::span<const std::uint8_t> bytes) : bytes(bytes) {}

    std::uint32_t Bits(std::uint32_t count) {
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < count; ++i) value = (value << 1u) | Bit();
        return value;
    }

    std::uint32_t Bit() {
        if (position >= bytes.size() * 8) throw std::runtime_error("AVC bitstream ends inside a syntax element");
        const auto bit = (bytes[position / 8] >> (7 - position % 8)) & 1u;
        ++position;
        return bit;
    }

    std::uint32_t Ue() {
        std::uint32_t zeros = 0;
        while (Bit() == 0) {
            if (++zeros > 31) throw std::runtime_error("AVC Exp-Golomb code is too long");
        }
        return zeros == 0 ? 0 : (1u << zeros) - 1 + Bits(zeros);
    }

    std::int32_t Se() {
        const auto code = Ue();
        return (code & 1u) != 0 ? static_cast<std::int32_t>((code + 1) / 2) : -static_cast<std::int32_t>(code / 2);
    }

private:
    std::span<const std::uint8_t> bytes;
    std::size_t position = 0;
};

void SkipScalingList(BitReader& reader, std::uint32_t size) {
    std::int32_t last = 8;
    std::int32_t next = 8;
    for (std::uint32_t i = 0; i < size; ++i) {
        if (next != 0) next = (last + reader.Se() + 256) % 256;
        last = next == 0 ? last : next;
    }
}

void SkipHrd(BitReader& reader) {
    const auto count = reader.Ue() + 1;
    reader.Bits(8);
    for (std::uint32_t i = 0; i < count; ++i) {
        reader.Ue();
        reader.Ue();
        reader.Bit();
    }
    reader.Bits(20);
}

void ParseVui(BitReader& reader, SequenceParameterSet& sps) {
    sps.aspectRatioInfoPresent = reader.Bit() != 0;
    if (sps.aspectRatioInfoPresent) {
        sps.aspectRatioIdc = static_cast<std::uint8_t>(reader.Bits(8));
        if (sps.aspectRatioIdc == 255) {
            sps.sarWidth = static_cast<std::uint16_t>(reader.Bits(16));
            sps.sarHeight = static_cast<std::uint16_t>(reader.Bits(16));
        }
    }
    if (reader.Bit() != 0) reader.Bit();
    sps.videoSignalTypePresent = reader.Bit() != 0;
    if (sps.videoSignalTypePresent) {
        sps.videoFormat = static_cast<std::uint8_t>(reader.Bits(3));
        sps.videoFullRange = reader.Bit() != 0;
        sps.colourDescriptionPresent = reader.Bit() != 0;
        if (sps.colourDescriptionPresent) {
            sps.colourPrimaries = static_cast<std::uint8_t>(reader.Bits(8));
            sps.transferCharacteristics = static_cast<std::uint8_t>(reader.Bits(8));
            sps.matrixCoefficients = static_cast<std::uint8_t>(reader.Bits(8));
        }
    }
    if (reader.Bit() != 0) {
        reader.Ue();
        reader.Ue();
    }
    sps.timingInfoPresent = reader.Bit() != 0;
    if (sps.timingInfoPresent) {
        sps.numUnitsInTick = reader.Bits(32);
        sps.timeScale = reader.Bits(32);
        sps.fixedFrameRate = reader.Bit() != 0;
    }
    const bool nalHrd = reader.Bit() != 0;
    if (nalHrd) SkipHrd(reader);
    const bool vclHrd = reader.Bit() != 0;
    if (vclHrd) SkipHrd(reader);
    if (nalHrd || vclHrd) reader.Bit();
    sps.picStructPresent = reader.Bit() != 0;
    sps.bitstreamRestriction = reader.Bit() != 0;
    if (sps.bitstreamRestriction) {
        reader.Bit();
        reader.Ue();
        reader.Ue();
        reader.Ue();
        reader.Ue();
        sps.maxNumReorderFrames = reader.Ue();
        sps.maxDecFrameBuffering = reader.Ue();
    }
}

std::uint32_t MaxDpbMbs(std::uint8_t levelIdc) {
    switch (levelIdc) {
        case 9: case 10: case 11: return levelIdc == 11 ? 900 : 396;
        case 12: case 13: case 20: return 2376;
        case 21: return 4752;
        case 22: case 30: return 8100;
        case 31: return 18000;
        case 32: return 20480;
        case 40: case 41: return 32768;
        case 42: return 34816;
        case 50: return 110400;
        case 51: case 52: return 184320;
        default: return 696320;
    }
}

}

std::uint32_t SequenceParameterSet::ReorderDepth() const {
    if (bitstreamRestriction) return maxNumReorderFrames;
    if (profileIdc == 66) return 0;
    const auto frameMbs = (picWidthInMbsMinus1 + 1) * (CodedHeight() / 16);
    return std::clamp<std::uint32_t>(MaxDpbMbs(levelIdc) / std::max<std::uint32_t>(frameMbs, 1), 1, 16);
}

std::vector<NalUnit> SplitAnnexB(std::span<const std::uint8_t> stream) {
    std::vector<NalUnit> units;
    const auto startCode = [&](std::size_t at) { return at + 2 < stream.size() && stream[at] == 0 && stream[at + 1] == 0 && stream[at + 2] == 1; };
    std::size_t cursor = 0;
    while (cursor < stream.size() && !startCode(cursor)) ++cursor;
    while (cursor < stream.size()) {
        const auto begin = cursor + 3;
        auto end = begin;
        while (end < stream.size() && !startCode(end)) ++end;
        auto last = end;
        while (last > begin && stream[last - 1] == 0) --last;
        if (last > begin) units.push_back({stream.subspan(begin, last - begin), static_cast<std::uint8_t>(stream[begin] & 0x1fu), static_cast<std::uint8_t>((stream[begin] >> 5) & 3u)});
        cursor = end;
    }
    return units;
}

std::vector<std::uint8_t> Unescape(std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> result;
    result.reserve(payload.size());
    std::size_t zeros = 0;
    for (const auto byte : payload) {
        if (zeros >= 2 && byte == 3) {
            zeros = 0;
            continue;
        }
        zeros = byte == 0 ? zeros + 1 : 0;
        result.push_back(byte);
    }
    return result;
}

SequenceParameterSet ParseSps(std::span<const std::uint8_t> nal) {
    const auto rbsp = Unescape(nal.subspan(1));
    BitReader reader(rbsp);
    SequenceParameterSet sps;
    sps.profileIdc = static_cast<std::uint8_t>(reader.Bits(8));
    sps.constraintFlags = static_cast<std::uint8_t>(reader.Bits(8));
    sps.levelIdc = static_cast<std::uint8_t>(reader.Bits(8));
    sps.id = reader.Ue();
    switch (sps.profileIdc) {
        case 100: case 110: case 122: case 244: case 44: case 83: case 86: case 118: case 128: case 138: case 139: case 134: case 135: {
            sps.chromaFormatIdc = reader.Ue();
            if (sps.chromaFormatIdc == 3) sps.separateColourPlane = reader.Bit() != 0;
            reader.Ue();
            reader.Ue();
            reader.Bit();
            if (reader.Bit() != 0) {
                const auto lists = sps.chromaFormatIdc != 3 ? 8u : 12u;
                for (std::uint32_t i = 0; i < lists; ++i) {
                    if (reader.Bit() != 0) SkipScalingList(reader, i < 6 ? 16 : 64);
                }
            }
            break;
        }
        default: break;
    }
    sps.log2MaxFrameNum = reader.Ue() + 4;
    sps.picOrderCntType = reader.Ue();
    if (sps.picOrderCntType == 0) {
        sps.log2MaxPicOrderCntLsb = reader.Ue() + 4;
    } else if (sps.picOrderCntType == 1) {
        reader.Bit();
        reader.Se();
        reader.Se();
        const auto cycle = reader.Ue();
        for (std::uint32_t i = 0; i < cycle; ++i) reader.Se();
    }
    sps.maxNumRefFrames = reader.Ue();
    reader.Bit();
    sps.picWidthInMbsMinus1 = reader.Ue();
    sps.picHeightInMapUnitsMinus1 = reader.Ue();
    sps.frameMbsOnly = reader.Bit() != 0;
    if (!sps.frameMbsOnly) reader.Bit();
    reader.Bit();
    sps.frameCropping = reader.Bit() != 0;
    if (sps.frameCropping) {
        sps.cropLeft = reader.Ue();
        sps.cropRight = reader.Ue();
        sps.cropTop = reader.Ue();
        sps.cropBottom = reader.Ue();
    }
    if (reader.Bit() != 0) ParseVui(reader, sps);
    if (sps.log2MaxFrameNum > 16 || sps.log2MaxPicOrderCntLsb > 16 || sps.picOrderCntType > 2) throw std::runtime_error("AVC sequence parameter set is out of range");
    return sps;
}

PictureParameterSet ParsePps(std::span<const std::uint8_t> nal) {
    const auto rbsp = Unescape(nal.subspan(1, std::min<std::size_t>(nal.size() - 1, 16)));
    BitReader reader(rbsp);
    PictureParameterSet pps;
    pps.id = reader.Ue();
    pps.spsId = reader.Ue();
    return pps;
}

std::uint32_t SlicePpsId(std::span<const std::uint8_t> nal) {
    const auto rbsp = Unescape(nal.subspan(1, std::min<std::size_t>(nal.size() - 1, 16)));
    BitReader reader(rbsp);
    reader.Ue();
    reader.Ue();
    return reader.Ue();
}

SliceHeader ParseSliceHeader(std::span<const std::uint8_t> nal, const SequenceParameterSet& sps) {
    const auto rbsp = Unescape(nal.subspan(1, std::min<std::size_t>(nal.size() - 1, 64)));
    BitReader reader(rbsp);
    SliceHeader slice;
    reader.Ue();
    slice.sliceType = reader.Ue();
    slice.ppsId = reader.Ue();
    if (sps.separateColourPlane) reader.Bits(2);
    slice.frameNum = reader.Bits(sps.log2MaxFrameNum);
    if (!sps.frameMbsOnly) {
        slice.fieldPic = reader.Bit() != 0;
        if (slice.fieldPic) slice.bottomField = reader.Bit() != 0;
    }
    if ((nal[0] & 0x1fu) == static_cast<std::uint8_t>(NalType::IdrSlice)) reader.Ue();
    if (sps.picOrderCntType == 0) slice.picOrderCntLsb = reader.Bits(sps.log2MaxPicOrderCntLsb);
    return slice;
}

std::int64_t PictureOrder::Next(const NalUnit& nal, const SliceHeader& slice, const SequenceParameterSet& sps) {
    const bool idr = nal.type == static_cast<std::uint8_t>(NalType::IdrSlice);
    if (idr) {
        ++epoch;
        previousMsb = 0;
        previousLsb = 0;
        previousFrameNum = 0;
        frameNumOffset = 0;
        decodeCounter = 0;
    }
    std::int64_t order = 0;
    if (sps.picOrderCntType == 0) {
        const auto maxLsb = 1u << sps.log2MaxPicOrderCntLsb;
        auto msb = previousMsb;
        if (slice.picOrderCntLsb < previousLsb && previousLsb - slice.picOrderCntLsb >= maxLsb / 2) msb += static_cast<std::int32_t>(maxLsb);
        else if (slice.picOrderCntLsb > previousLsb && slice.picOrderCntLsb - previousLsb > maxLsb / 2) msb -= static_cast<std::int32_t>(maxLsb);
        order = static_cast<std::int64_t>(msb) + slice.picOrderCntLsb;
        if (nal.refIdc != 0) {
            previousMsb = msb;
            previousLsb = slice.picOrderCntLsb;
        }
    } else if (sps.picOrderCntType == 2) {
        if (!idr && slice.frameNum < previousFrameNum) frameNumOffset += std::int64_t{1} << sps.log2MaxFrameNum;
        previousFrameNum = slice.frameNum;
        order = 2 * (frameNumOffset + slice.frameNum) - (nal.refIdc == 0 ? 1 : 0);
    } else {
        order = decodeCounter;
    }
    ++decodeCounter;
    return (epoch << 40) + order + (std::int64_t{1} << 39);
}

void PictureOrder::Reset() {
    ++epoch;
    previousMsb = 0;
    previousLsb = 0;
    previousFrameNum = 0;
    frameNumOffset = 0;
    decodeCounter = 0;
}

}

#ifndef CORE_LIBS_PRX_LIBSCEVDECSW_INCLUDE_AVCPARSER_HPP
#define CORE_LIBS_PRX_LIBSCEVDECSW_INCLUDE_AVCPARSER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Vdecsw::Avc {

enum class NalType : std::uint8_t {
    Slice = 1,
    IdrSlice = 5,
    Sei = 6,
    Sps = 7,
    Pps = 8,
    AccessUnitDelimiter = 9,
    EndOfSequence = 10,
    EndOfStream = 11,
    Filler = 12
};

struct NalUnit {
    std::span<const std::uint8_t> bytes;
    std::uint8_t type;
    std::uint8_t refIdc;
};

struct SequenceParameterSet {
    std::uint8_t profileIdc = 0;
    std::uint8_t constraintFlags = 0;
    std::uint8_t levelIdc = 0;
    std::uint32_t id = 0;
    std::uint32_t chromaFormatIdc = 1;
    bool separateColourPlane = false;
    std::uint32_t log2MaxFrameNum = 4;
    std::uint32_t picOrderCntType = 0;
    std::uint32_t log2MaxPicOrderCntLsb = 4;
    std::uint32_t maxNumRefFrames = 0;
    std::uint32_t picWidthInMbsMinus1 = 0;
    std::uint32_t picHeightInMapUnitsMinus1 = 0;
    bool frameMbsOnly = true;
    bool frameCropping = false;
    std::uint32_t cropLeft = 0;
    std::uint32_t cropRight = 0;
    std::uint32_t cropTop = 0;
    std::uint32_t cropBottom = 0;
    bool vuiPresent = false;
    bool aspectRatioInfoPresent = false;
    std::uint8_t aspectRatioIdc = 0;
    std::uint16_t sarWidth = 0;
    std::uint16_t sarHeight = 0;
    bool videoSignalTypePresent = false;
    std::uint8_t videoFormat = 5;
    bool videoFullRange = false;
    bool colourDescriptionPresent = false;
    std::uint8_t colourPrimaries = 2;
    std::uint8_t transferCharacteristics = 2;
    std::uint8_t matrixCoefficients = 2;
    bool timingInfoPresent = false;
    std::uint32_t numUnitsInTick = 0;
    std::uint32_t timeScale = 0;
    bool fixedFrameRate = false;
    bool picStructPresent = false;
    bool bitstreamRestriction = false;
    std::uint32_t maxNumReorderFrames = 0;
    std::uint32_t maxDecFrameBuffering = 0;
    std::size_t vuiBit = 0;
    std::size_t restrictionBit = 0;
    std::size_t reorderBit = 0;

    std::uint32_t CodedWidth() const { return (picWidthInMbsMinus1 + 1) * 16; }
    std::uint32_t CodedHeight() const { return (2 - (frameMbsOnly ? 1 : 0)) * (picHeightInMapUnitsMinus1 + 1) * 16; }
    std::uint32_t CropUnitX() const { return chromaFormatIdc == 0 || separateColourPlane || chromaFormatIdc == 3 ? 1 : 2; }
    std::uint32_t CropUnitY() const { return (chromaFormatIdc == 1 ? 2 : 1) * (2 - (frameMbsOnly ? 1 : 0)); }
    std::uint32_t DpbFrames() const;
    std::uint32_t ReorderDepth() const;
};

struct PictureParameterSet {
    std::uint32_t id = 0;
    std::uint32_t spsId = 0;
};

struct SliceHeader {
    std::uint32_t sliceType = 0;
    std::uint32_t ppsId = 0;
    std::uint32_t frameNum = 0;
    bool fieldPic = false;
    bool bottomField = false;
    std::uint32_t picOrderCntLsb = 0;
};

std::vector<NalUnit> SplitAnnexB(std::span<const std::uint8_t> stream);
std::vector<std::uint8_t> Unescape(std::span<const std::uint8_t> payload);
SequenceParameterSet ParseSps(std::span<const std::uint8_t> nal);
std::vector<std::uint8_t> WithoutReordering(std::span<const std::uint8_t> nal);
PictureParameterSet ParsePps(std::span<const std::uint8_t> nal);
std::uint32_t SlicePpsId(std::span<const std::uint8_t> nal);
SliceHeader ParseSliceHeader(std::span<const std::uint8_t> nal, const SequenceParameterSet& sps);

class PictureOrder {
public:
    std::int64_t Next(const NalUnit& nal, const SliceHeader& slice, const SequenceParameterSet& sps);
    void Reset();

private:
    std::int64_t epoch = 0;
    std::int32_t previousMsb = 0;
    std::uint32_t previousLsb = 0;
    std::int64_t decodeCounter = 0;
    std::uint32_t previousFrameNum = 0;
    std::int64_t frameNumOffset = 0;
};

}

#endif

#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include <array>
#include <string>
#include <string_view>

namespace {

using namespace AgcDriver::Graphics;
using Shape = ShaderRecompiler::DescriptorImageShape;

struct Fields {
    std::uint64_t base40 = 0x123456ull;
    std::uint32_t minLod = 0;
    std::uint32_t format = 56;
    std::uint32_t width = 16;
    std::uint32_t height = 16;
    std::uint32_t dstSelX = 4;
    std::uint32_t dstSelY = 5;
    std::uint32_t dstSelZ = 6;
    std::uint32_t dstSelW = 7;
    std::uint32_t baseLevel = 0;
    std::uint32_t lastLevel = 0;
    std::uint32_t tileModeRaw = 0x00;
    std::uint32_t bcSwizzle = 0;
    std::uint32_t typeRaw = 9;
    std::uint32_t depth = 0;
    std::uint32_t baseArray = 0;
    std::uint32_t arrayPitch = 0;
    std::uint32_t maxMip = 0;
    std::uint32_t minLodWarn = 0;
    std::uint32_t perfMod = 0;
    bool cornerSample = false;
    bool mipStatsCntEn = false;
    bool prtDefColor = false;
    std::uint32_t mipStatsCntId = 0;
    bool msaaDepth = false;
    std::uint32_t maxUncompBlkSize = 0;
    std::uint32_t maxCompBlkSize = 0;
    bool metaPipeAligned = false;
    bool writeCompress = false;
    bool metaCompress = false;
    bool dccAlphaPos = false;
    bool dccColorTransf = false;
    std::uint64_t metaAddr = 0;
};

std::array<std::uint32_t, 8> pack(const Fields& f) {
    std::array<std::uint32_t, 8> words{};
    const auto widthMinus1 = f.width - 1u;
    const auto heightMinus1 = f.height - 1u;
    words[0] = static_cast<std::uint32_t>(f.base40 & 0xffffffffull);
    words[1] = static_cast<std::uint32_t>((f.base40 >> 32u) & 0xffull)
        | ((f.minLod & 0xfffu) << 8u)
        | ((f.format & 0x1ffu) << 20u)
        | ((widthMinus1 & 0x3u) << 30u);
    words[2] = ((widthMinus1 >> 2u) & 0xfffu) | ((heightMinus1 & 0x3fffu) << 14u);
    words[3] = (f.dstSelX & 0x7u) | ((f.dstSelY & 0x7u) << 3u) | ((f.dstSelZ & 0x7u) << 6u) | ((f.dstSelW & 0x7u) << 9u)
        | ((f.baseLevel & 0xfu) << 12u) | ((f.lastLevel & 0xfu) << 16u) | ((f.tileModeRaw & 0x1fu) << 20u)
        | ((f.bcSwizzle & 0x7u) << 25u) | ((f.typeRaw & 0xfu) << 28u);
    words[4] = (f.depth & 0x1fffu) | ((f.baseArray & 0x1fffu) << 16u);
    words[5] = (f.arrayPitch & 0xfu) | ((f.maxMip & 0xfu) << 4u) | ((f.minLodWarn & 0xfffu) << 8u)
        | ((f.perfMod & 0x7u) << 20u) | ((f.cornerSample ? 1u : 0u) << 23u) | ((f.mipStatsCntEn ? 1u : 0u) << 25u)
        | ((f.prtDefColor ? 1u : 0u) << 26u);
    words[6] = (f.mipStatsCntId & 0xffu) | ((f.msaaDepth ? 1u : 0u) << 10u) | ((f.maxUncompBlkSize & 0x3u) << 15u)
        | ((f.maxCompBlkSize & 0x3u) << 17u) | ((f.metaPipeAligned ? 1u : 0u) << 19u) | ((f.writeCompress ? 1u : 0u) << 20u)
        | ((f.metaCompress ? 1u : 0u) << 21u) | ((f.dccAlphaPos ? 1u : 0u) << 22u) | ((f.dccColorTransf ? 1u : 0u) << 23u)
        | static_cast<std::uint32_t>((f.metaAddr & 0xffull) << 24u);
    words[7] = static_cast<std::uint32_t>(f.metaAddr >> 8u);
    return words;
}

template<typename TAction>
void reject(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected guest texture test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected guest texture rejection: ") + std::string(reason));
}

void rejectFields(const Fields& f, std::string_view reason) {
    const auto words = pack(f);
    reject([&] { DecodeTextureResource(words); }, reason);
}

void requireSameTexture(const GuestTextureResource& decoded, const GuestTextureResource& expected, const std::string& reason) {
    Require(decoded.baseAddress == expected.baseAddress && decoded.width == expected.width && decoded.height == expected.height, reason + " changed texture storage");
    Require(decoded.depthOrLastArray == expected.depthOrLastArray && decoded.baseArray == expected.baseArray && decoded.mipCount == expected.mipCount && decoded.baseLevel == expected.baseLevel && decoded.lastLevel == expected.lastLevel && decoded.minLod == expected.minLod, reason + " changed texture subresources");
    Require(decoded.tileMode == expected.tileMode && decoded.dimension == expected.dimension && decoded.viewDimension == expected.viewDimension && decoded.format == expected.format, reason + " changed texture format or layout");
    Require(decoded.dstSelX == expected.dstSelX && decoded.dstSelY == expected.dstSelY && decoded.dstSelZ == expected.dstSelZ && decoded.dstSelW == expected.dstSelW, reason + " changed texture channel selectors");
}

template<typename TMutate>
void requireIgnored(const Fields& base, TMutate mutate, const std::string& reason) {
    Fields fields = base;
    mutate(fields);
    requireSameTexture(DecodeTextureResource(pack(fields)), DecodeTextureResource(pack(base)), reason);
}

void requireExtent(const Fields& fields, const Fields& surface, const std::string& reason) {
    const auto decoded = DecodeTextureResource(pack(surface));
    const auto expected = ComputeSurfaceSize(ComputeMipLayout(decoded.tileMode, decoded.format, decoded.width, decoded.height, decoded.mipCount), FullArrayLayers(decoded));
    std::uint64_t base = 0;
    std::uint64_t bytes = 0;
    Require(DecodeTextureExtent(pack(fields), base, bytes), reason + " has no write extent");
    Require(base == decoded.baseAddress && bytes == expected, reason + " decoded a write extent other than its surface");
}

template<typename TMutate>
void requireSameExtent(const Fields& base, TMutate mutate, const std::string& reason) {
    Fields fields = base;
    mutate(fields);
    requireExtent(fields, base, reason);
}

void requireNoExtent(std::span<const std::uint32_t> words, const std::string& reason) {
    std::uint64_t base = 1;
    std::uint64_t bytes = 2;
    Require(!DecodeTextureExtent(words, base, bytes), reason + " decoded a write extent");
    Require(base == 1 && bytes == 2, reason + " changed the extent outputs without decoding one");
}

void extentTests(const Fields& plain) {
    requireExtent(plain, plain, "a linear 2D texture");
    Fields oneD = plain;
    oneD.typeRaw = 8;
    oneD.width = 300;
    oneD.height = 1;
    requireExtent(oneD, oneD, "a 1D texture");
    Fields array = plain;
    array.typeRaw = 13;
    array.tileModeRaw = 0x05;
    array.depth = 3;
    array.baseArray = 1;
    requireExtent(array, array, "a tiled 2D array texture");
    Fields cube = plain;
    cube.typeRaw = 11;
    cube.width = 64;
    cube.height = 64;
    cube.depth = 11;
    cube.tileModeRaw = 0x09;
    cube.maxMip = 6;
    cube.lastLevel = 6;
    requireExtent(cube, cube, "a mip-mapped cube array texture");
    Fields volume = plain;
    volume.typeRaw = 10;
    volume.width = 32;
    volume.height = 32;
    volume.depth = 31;
    requireExtent(volume, volume, "a linear 3D texture");
    Fields target = plain;
    target.width = 2560;
    target.height = 1440;
    target.tileModeRaw = 0x1b;
    requireExtent(target, target, "a render target texture");
    Fields compressed = plain;
    compressed.format = 169;
    compressed.width = 256;
    compressed.height = 128;
    compressed.tileModeRaw = 0x09;
    compressed.maxMip = 8;
    compressed.lastLevel = 8;
    requireExtent(compressed, compressed, "a block-compressed texture");

    Fields streamed = plain;
    streamed.width = 8192;
    streamed.height = 8192;
    streamed.tileModeRaw = 0x09;
    streamed.maxMip = 13;
    streamed.lastLevel = 13;
    requireSameExtent(streamed, [](Fields& f) {
        f.minLod = 0xd4d;
        f.minLodWarn = 0xcd1;
        f.mipStatsCntEn = true;
        f.mipStatsCntId = 0x2a;
    }, "a streamed texture with mip statistics counters");
    requireSameExtent(streamed, [](Fields& f) {
        f.mipStatsCntEn = true;
        f.mipStatsCntId = 7;
        f.prtDefColor = true;
        f.maxUncompBlkSize = 1;
        f.maxCompBlkSize = 2;
        f.metaPipeAligned = true;
        f.writeCompress = true;
        f.metaCompress = true;
        f.dccAlphaPos = true;
        f.dccColorTransf = true;
        f.metaAddr = 0x123456789aull;
        f.bcSwizzle = 3;
        f.perfMod = 5;
    }, "a texture with mip statistics and DCC metadata fields");

    requireSameExtent(plain, [](Fields& f) { f.cornerSample = true; }, "a texture with corner sampling");
    requireSameExtent(plain, [](Fields& f) { f.arrayPitch = 1; }, "a texture with a nonzero array pitch");
    requireSameExtent(plain, [](Fields& f) { f.msaaDepth = true; }, "a texture with the MSAA depth bit");
    requireSameExtent(plain, [](Fields& f) { f.dstSelX = 2; f.dstSelW = 3; }, "a texture with invalid destination selectors");
    requireSameExtent(plain, [](Fields& f) { f.depth = 1; f.baseArray = 1; }, "a 2D texture with a depth and base array");
    Fields twoMips = plain;
    twoMips.maxMip = 1;
    twoMips.lastLevel = 1;
    requireSameExtent(twoMips, [](Fields& f) { f.baseLevel = 2; }, "a texture whose base mip level is past its last level");
    requireSameExtent(twoMips, [](Fields& f) { f.lastLevel = 2; }, "a texture whose last mip level is past the surface");
    Fields cubeNotSquare = cube;
    cubeNotSquare.height = 32;
    Fields cubeSurface = cubeNotSquare;
    cubeSurface.typeRaw = 13;
    requireExtent(cubeNotSquare, cubeSurface, "a cube texture that is not square");

    Fields unsupportedTile = plain;
    unsupportedTile.tileModeRaw = 0x02;
    requireNoExtent(pack(unsupportedTile), "a texture with an unsupported tile mode");
    Fields unsupportedType = plain;
    unsupportedType.typeRaw = 0;
    requireNoExtent(pack(unsupportedType), "a texture with an unsupported image type");
    Fields nullBase = plain;
    nullBase.base40 = 0;
    requireNoExtent(pack(nullBase), "a texture with a null base address");
    Fields unsupportedFormat = plain;
    unsupportedFormat.format = 2;
    requireNoExtent(pack(unsupportedFormat), "a texture with an unsupported format");
    Fields compressedTarget = compressed;
    compressedTarget.tileModeRaw = 0x1b;
    requireNoExtent(pack(compressedTarget), "a block-compressed texture in render target tiling");
    const std::array<std::uint32_t, 4> shortWords{0x123456u, 56u << 20u, 0u, 0x90000facu};
    requireNoExtent(shortWords, "a texture descriptor shorter than 8 dwords");
}

}

void RunGuestTextureResourceTests() {
    Fields base;
    auto result = DecodeTextureResource(pack(base));
    Require(result.baseAddress == (base.base40 << 8u), "decoded base address changed");
    Require(result.width == 16 && result.height == 16, "decoded width or height changed");
    Require(result.depthOrLastArray == 0 && result.baseArray == 0, "decoded depth or base array changed");
    Require(result.mipCount == 1 && result.baseLevel == 0, "decoded mip count or base level changed");
    Require(result.tileMode == TextureTileMode::kLinear, "decoded tile mode changed");
    Require(result.dimension == TextureDimension::k2D, "decoded dimension changed");
    Require(result.format == 56, "decoded format changed");
    Require(result.dstSelX == 4 && result.dstSelY == 5 && result.dstSelZ == 6 && result.dstSelW == 7, "decoded destination selectors changed");
    Require(result.minLod == 0.0f, "a descriptor without a minimum LOD decoded one");

    Fields wide = base;
    wide.width = 8192;
    wide.height = 4096;
    result = DecodeTextureResource(pack(wide));
    Require(result.width == 8192 && result.height == 4096, "wide texture dimensions were split across dwords incorrectly");

    Fields tileModes = base;
    tileModes.tileModeRaw = 0x01;
    Require(DecodeTextureResource(pack(tileModes)).tileMode == TextureTileMode::kStandard256B, "tile mode 0x01 must decode to standard 256B");
    tileModes.tileModeRaw = 0x05;
    Require(DecodeTextureResource(pack(tileModes)).tileMode == TextureTileMode::kStandard4KB, "tile mode 0x05 must decode to standard 4KB");
    tileModes.tileModeRaw = 0x09;
    Require(DecodeTextureResource(pack(tileModes)).tileMode == TextureTileMode::kStandard64KB, "tile mode 0x09 must decode to standard 64KB");
    tileModes.tileModeRaw = 0x1b;
    Require(DecodeTextureResource(pack(tileModes)).tileMode == TextureTileMode::RenderTarget64KB, "tile mode 0x1b must decode to render target 64KB");
    tileModes.tileModeRaw = 0x02;
    rejectFields(tileModes, "unsupported tile mode");

    Fields oneD = base;
    oneD.typeRaw = 8;
    oneD.height = 1;
    result = DecodeTextureResource(pack(oneD));
    Require(result.dimension == TextureDimension::k1D, "1D descriptor did not decode to 1D dimension");
    oneD.height = 2;
    rejectFields(oneD, "nonzero height, depth or base array");

    Fields twoD = base;
    twoD.depth = 1;
    rejectFields(twoD, "nonzero depth or base array");
    twoD = base;
    twoD.baseArray = 1;
    rejectFields(twoD, "nonzero depth or base array");

    Fields array = base;
    array.typeRaw = 13;
    array.depth = 3;
    array.baseArray = 1;
    result = DecodeTextureResource(pack(array));
    Require(result.dimension == TextureDimension::k2DArray && result.depthOrLastArray == 3 && result.baseArray == 1, "2D array descriptor decoded incorrectly");
    array.baseArray = 5;
    rejectFields(array, "base array past its last array slice");

    Fields cube = base;
    cube.typeRaw = 11;
    cube.width = 32;
    cube.height = 32;
    cube.depth = 5;
    cube.baseArray = 0;
    result = DecodeTextureResource(pack(cube));
    Require(result.dimension == TextureDimension::kCube, "cube descriptor did not decode to cube dimension");
    Fields cubeNotSquare = cube;
    cubeNotSquare.height = 16;
    rejectFields(cubeNotSquare, "not square");
    Fields cubeBadArray = cube;
    cubeBadArray.baseArray = 6;
    rejectFields(cubeBadArray, "base array past its last array slice");
    Fields cubeNotMultiple = cube;
    cubeNotMultiple.depth = 4;
    rejectFields(cubeNotMultiple, "multiple of 6");

    Fields badType = base;
    badType.typeRaw = 0;
    rejectFields(badType, "unsupported image type");

    Fields badSel = base;
    badSel.dstSelX = 2;
    rejectFields(badSel, "invalid destination channel selector");
    badSel = base;
    badSel.dstSelW = 3;
    rejectFields(badSel, "invalid destination channel selector");

    Fields zeroAddress = base;
    zeroAddress.base40 = 0;
    rejectFields(zeroAddress, "null base address");

    Fields badFormat = base;
    badFormat.format = 2;
    rejectFields(badFormat, "unsupported guest texture format 2");

    Fields clampedLod = base;
    clampedLod.minLod = 0xd4d;
    Require(DecodeTextureResource(pack(clampedLod)).minLod == static_cast<float>(0xd4d) / 256.0f, "the minimum LOD was not decoded as 4.8 fixed point");
    clampedLod.minLod = 0x180;
    Require(DecodeTextureResource(pack(clampedLod)).minLod == 1.5f, "a fractional minimum LOD was not decoded");
    requireIgnored(base, [](Fields& f) { f.minLodWarn = 0xcd1; }, "a minimum LOD warning threshold");

    Fields streamed = base;
    streamed.width = 8192;
    streamed.height = 8192;
    streamed.tileModeRaw = 0x09;
    streamed.maxMip = 13;
    streamed.lastLevel = 13;
    streamed.minLod = 0xd4d;
    streamed.minLodWarn = 0xcd1;
    streamed.mipStatsCntEn = true;
    streamed.mipStatsCntId = 0x2a;
    const auto streamedTexture = DecodeTextureResource(pack(streamed));
    Require(streamedTexture.minLod == static_cast<float>(0xd4d) / 256.0f && streamedTexture.mipCount == 14 && streamedTexture.lastLevel == 13, "a streamed texture with mip statistics counters decoded incorrectly");

    const auto unmodulated = DecodeTextureResource(pack(base));
    for (std::uint32_t perfMod = 0; perfMod < 8; ++perfMod) {
        Fields modulated = base;
        modulated.perfMod = perfMod;
        const auto decoded = DecodeTextureResource(pack(modulated));
        Require(decoded.baseAddress == unmodulated.baseAddress && decoded.width == unmodulated.width && decoded.height == unmodulated.height, "performance modulation changed texture storage");
        Require(decoded.depthOrLastArray == unmodulated.depthOrLastArray && decoded.baseArray == unmodulated.baseArray && decoded.mipCount == unmodulated.mipCount && decoded.baseLevel == unmodulated.baseLevel, "performance modulation changed texture subresources");
        Require(decoded.tileMode == unmodulated.tileMode && decoded.dimension == unmodulated.dimension && decoded.format == unmodulated.format, "performance modulation changed texture format or layout");
        Require(decoded.dstSelX == unmodulated.dstSelX && decoded.dstSelY == unmodulated.dstSelY && decoded.dstSelZ == unmodulated.dstSelZ && decoded.dstSelW == unmodulated.dstSelW, "performance modulation changed texture channel selectors");
        modulated.cornerSample = true;
        rejectFields(modulated, "corner sampling");
    }

    requireIgnored(base, [](Fields& f) { f.mipStatsCntEn = true; }, "mip statistics counting");
    requireIgnored(base, [](Fields& f) { f.mipStatsCntId = 0xff; }, "a mip statistics counter id");
    requireIgnored(base, [](Fields& f) { f.prtDefColor = true; }, "a partially resident default color");
    requireIgnored(base, [](Fields& f) { f.maxUncompBlkSize = 3; }, "a DCC maximum uncompressed block size");
    requireIgnored(base, [](Fields& f) { f.maxCompBlkSize = 3; }, "a DCC maximum compressed block size");
    requireIgnored(base, [](Fields& f) { f.metaPipeAligned = true; }, "pipe-aligned metadata");
    requireIgnored(base, [](Fields& f) { f.writeCompress = true; }, "compressed writes");
    requireIgnored(base, [](Fields& f) { f.metaCompress = true; }, "metadata compression");
    requireIgnored(base, [](Fields& f) { f.dccAlphaPos = true; }, "the DCC alpha position");
    requireIgnored(base, [](Fields& f) { f.dccColorTransf = true; }, "the DCC color transform");
    requireIgnored(base, [](Fields& f) { f.metaAddr = 0xffffffffffull; }, "a metadata address");
    requireIgnored(base, [](Fields& f) { f.bcSwizzle = 5; }, "a border color swizzle");
    requireIgnored(base, [](Fields& f) {
        f.mipStatsCntEn = true;
        f.mipStatsCntId = 7;
        f.prtDefColor = true;
        f.minLodWarn = 0xfff;
        f.maxUncompBlkSize = 1;
        f.maxCompBlkSize = 2;
        f.metaPipeAligned = true;
        f.writeCompress = true;
        f.metaCompress = true;
        f.dccAlphaPos = true;
        f.dccColorTransf = true;
        f.metaAddr = 0x123456789aull;
        f.bcSwizzle = 3;
    }, "every ignored field at once");

    Fields badCorner = base;
    badCorner.cornerSample = true;
    rejectFields(badCorner, "corner sampling");

    Fields badPitch = base;
    badPitch.arrayPitch = 1;
    rejectFields(badPitch, "nonzero array pitch");

    Fields badMsaa = base;
    badMsaa.msaaDepth = true;
    rejectFields(badMsaa, "MSAA");

    Fields badLevels = base;
    badLevels.baseLevel = 2;
    badLevels.lastLevel = 1;
    badLevels.maxMip = 1;
    rejectFields(badLevels, "base mip level past its last mip level");

    Fields partialMips = base;
    partialMips.lastLevel = 1;
    partialMips.maxMip = 2;
    Require(DecodeTextureResource(pack(partialMips)).lastLevel == 1, "a view of the upper mip levels was not decoded");

    Fields badMaxMip = base;
    badMaxMip.lastLevel = 2;
    badMaxMip.maxMip = 1;
    rejectFields(badMaxMip, "mip levels past the surface");

    std::array<std::uint32_t, 4> shortWords{};
    reject([&] { DecodeTextureResource(shortWords); }, "8 dwords");

    extentTests(base);

    Require(MatchesGuestDimension(Shape::Image1D, TextureDimension::k1D), "1D shape must match 1D dimension");
    Require(!MatchesGuestDimension(Shape::Image1D, TextureDimension::k2D), "1D shape must not match 2D dimension");
    Require(MatchesGuestDimension(Shape::Image2D, TextureDimension::k2D), "2D shape must match 2D dimension");
    Require(!MatchesGuestDimension(Shape::Image2D, TextureDimension::k2DArray), "2D shape must not match 2D array dimension");
    Require(MatchesGuestDimension(Shape::Image2DArray, TextureDimension::k2DArray), "2D array shape must match 2D array dimension");
    Require(!MatchesGuestDimension(Shape::Image2DArray, TextureDimension::kCube), "2D array shape must not match cube dimension");
    Require(MatchesGuestDimension(Shape::ImageCube, TextureDimension::kCube), "cube shape must match cube dimension");
    Require(!MatchesGuestDimension(Shape::ImageCube, TextureDimension::k1D), "cube shape must not match 1D dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::k1D), "3D shape must never match a guest dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::k2D), "3D shape must never match a guest dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::k2DArray), "3D shape must never match a guest dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::kCube), "3D shape must never match a guest dimension");
}

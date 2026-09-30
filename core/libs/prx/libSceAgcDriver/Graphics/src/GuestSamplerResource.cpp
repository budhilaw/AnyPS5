#include "prx/libSceAgcDriver/Graphics/include/GuestSamplerResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

VkCompareOp toVkCompareOp(std::uint32_t raw) {
    const std::array compareOps{VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS};
    return compareOps.at(raw);
}

VkFilter toVkFilter(std::uint32_t raw) {
    switch (raw) {
        case 0: case 2: return VK_FILTER_NEAREST;
        case 1: case 3: return VK_FILTER_LINEAR;
        default: throw std::runtime_error("AGC graphics: guest sampler descriptor uses an unknown filter " + std::to_string(raw));
    }
}

bool isAnisoFilter(std::uint32_t raw) {
    return raw == 2 || raw == 3;
}

VkSamplerAddressMode toVkAddressMode(std::uint32_t raw) {
    switch (raw) {
        case 0: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case 1: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case 2: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case 3: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        case 4: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case 5: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        case 6: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case 7: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default: throw std::runtime_error("AGC graphics: guest sampler descriptor uses an unknown clamp mode " + std::to_string(raw));
    }
}

float toSignedLodBias(std::uint32_t raw) {
    const auto extended = static_cast<std::int32_t>((raw ^ 0x2000u) - 0x2000u);
    return static_cast<float>(extended) / 256.0f;
}

}

GuestSamplerResource DecodeSamplerResource(std::span<const std::uint32_t> words) {
    Require(words.size() == 4, "guest sampler descriptor must contain 4 dwords");

    const auto clampX = (words[0] >> 0u) & 0x7u;
    const auto clampY = (words[0] >> 3u) & 0x7u;
    const auto clampZ = (words[0] >> 6u) & 0x7u;
    const auto maxAnisoRatio = (words[0] >> 9u) & 0x7u;
    const auto depthCompareFunc = (words[0] >> 12u) & 0x7u;
    const auto forceUnormCoords = ((words[0] >> 15u) & 0x1u) != 0;
    const auto anisoThreshold = (words[0] >> 16u) & 0x7u;
    const auto forceSrgb = ((words[0] >> 20u) & 0x1u) != 0;
    const auto anisoBias = (words[0] >> 21u) & 0x3fu;
    const auto truncCoord = ((words[0] >> 27u) & 0x1u) != 0;
    const auto disableCubeWrap = ((words[0] >> 28u) & 0x1u) != 0;
    const auto filterMode = (words[0] >> 29u) & 0x3u;
    const auto disableDegamma = ((words[0] >> 31u) & 0x1u) != 0;

    const auto minLodRaw = (words[1] >> 0u) & 0xfffu;
    const auto maxLodRaw = (words[1] >> 12u) & 0xfffu;
    const auto perfMip = (words[1] >> 24u) & 0xfu;
    const auto perfZ = (words[1] >> 28u) & 0xfu;

    const auto lodBiasRaw = (words[2] >> 0u) & 0x3fffu;
    const auto lodBiasSec = (words[2] >> 14u) & 0x3fu;
    const auto xyMagFilter = (words[2] >> 20u) & 0x3u;
    const auto xyMinFilter = (words[2] >> 22u) & 0x3u;
    const auto mipFilter = (words[2] >> 26u) & 0x3u;
    const auto pointPreclamp = ((words[2] >> 28u) & 0x1u) != 0;
    const auto anisoOverride = ((words[2] >> 29u) & 0x1u) != 0;
    const auto blendZeroPrt = ((words[2] >> 30u) & 0x1u) != 0;

    const auto borderColorPtr = words[3] & 0xfffu;
    const auto borderColorType = (words[3] >> 30u) & 0x3u;

    Require(!forceUnormCoords, "guest sampler descriptor uses unnormalized coordinates which are not implemented");
    Require(!forceSrgb, "guest sampler descriptor forces sRGB decoding which is not implemented");
    Require(!truncCoord, "guest sampler descriptor uses coordinate truncation which is not implemented");
    Require(filterMode == 0, "guest sampler descriptor uses a reduction filter mode which is not implemented");
    Require(!disableDegamma, "guest sampler descriptor disables degamma which is not implemented");
    Require(mipFilter <= 2u, "guest sampler descriptor uses an unknown mip filter " + std::to_string(mipFilter));

    const std::array<std::pair<const char*, std::uint32_t>, 9> ignoredFields{{
        {"a mip filtering performance setting", perfMip},
        {"a depth filtering performance setting", perfZ},
        {"an anisotropy threshold", anisoThreshold},
        {"an anisotropy bias", anisoBias},
        {"an anisotropy override", anisoOverride},
        {"a secondary LOD bias", lodBiasSec},
        {"point preclamping", pointPreclamp},
        {"PRT blend-zero", blendZeroPrt},
        {"non-seamless cube filtering", disableCubeWrap}
    }};
    static std::array<std::once_flag, std::tuple_size_v<decltype(ignoredFields)>> ignoredOnce;
    for (std::size_t field = 0; field < ignoredFields.size(); ++field) {
        if (ignoredFields[field].second != 0) std::call_once(ignoredOnce[field], [&] { APS5_LOG_OUT("guest sampler descriptors use %s (0x%x); the driver ignores it", ignoredFields[field].first, ignoredFields[field].second); });
    }

    const auto aniso = isAnisoFilter(xyMagFilter) || isAnisoFilter(xyMinFilter);
    auto anisoRatio = 1.0f;
    if (aniso) {
        Require(maxAnisoRatio <= 4u, "guest sampler descriptor uses an unknown anisotropy ratio " + std::to_string(maxAnisoRatio));
        const std::array ratios{1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
        anisoRatio = ratios[maxAnisoRatio];
    }

    auto minLod = 0.0f;
    auto maxLod = 0.0f;
    if (mipFilter != 0u) {
        Require(minLodRaw <= maxLodRaw, "guest sampler descriptor has a minimum LOD past its maximum LOD");
        minLod = static_cast<float>(minLodRaw) / 256.0f;
        maxLod = static_cast<float>(maxLodRaw) / 256.0f;
    }

    VkBorderColor border;
    switch (borderColorType) {
        case 0: border = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK; break;
        case 1: border = VK_BORDER_COLOR_INT_OPAQUE_BLACK; break;
        case 2: border = VK_BORDER_COLOR_INT_OPAQUE_WHITE; break;
        default: {
            static std::once_flag once;
            std::call_once(once, [&] { APS5_LOG_OUT("guest sampler descriptors use border color table entry %u; their borders are transparent black", borderColorPtr); });
            border = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        }
    }

    GuestSamplerResource result{};
    result.magFilter = toVkFilter(xyMagFilter);
    result.minFilter = toVkFilter(xyMinFilter);
    result.mipmapMode = mipFilter == 2u ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    result.addressModeU = toVkAddressMode(clampX);
    result.addressModeV = toVkAddressMode(clampY);
    result.addressModeW = toVkAddressMode(clampZ);
    result.anisotropyEnable = aniso;
    result.maxAnisotropy = anisoRatio;
    result.minLod = minLod;
    result.maxLod = maxLod;
    result.lodBias = toSignedLodBias(lodBiasRaw);
    result.borderColor = border;
    result.compareOp = toVkCompareOp(depthCompareFunc);
    return result;
}

GuestSamplerResource FallbackSamplerResource(std::span<const std::uint32_t> words) {
    Require(words.size() == 4, "guest sampler descriptor must contain 4 dwords");
    GuestSamplerResource result{};
    result.magFilter = VK_FILTER_LINEAR;
    result.minFilter = VK_FILTER_LINEAR;
    result.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    result.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    result.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    result.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    result.anisotropyEnable = false;
    result.maxAnisotropy = 1.0f;
    result.minLod = 0.0f;
    result.maxLod = VK_LOD_CLAMP_NONE;
    result.lodBias = 0.0f;
    result.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    result.compareOp = toVkCompareOp((words[0] >> 12u) & 0x7u);
    return result;
}

}

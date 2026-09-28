#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_DESCRIPTORBINDING_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_DESCRIPTORBINDING_HPP

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace ShaderRecompiler {

// Sampled image classes (float, uint, sint, comparison) have 8 dimension slots each (1D, 1D
// array, 2D, 2D array, MSAA, MSAA array, 3D, cube); storage classes (float, uint, atomic) 5.
inline constexpr std::uint32_t FirstImageBinding = 1u;
inline constexpr std::uint32_t SampledImageDimensionSlots = 8u;
inline constexpr std::uint32_t StorageImageDimensionSlots = 5u;
inline constexpr std::uint32_t FirstComparisonImageBinding = FirstImageBinding + 3u * SampledImageDimensionSlots;
inline constexpr std::uint32_t FirstStorageImageBinding = FirstComparisonImageBinding + SampledImageDimensionSlots;
inline constexpr std::uint32_t ImageBindingCount = 4u * SampledImageDimensionSlots + 3u * StorageImageDimensionSlots;

enum class DescriptorBindingKind : std::uint32_t {
    Buffers = 0u,
    Samplers = FirstImageBinding + ImageBindingCount,
    Gds,
    BdaPagetable,
    FaultBuffer,
    FlattenedSrt,
    ShaderData,
    Count,
};

struct PushData {
    static constexpr std::uint32_t DwordCount = 32;
    static constexpr std::uint32_t MeshDrawDwordCount = 6;
    static constexpr std::uint32_t NoStart = std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint32_t, DwordCount> dwords {};

    [[nodiscard]] static bool CanFit(std::uint32_t start, std::uint32_t size) {
        return size != 0u && start <= DwordCount && size <= DwordCount - start;
    }
    [[nodiscard]] static std::uint32_t StartFor(std::uint32_t cursor, std::uint32_t size) {
        return CanFit(cursor, size) ? cursor : NoStart;
    }
};

struct IrDescriptorBinding {
    DescriptorBindingKind kind = DescriptorBindingKind::Buffers;
    std::vector<std::uint32_t> resources;

    bool operator==(const IrDescriptorBinding& other) const = default;
};

struct IrBindingLayout {
    std::uint32_t pushDataStartDword = PushData::NoStart;
    std::uint32_t memoryOffsetDword = 0;
    std::uint32_t memoryOffsetCount = 0;
    std::vector<std::uint32_t> userDataRegisters;
    std::vector<IrDescriptorBinding> descriptors;

    [[nodiscard]] std::uint32_t ShaderDataDwords() const {
        return memoryOffsetDword + (memoryOffsetCount + 3u) / 4u;
    }
    [[nodiscard]] bool UsesPushData() const {
        return pushDataStartDword != PushData::NoStart;
    }
    void AdvancePushData(std::uint32_t& cursor) const {
        if (UsesPushData()) {
            cursor = pushDataStartDword + ShaderDataDwords();
        }
    }

    bool operator==(const IrBindingLayout& other) const = default;
};

}

#endif

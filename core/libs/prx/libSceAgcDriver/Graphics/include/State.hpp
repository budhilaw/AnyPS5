#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_STATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_STATE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include <array>
#include <vector>
#include <cstddef>
#include <cstdint>
#include "Recompiler.hpp"

namespace AgcDriver::Graphics {

enum class ShaderPath {
    Vertex,
    Geometry,
    Tessellation,
    TessellationGeometry
};

struct ShaderStages {
    ShaderPath path;
    std::uint32_t registerValue;
    std::uint32_t vertexWaveSize;
    std::uint32_t fragmentWaveSize;
    std::optional<ShaderRecompiler::MeshConfiguration> mesh;
    std::optional<ShaderRecompiler::TessellationConfiguration> tessellation;
};

struct ColorTarget {
    std::uint64_t address;
    VkExtent2D extent;
    VkFormat format;
    std::size_t bytes;
    std::uint8_t componentMapping;
    ColorTileMode tileMode = ColorTileMode::Linear;
    std::uint32_t bytesPerPixel = 4;
    bool gpuOnly = false;
};

struct DepthTarget {
    std::uint64_t address;
    VkExtent2D extent;
    VkFormat format;
    bool stencil;
    std::uint64_t stencilAddress = 0;
};

struct DepthState {
    bool test = false;
    bool write = false;
    VkCompareOp compare = VK_COMPARE_OP_ALWAYS;
    bool stencilTest = false;
    VkStencilOpState front{};
    VkStencilOpState back{};
    bool clearDepth = false;
    float depthClear = 1.0f;
    bool clearStencil = false;
    std::uint32_t stencilClear = 0;
};

struct State {
    ShaderStages stages;
    ColorTarget color;
    bool hasColorTarget;
    std::vector<ColorTarget> extraColors;
    std::vector<VkPipelineColorBlendAttachmentState> extraBlends;
    bool hasDepthTarget = false;
    DepthTarget depth{};
    DepthState depthState{};
    bool rectList = false;
    VkExtent2D renderExtent;
    VkPrimitiveTopology topology;
    VkViewport viewport;
    bool negativeOneToOne;
    VkRect2D scissor;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    bool depthClamp = false;
    bool depthBias = false;
    float depthBiasConstant = 0.0f;
    float depthBiasSlope = 0.0f;
    VkPipelineColorBlendAttachmentState blend;
    std::array<float, 4> blendConstants;
    bool dualSourceBlend = false;
};

ShaderStages DecodeShaderStages(const QueueState& queue);
State DecodeState(const QueueState& queue);

}

#endif

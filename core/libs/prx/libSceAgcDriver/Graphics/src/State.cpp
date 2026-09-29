#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <set>
#include <mutex>
#include <bit>
#include <cmath>
#include <limits>
#include <sstream>
#include <cstdlib>

namespace AgcDriver::Graphics {

namespace {

struct DecodedColorFormat { VkFormat format; std::uint32_t bytesPerPixel; };

// CB_COLOR_INFO format (bits 2-6), number type (bits 8-10) and component swap (bits 11-12) to a
// Vulkan color attachment format. Number types: 0 UNORM, 1 SNORM, 6 SRGB, 7 FLOAT; integer
// targets need integer shader exports the recompiler does not produce yet. Swap 1 (ALT) reverses
// the first three channels; only the 8-bit RGBA formats have such a Vulkan counterpart.
DecodedColorFormat DecodeColorFormat(std::uint32_t format, std::uint32_t number, std::uint32_t swap) {
    const bool unorm = number == 0, snorm = number == 1, srgb = number == 6, real = number == 7;
    if (swap > 1) return {VK_FORMAT_UNDEFINED, 0};
    if (swap == 1 && format != 10 && format != 9) return {VK_FORMAT_UNDEFINED, 0};
    switch (format) {
        case 1: return {unorm ? VK_FORMAT_R8_UNORM : snorm ? VK_FORMAT_R8_SNORM : srgb ? VK_FORMAT_R8_SRGB : VK_FORMAT_UNDEFINED, 1};
        case 2: return {unorm ? VK_FORMAT_R16_UNORM : snorm ? VK_FORMAT_R16_SNORM : real ? VK_FORMAT_R16_SFLOAT : VK_FORMAT_UNDEFINED, 2};
        case 3: return {unorm ? VK_FORMAT_R8G8_UNORM : snorm ? VK_FORMAT_R8G8_SNORM : srgb ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_UNDEFINED, 2};
        case 4: return {real ? VK_FORMAT_R32_SFLOAT : VK_FORMAT_UNDEFINED, 4};
        case 5: return {unorm ? VK_FORMAT_R16G16_UNORM : snorm ? VK_FORMAT_R16G16_SNORM : real ? VK_FORMAT_R16G16_SFLOAT : VK_FORMAT_UNDEFINED, 4};
        case 6: return {real ? VK_FORMAT_B10G11R11_UFLOAT_PACK32 : VK_FORMAT_UNDEFINED, 4};
        case 9: return {unorm ? (swap == 0 ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_A2R10G10B10_UNORM_PACK32) : VK_FORMAT_UNDEFINED, 4};
        case 10:
            if (swap == 0) return {unorm ? VK_FORMAT_R8G8B8A8_UNORM : snorm ? VK_FORMAT_R8G8B8A8_SNORM : srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_UNDEFINED, 4};
            return {unorm ? VK_FORMAT_B8G8R8A8_UNORM : snorm ? VK_FORMAT_B8G8R8A8_SNORM : srgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_UNDEFINED, 4};
        case 11: return {real ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_UNDEFINED, 8};
        case 12: return {unorm ? VK_FORMAT_R16G16B16A16_UNORM : snorm ? VK_FORMAT_R16G16B16A16_SNORM : real ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_UNDEFINED, 8};
        case 14: return {real ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_UNDEFINED, 16};
        case 16: return {unorm ? VK_FORMAT_R5G6B5_UNORM_PACK16 : VK_FORMAT_UNDEFINED, 2};
        case 17: return {unorm ? VK_FORMAT_A1R5G5B5_UNORM_PACK16 : VK_FORMAT_UNDEFINED, 2};
        case 18: return {unorm ? VK_FORMAT_R5G5B5A1_UNORM_PACK16 : VK_FORMAT_UNDEFINED, 2};
        case 19: return {unorm ? VK_FORMAT_R4G4B4A4_UNORM_PACK16 : VK_FORMAT_UNDEFINED, 2};
        default: return {VK_FORMAT_UNDEFINED, 0};
    }
}

}

namespace {

std::uint32_t read(const Registers& registers, std::uint32_t offset, const char* bank = "context") {
    const auto it = registers.find(offset);
    if (it == registers.end()) {
        std::ostringstream message;
        message << "missing register in " << bank << " bank at DWORD 0x" << std::hex << offset << " (" << std::dec << offset << ')';
        throw std::runtime_error("AGC graphics: " + message.str());
    }
    return it->second;
}

float readFloat(const Registers& registers, std::uint32_t offset) {
    const auto value = std::bit_cast<float>(read(registers, offset));
    if (!std::isfinite(value)) Require(false, "non-finite register at DWORD " + std::to_string(offset));
    return value;
}

std::uint32_t readOr(const Registers& registers, std::uint32_t offset, std::uint32_t fallback) {
    const auto it = registers.find(offset);
    return it == registers.end() ? fallback : it->second;
}

VkStencilOp stencilOp(std::uint32_t value) {
    switch (value) {
        case 0: return VK_STENCIL_OP_KEEP;
        case 1: return VK_STENCIL_OP_ZERO;
        case 2: return VK_STENCIL_OP_REPLACE; // ONES: replace with 0xff is not representable; titles pair it with an all-ones reference
        case 3: return VK_STENCIL_OP_REPLACE;
        case 4: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case 5: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case 6: return VK_STENCIL_OP_INVERT;
        case 7: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case 8: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default: throw std::runtime_error("AGC graphics: unsupported stencil operation " + std::to_string(value));
    }
}

void zero(const Registers& registers, std::uint32_t offset, std::uint32_t mask, const char* name, const char* bank = "context") {
    if ((read(registers, offset, bank) & mask) != 0) Require(false, std::string(name) + " is unsupported");
}

VkBlendFactor blendFactor(std::uint32_t value) {
    switch (value) {
        case 0: return VK_BLEND_FACTOR_ZERO;
        case 1: return VK_BLEND_FACTOR_ONE;
        case 2: return VK_BLEND_FACTOR_SRC_COLOR;
        case 3: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case 4: return VK_BLEND_FACTOR_SRC_ALPHA;
        case 5: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case 6: return VK_BLEND_FACTOR_DST_ALPHA;
        case 7: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case 8: return VK_BLEND_FACTOR_DST_COLOR;
        case 9: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case 10: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case 13: return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case 14: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case 15: return VK_BLEND_FACTOR_SRC1_COLOR;
        case 16: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        case 17: return VK_BLEND_FACTOR_SRC1_ALPHA;
        case 18: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
        case 19: return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case 20: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        default: throw std::runtime_error("AGC graphics: unsupported blend factor " + std::to_string(value));
    }
}

VkBlendOp blendOp(std::uint32_t value) {
    switch (value) {
        case 0: return VK_BLEND_OP_ADD;
        case 1: return VK_BLEND_OP_SUBTRACT;
        case 2: return VK_BLEND_OP_MIN;
        case 3: return VK_BLEND_OP_MAX;
        case 4: return VK_BLEND_OP_REVERSE_SUBTRACT;
        default: throw std::runtime_error("AGC graphics: unsupported blend operation " + std::to_string(value));
    }
}

// Clips `result` to a scissor register pair. PA_SC_WINDOW_OFFSET shifts every scissor except
// the screen one and those with WINDOW_OFFSET_DISABLE (TL bit 31) set.
void intersect(VkRect2D& result, const Registers& registers, std::uint32_t offset, bool screen, std::int32_t windowX = 0, std::int32_t windowY = 0) {
    const auto tl = read(registers, offset);
    const auto br = read(registers, offset + 1);
    if (!screen) Require((tl & 0x8000u) == 0 && (br & 0x80008000u) == 0, "scissor reserved bits are unsupported");
    const bool shifted = !screen && (tl & 0x80000000u) == 0;
    const auto shift = [&](std::uint32_t value, std::int32_t delta) { return static_cast<std::uint32_t>(std::max<std::int32_t>(0, static_cast<std::int32_t>(value) + (shifted ? delta : 0))); };
    const auto x = shift(tl & 0x7fffu, windowX);
    const auto y = shift((tl >> 16u) & 0x7fffu, windowY);
    const auto right = shift(br & 0x7fffu, windowX);
    const auto bottom = shift((br >> 16u) & 0x7fffu, windowY);
    Require(x <= right && y <= bottom, "inverted scissor rectangle");
    const auto oldRight = static_cast<std::uint32_t>(result.offset.x) + result.extent.width;
    const auto oldBottom = static_cast<std::uint32_t>(result.offset.y) + result.extent.height;
    const auto left = std::max(static_cast<std::uint32_t>(result.offset.x), x);
    const auto top = std::max(static_cast<std::uint32_t>(result.offset.y), y);
    result.offset = {static_cast<std::int32_t>(left), static_cast<std::int32_t>(top)};
    result.extent = {std::min(oldRight, right) > left ? std::min(oldRight, right) - left : 0, std::min(oldBottom, bottom) > top ? std::min(oldBottom, bottom) - top : 0};
}

}

ShaderStages DecodeShaderStages(const QueueState& queue) {
    const auto value = read(queue.context, 0x2d5);
    const auto validate = [&](bool condition, const char* reason) {
        if (condition) return;
        std::ostringstream prefix;
        prefix << "VGT_SHADER_STAGES_EN=0x" << std::hex << value << ": " << reason;
        Require(false, prefix.str());
    };
    validate((value & 0xfc000000u) == 0, "reserved stage bits are set");
    validate((value & 3u) != 3u && ((value >> 3u) & 3u) != 3u && ((value >> 6u) & 3u) != 3u, "reserved LS_EN, ES_EN or VS_EN encoding");
    const auto primitive = read(queue.userConfig, 0x242, "user-config");
    const bool tessellation = primitive == 9;
    const bool geometry = (value & 0x20u) != 0;
    validate(tessellation == ((value & 4u) != 0), "Patch topology and HS_EN disagree");
    validate(!tessellation || !geometry, "combined tessellation and geometry is unsupported by the reference path");
    const auto path = tessellation ? ShaderPath::Tessellation : geometry ? ShaderPath::Geometry : ShaderPath::Vertex;
    ShaderStages result{path, value, (value & 0x00400000u) != 0 ? 32u : 64u, (read(queue.context, 0x1b6) & 0x8000u) != 0 ? 32u : 64u, {}, {}};
    if (path == ShaderPath::Vertex) {
        validate((value & 0x2000u) != 0, "legacy vertex routing without PRIMGEN_EN is unsupported");
        validate((value & ~0x02402010u) == 0, "unsupported vertex routing, scheduling or wave-ID state");
    } else if (path == ShaderPath::Tessellation) {
        validate((value & 0x00600020u) == 0, "wave32 tessellation or geometry amplification is unsupported");
        validate((value & ~0x0007ed0du) == 0 && (value & 3u) == 1u && ((value >> 3u) & 3u) == 1u, "unsupported tessellation routing");
        const auto config = read(queue.context, 0x2d6);
        const auto parameters = read(queue.context, 0x2db);
        ShaderRecompiler::TessellationConfiguration tess{(config >> 8u) & 0x3fu, (config >> 14u) & 0x3fu, parameters & 3u, (parameters >> 2u) & 3u, (parameters >> 5u) & 3u};
        validate(tess.inputControlPoints != 0 && tess.inputControlPoints <= 32 && tess.outputControlPoints != 0 && tess.outputControlPoints <= 32, "invalid tessellation control-point counts");
        validate(tess.domain == 1 && tess.partitioning == 2 && tess.outputTopology == 2, "only triangular, fractional-odd, clockwise tessellation is supported by the reference path");
        result.tessellation = tess;
    } else {
        validate((value & ~0x0047ec30u) == 0, "unsupported geometry routing, fast launch or wave-ID state");
        const auto group = read(queue.userConfig, 0x25b, "user-config");
        const auto vertices = (group >> 9u) & 0x1ffu;
        const auto primitives = group & 0x1ffu;
        const auto maxVertices = read(queue.context, 0x1ff);
        const auto verticesPerPrimitive = read(queue.context, 0x2ce);
        validate((primitive == 1 || primitive == 2 || primitive == 4 || primitive == 6) && read(queue.context, 0x29b) == 2 && verticesPerPrimitive >= 3, "unsupported geometry input or output assembly");
        const auto inputSize = primitive == 1 ? 1u : primitive == 2 ? 2u : 3u;
        validate(vertices >= inputSize && maxVertices != 0 && maxVertices <= 256 && verticesPerPrimitive <= 256, "invalid geometry subgroup output");
        const auto inputStep = primitive == 6 ? 1u : inputSize;
        const auto groupPrimitives = std::min({primitives, (vertices - inputSize) / inputStep + 1u, maxVertices / verticesPerPrimitive});
        validate(groupPrimitives != 0, "geometry subgroup contains no primitives");
        const auto resources = read(queue.shader, 0x8b, "shader");
        validate(((read(queue.shader, 0x8a, "shader") >> 29u) & 3u) == 3 && ((resources >> 16u) & 3u) == 3, "unsupported geometry VGPR allocation");
        result.mesh = ShaderRecompiler::MeshConfiguration{primitive, groupPrimitives, (groupPrimitives - 1u) * inputStep + inputSize, maxVertices, primitives * (verticesPerPrimitive - 2u), ((maxVertices + result.vertexWaveSize - 1u) / result.vertexWaveSize) * result.vertexWaveSize, ((resources >> 19u) & 0xffu) * 128u, 0};
    }
    return result;
}

State DecodeState(const QueueState& queue) {
    const auto& cx = queue.context;
    State result{};
    result.stages = DecodeShaderStages(queue);
    const auto primitive = read(queue.userConfig, 0x242, "user-config");
    switch (primitive) {
        case 1: Require(result.stages.mesh.has_value(), "point-list vertex rendering requires point-size output support"); result.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
        case 2: result.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
        case 7:
        case 17:
            Require(result.stages.path == ShaderPath::Vertex, "rect-list requires vertex routing");
            result.rectList = true;
            result.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
            break;
        case 9: result.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
        case 4: result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
        case 5: result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; break;
        case 6: result.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
        default: throw std::runtime_error("AGC graphics: unsupported primitive type " + std::to_string(primitive));
    }
    zero(queue.userConfig, 0x24b, ~0u, "primitive restart (GE_MULTI_PRIM_IB_RESET_EN)", "user-config");
    // PA_CL_VS_OUT_CNTL: clip/cull distance enables (bits 0-15) only matter when the shader
    // exports a distance vector (VS_OUT_CCDIST0/1_VEC_ENA, bits 22-23); titles leave stale enables.
    auto outControl = readOr(cx, 0x207, 0);
    if ((outControl & 0x00c00000u) == 0) outControl &= ~0x0000ffffu;
    if (outControl != 0) {
        std::ostringstream message;
        message << "AGC graphics: clip distances, layer, viewport or auxiliary vertex exports are unsupported: PA_CL_VS_OUT_CNTL=0x" << std::hex << outControl << " SPI_SHADER_POS_FORMAT=0x" << readOr(cx, 0x1c3, 0) << " VGT_PRIMITIVE_TYPE(0x2f0)=0x" << readOr(cx, 0x2f0, 0);
        throw std::runtime_error(message.str());
    }
    {
        // DB_DEPTH_CONTROL, DB_Z_INFO, DB_STENCIL_INFO, DB_Z_READ_BASE(_HI), DB_DEPTH_SIZE_XY,
        // DB_RENDER_CONTROL, DB_DEPTH_CLEAR, DB_STENCIL_CLEAR, DB_STENCIL_CONTROL, DB_STENCILREFMASK(_BF).
        const auto control = read(cx, 0x200);
        Require((control & 0xc0000008u) == 0, "depth bounds or conditional color writes are unsupported");
        auto& depth = result.depthState;
        depth.stencilTest = (control & 1u) != 0;
        depth.test = (control & 2u) != 0;
        depth.write = (control & 4u) != 0;
        depth.compare = static_cast<VkCompareOp>((control >> 4u) & 7u);
        const auto renderControl = readOr(cx, 0x0, 0);
        // Bits 4-6 (resummarize, compression disables) only affect HTILE metadata, which does not exist here.
        if ((renderControl & ~0x73u) != 0) {
            std::ostringstream message;
            message << "DB_RENDER_CONTROL=0x" << std::hex << renderControl << ": depth or stencil copy and decompression are unsupported";
            throw std::runtime_error("AGC graphics: " + message.str());
        }
        depth.clearDepth = (renderControl & 1u) != 0;
        depth.clearStencil = (renderControl & 2u) != 0;
        depth.depthClear = std::bit_cast<float>(readOr(cx, 0xb, 0));
        depth.stencilClear = readOr(cx, 0xa, 0) & 0xffu;
        if (depth.stencilTest) {
            const auto ops = readOr(cx, 0x10b, 0);
            const auto frontMask = readOr(cx, 0x10c, 0);
            const auto backMask = (control & 0x80u) != 0 ? readOr(cx, 0x10d, 0) : frontMask;
            const auto backOps = (control & 0x80u) != 0 ? ops >> 12u : ops;
            const auto backFunc = (control & 0x80u) != 0 ? (control >> 20u) & 7u : (control >> 8u) & 7u;
            depth.front = {stencilOp(ops & 0xfu), stencilOp((ops >> 4u) & 0xfu), stencilOp((ops >> 8u) & 0xfu), static_cast<VkCompareOp>((control >> 8u) & 7u), (frontMask >> 8u) & 0xffu, (frontMask >> 16u) & 0xffu, frontMask & 0xffu};
            depth.back = {stencilOp(backOps & 0xfu), stencilOp((backOps >> 4u) & 0xfu), stencilOp((backOps >> 8u) & 0xfu), static_cast<VkCompareOp>(backFunc), (backMask >> 8u) & 0xffu, (backMask >> 16u) & 0xffu, backMask & 0xffu};
        }
        result.hasDepthTarget = depth.test || depth.write || depth.stencilTest || depth.clearDepth || depth.clearStencil;
        if (result.hasDepthTarget && (readOr(cx, 0x10, 0) & 3u) == 0) {
            // DB_Z_INFO.FORMAT 0 means no depth surface is bound: the hardware then neither tests
            // nor writes depth, whatever DB_DEPTH_CONTROL says.
            result.hasDepthTarget = false;
            depth = DepthState{};
        }
        if (result.hasDepthTarget) {
            const auto zInfo = readOr(cx, 0x10, 0);
            const auto stencilInfo = readOr(cx, 0x11, 0);
            const auto zFormat = zInfo & 3u;
            const bool stencil = (stencilInfo & 1u) != 0;
            // Z formats: 1 = Z_16, 2 = Z_24 (kept by the console's part), 3 = Z_32_FLOAT. The host has
            // no 24-bit depth on Apple GPUs, so Z_24 renders as 32-bit float (more precision).
            Require(zFormat == 1 || zFormat == 2 || zFormat == 3, "depth target format must be Z_16, Z_24 or Z_32_FLOAT");
            Require(((zInfo >> 2u) & 3u) == 0, "multisampled depth targets are unsupported");
            Require(depth.stencilTest ? stencil : true, "stencil test without a stencil surface");
            result.depth.stencil = stencil;
            result.depth.format = stencil ? VK_FORMAT_D32_SFLOAT_S8_UINT : zFormat == 1 ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;
            const auto high = readOr(cx, 0x1a, 0);
            result.depth.address = (static_cast<std::uint64_t>(high & 0xffu) << 40u) | (static_cast<std::uint64_t>(readOr(cx, 0x12, 0)) << 8u);
            Require(result.depth.address != 0, "depth target without a Z base address");
            if (stencil) result.depth.stencilAddress = (static_cast<std::uint64_t>(readOr(cx, 0x1b, 0) & 0xffu) << 40u) | (static_cast<std::uint64_t>(readOr(cx, 0x13, 0)) << 8u);
            const auto size = readOr(cx, 0x7, 0);
            result.depth.extent = {(size & 0x3fffu) + 1u, ((size >> 16u) & 0x3fffu) + 1u};
        }
    }
    // Bits 9-10 (EXEC_ON_HIER_FAIL/NOOP) and 17 (pre-shader depth coverage) only steer the
    // hardware's early depth scheduling; results are unaffected.
    if (const auto shaderControl = readOr(cx, 0x203, 0); (shaderControl & ~0x00029e70u) != 0) {
        std::ostringstream message;
        message << "AGC graphics: depth export, shader coverage or ordered fragment execution is unsupported: DB_SHADER_CONTROL=0x" << std::hex << shaderControl;
        throw std::runtime_error(message.str());
    }
    zero(cx, 0x2dc, ~0x0001ff00u, "alpha-to-coverage");
    zero(cx, 0x2f8, ~0u, "multisampling or coverage conversion");
    // PA_SC_MODE_CNTL_0/1: MSAA and line stipple change results; the remaining bits are tile walk,
    // fence, out-of-order and multi-GPU hints that an in-order host rasterizer realizes correctly.
    zero(cx, 0x292, 0x5u, "multisampled scan conversion or line stipple");
    zero(cx, 0x293, 0u, "");
    // PA_SC_WINDOW_OFFSET: signed 16-bit x/y offsets applied after the viewport transform.
    const auto windowOffset = readOr(cx, 0x80, 0);
    const auto windowX = static_cast<std::int32_t>(static_cast<std::int16_t>(windowOffset & 0xffffu));
    const auto windowY = static_cast<std::int32_t>(static_cast<std::int16_t>(windowOffset >> 16u));
    zero(cx, 0x8d, ~0x01ff01ffu, "reserved PA_SU_HARDWARE_SCREEN_OFFSET bits");
    Require(read(cx, 0x83) == 0xffffu, "clip rectangles are unsupported");
    Require((read(cx, 0x8c) & 0xfu) == 0xau, "nonstandard triangle edge rules are unsupported");
    Require(read(cx, 0x2f9) == 0x2du, "nonstandard pixel center or vertex quantization is unsupported");
    Require(read(cx, 0x313) == 0x6000u, "conservative rasterization is unsupported");
    Require(read(cx, 0x30e) == 0xffffffffu && read(cx, 0x30f) == 0xffffffffu, "sample masks are unsupported");
    const auto viewportControl = read(cx, 0x206);
    if (viewportControl != 0x43fu) {
        std::ostringstream message;
        message << "AGC graphics: PA_CL_VTE_CNTL=0x" << std::hex << viewportControl << ": expected 0x43f for homogeneous positions and all viewport transforms; pre-divided coordinates, reciprocal W or disabled transforms are unsupported";
        throw std::runtime_error(message.str());
    }
    // PA_CL_CLIP_CNTL: DX_CLIP_SPACE_DEF (bit 19) picks the depth range; ZCLIP_NEAR/FAR_DISABLE
    // (bits 26-27) keep primitives beyond the depth planes by clamping (shadow rendering).
    const auto clipControl = read(cx, 0x204);
    if ((clipControl & ~0x0c080000u) != 0) {
        std::ostringstream message;
        message << "AGC graphics: unsupported PA_CL_CLIP_CNTL flags: 0x" << std::hex << clipControl;
        throw std::runtime_error(message.str());
    }
    result.depthClamp = (clipControl & 0x0c000000u) != 0;
    result.negativeOneToOne = (clipControl & 0x80000u) == 0;
    const auto raster = read(cx, 0x205);
    // Bits 11-13: polygon offset for front, back and parallelogram primitives (one setting serves
    // both faces on the host: titles enable them together).
    const bool offsetFront = (raster & 0x800u) != 0, offsetBack = (raster & 0x1000u) != 0;
    if (const auto rest = raster & ~0x3807u; rest != 0 && rest != 0x240u) {
        std::ostringstream message;
        message << "AGC graphics: polygon mode, provoking vertex or nonstandard rasterization is unsupported: PA_SU_SC_MODE_CNTL=0x" << std::hex << raster;
        throw std::runtime_error(message.str());
    }
    result.depthBias = offsetFront || offsetBack;
    if (result.depthBias) {
        Require(offsetFront == offsetBack || ((raster & 3u) != 0), "polygon offset for a single face of two-sided geometry is unsupported");
        const bool useBack = !offsetFront;
        result.depthBiasSlope = readFloat(cx, useBack ? 0x2e2 : 0x2e0) / 16.0f;
        result.depthBiasConstant = readFloat(cx, useBack ? 0x2e3 : 0x2e1);
    }
    result.cullMode = ((raster & 1u) != 0 ? VK_CULL_MODE_FRONT_BIT : 0u) | ((raster & 2u) != 0 ? VK_CULL_MODE_BACK_BIT : 0u);
    if (result.rectList) result.cullMode = VK_CULL_MODE_NONE;
    static const bool disableCulling = std::getenv("ANYPS5_DEBUG_NO_CULL") != nullptr; // diagnostics
    if (disableCulling) result.cullMode = VK_CULL_MODE_NONE;
    result.frontFace = (raster & 4u) != 0 ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    const auto targetMask = read(cx, 0x8e);
    const auto shaderMask = read(cx, 0x8f);
    // Dual-source blending exports MRT1 as the second blend source of target 0.
    result.dualSourceBlend = ((read(cx, 0x203) >> 9u) & 1u) != 0;
    // Dual-source blending owns MRT1 as the second blend source; otherwise targets 0..k-1 may be
    // written (a gap in the sequence has no host attachment and is unsupported).
    if (result.dualSourceBlend && (targetMask & ~0xfu) != 0) {
        std::ostringstream message;
        message << "AGC graphics: dual-source blending with more than one color target is unsupported: CB_TARGET_MASK=0x" << std::hex << targetMask << ", CB_SHADER_MASK=0x" << shaderMask;
        throw std::runtime_error(message.str());
    }
    // CB_COLOR_CONTROL: MODE (bits 4-6) 0 disables the color buffer (depth-only passes keep
    // their target registers), 1 is normal rendering; ROP3 (bits 16-23) must be copy (0xcc).
    const auto colorControl = read(cx, 0x202);
    const auto colorMode = (colorControl >> 4u) & 7u;
    if (colorMode > 1 || (colorControl & 0xff0000u) != 0xcc0000u || (colorControl & ~0xff0070u) != 0) {
        std::ostringstream message;
        message << "AGC graphics: only normal or disabled color rendering with copy ROP is supported: CB_COLOR_CONTROL=0x" << std::hex << colorControl;
        throw std::runtime_error(message.str());
    }
    // SPI_SHADER_COL_FORMAT: one nibble per MRT; the recompiler unpacks 16-bit exports (modes
    // 4-7) and passes 32-bit ones (1-3, 9) through. Only MRT0 (and MRT1 for dual-source
    // blending) may export. Mode 0 exports nothing: the bound color target keeps its contents.
    const auto exportFormat = read(cx, 0x1c5);
    const auto exportMode0 = exportFormat & 0xfu;
    const auto supportedExport = [](std::uint32_t mode) { return (mode >= 1 && mode <= 7) || mode == 9; };
    result.hasColorTarget = (targetMask & 0xfu) != 0 && colorMode == 1 && exportMode0 != 0;
    // CB_SHADER_MASK lists the components the pixel shader exports; unexported ones are not written.
    Require(!result.hasColorTarget || (shaderMask & 0xfu) != 0, "color target without shader color exports");
    zero(cx, 0x1c4, ~0u, "depth or sample-mask export");
    // Every written target needs a supported export format; exports to masked targets are dropped.
    // Titles set DUAL_EXPORT_ENABLE with a single export too; MRT1 then simply does not exist.
    std::uint32_t targetCount = 0;
    if (result.hasColorTarget && !result.dualSourceBlend) {
        while (targetCount < 8 && ((targetMask >> (4u * targetCount)) & 0xfu) != 0 && ((exportFormat >> (4u * targetCount)) & 0xfu) != 0) ++targetCount;
        for (std::uint32_t n = targetCount; n < 8; ++n) {
            if (((targetMask >> (4u * n)) & 0xfu) != 0 && ((exportFormat >> (4u * n)) & 0xfu) != 0) {
                std::ostringstream message;
                message << "AGC graphics: color targets must be written contiguously from target zero: CB_TARGET_MASK=0x" << std::hex << targetMask << ", SPI_SHADER_COL_FORMAT=0x" << exportFormat;
                throw std::runtime_error(message.str());
            }
        }
    } else if (result.hasColorTarget) targetCount = 1;
    for (std::uint32_t n = 0; n < 8; ++n) {
        const auto mode = (exportFormat >> (4u * n)) & 0xfu;
        const bool written = n < targetCount || (n == 1 && result.dualSourceBlend);
        if (mode != 0 && written && !supportedExport(mode)) {
            std::ostringstream message;
            message << "AGC graphics: unsupported color export format: SPI_SHADER_COL_FORMAT=0x" << std::hex << exportFormat;
            throw std::runtime_error(message.str());
        }
    }
    Require(read(cx, 0x1c3) == 4, "additional position exports are unsupported");
    // One color target: CB_COLORn_* registers sit 15 DWORDs apart, the ATTRIB2/3 and BASE_EXT
    // registers one DWORD apart.
    const auto decodeTarget = [&](std::uint32_t n, ColorTarget& target) {
        const auto cbReg = [n](std::uint32_t base) { return base + n * 0xfu; };
        const auto info = read(cx, cbReg(0x31c));
        const auto number = (info >> 8u) & 7u;
        const auto swap = (info >> 11u) & 3u;
        const auto colorFormat = DecodeColorFormat((info >> 2u) & 0x1fu, number, swap);
        if (colorFormat.format == VK_FORMAT_UNDEFINED) {
            std::ostringstream message;
            message << "AGC graphics: unsupported color format or component order: CB_COLOR" << std::dec << n << "_INFO=0x" << std::hex << info << " (format " << std::dec << ((info >> 2u) & 0x1fu) << ", number type " << number << ", swap " << swap << ")";
            throw std::runtime_error(message.str());
        }
        // Bit 18 (ROUND_MODE) picks the conversion rounding; the host rounds to nearest, which is
        // what float targets (no conversion) and titles' normalized targets tolerate.
        // Compression metadata (FAST_CLEAR, COMPRESSION, CMASK/FMASK/DCC controls) and blend
        // optimization hints (bits 13-14 and 19-30) describe how the console's color block keeps
        // the surface; the host renders every target uncompressed, so they change nothing here.
        // Endian swaps (bits 0-1) would change the stored bytes and stay unsupported.
        constexpr std::uint32_t compressionBits = 0x7ff86000u;
        if ((info & compressionBits) != 0) {
            static std::once_flag once;
            std::call_once(once, [&] { APS5_LOG_OUT("color target compression or optimization bits ignored: CB_COLOR%u_INFO=0x%08x", n, info); });
        }
        Require((info & ~(0x00069f7cu | compressionBits)) == 0, "color endian conversion is unsupported");
        Require((info & 0x8000u) != 0 || number == 7, "unclamped normalized color is unsupported");
        // CB_COLOR0_VIEW: slice range (bits 0-12, 13-25) and mip level (bits 26-29) of the surface.
        const auto view = readOr(cx, cbReg(0x31b), 0);
        const auto sliceStart = view & 0x1fffu;
        const auto sliceMax = (view >> 13u) & 0x1fffu;
        const auto requestedMip = (view >> 26u) & 0xfu;
        if ((view & ~0x3fffffffu) != 0 || sliceMax != sliceStart) {
            std::ostringstream message;
            message << "AGC graphics: multi-slice color views are unsupported: CB_COLOR" << std::dec << n << "_VIEW=0x" << std::hex << view;
            throw std::runtime_error(message.str());
        }
        zero(cx, cbReg(0x31d), ~0u, "color samples, fragments or destination alpha override");
        const auto attrib2 = read(cx, 0x3b0 + n);
        const auto maxMip = attrib2 >> 28u;
        if (view != 0) {
            static std::mutex viewMutex;
            static std::set<std::uint64_t> seenViews;
            std::lock_guard lock(viewMutex);
            if (seenViews.insert((static_cast<std::uint64_t>(view) << 32u) | attrib2).second) APS5_LOG_OUT("color view 0x%08x attrib2 0x%08x (mip %u of max %u, base %ux%u)", view, attrib2, requestedMip, maxMip, ((attrib2 >> 14u) & 0x3fffu) + 1u, (attrib2 & 0x3fffu) + 1u);
        }
        // A stale view (titles leave the mip field from a previous target) is clamped to the
        // surface's last level, as the hardware bounds addressing by MAX_MIP.
        const auto mipLevel = std::min(requestedMip, maxMip);
        const auto attrib3 = read(cx, 0x3b8 + n);
        target.tileMode = DecodeColorTileMode(attrib3);
        const VkExtent2D baseExtent{((attrib2 >> 14u) & 0x3fffu) + 1u, (attrib2 & 0x3fffu) + 1u};
        target.extent = {std::max(baseExtent.width >> mipLevel, 1u), std::max(baseExtent.height >> mipLevel, 1u)};
        const auto high = read(cx, 0x390 + n);
        Require((high & ~0xffu) == 0, "invalid color address extension");
        target.address = (static_cast<std::uint64_t>(high) << 40u) | (static_cast<std::uint64_t>(read(cx, cbReg(0x318))) << 8u);
        target.format = colorFormat.format;
        target.bytesPerPixel = colorFormat.bytesPerPixel;
        // A GPU-only target's guest range only serves aliasing checks: when the title's allocation
        // is smaller than the padded surface (protection gaps at its tail), the mapped prefix is used.
        const auto checkRange = [&](std::size_t alignment) {
            try {
                GuestMemory::CheckGpuRange(reinterpret_cast<const void*>(target.address), target.bytes, alignment, true);
            } catch (const std::exception& error) {
                if (!target.gpuOnly) {
                    std::ostringstream message;
                    message << error.what() << " (color target " << target.extent.width << "x" << target.extent.height << ", " << colorFormat.bytesPerPixel << " bytes per texel, tile mode " << static_cast<unsigned>(target.tileMode) << ", mip " << mipLevel << ", slice " << sliceStart << ")";
                    throw std::runtime_error(message.str());
                }
                const auto mapped = GuestMemory::MappedGpuBytes(reinterpret_cast<const void*>(target.address), target.bytes, true);
                Require(mapped != 0, "GPU-only color target starts in unmapped guest memory");
                static std::mutex reportMutex;
                static std::set<std::uint64_t> reported;
                std::lock_guard lock(reportMutex);
                if (reported.insert(target.address).second) {
                    APS5_LOG_OUT("color target 0x%llx %ux%u (%u bpp, mip %u, slice %u, CB_COLOR%u_INFO 0x%08x, ATTRIB2 0x%08x, ATTRIB3 0x%08x): %zu of %zu bytes mapped; using the mapped prefix", static_cast<unsigned long long>(target.address), target.extent.width, target.extent.height, colorFormat.bytesPerPixel, mipLevel, sliceStart, n, info, attrib2, attrib3, mapped, target.bytes);
                    GuestMemory::DescribeRegions(target.address, target.bytes);
                }
                target.bytes = mapped;
            }
        };
        if (mipLevel == 0 && sliceStart == 0) {
            const ColorTargetLayout colorLayout(target.extent.width, target.extent.height, target.tileMode, colorFormat.bytesPerPixel);
            target.bytes = colorLayout.Bytes();
            // The host tiling code and guest write-back handle 32-bit texels; other targets stay on the GPU.
            target.gpuOnly = colorFormat.bytesPerPixel != 4 || target.tileMode == ColorTileMode::ZOrder64KB;
            checkRange(colorLayout.Alignment());
        } else {
            // A mip level or array slice of a surface is its own GPU-resident target at its offset
            // inside the surface (the texture mip layout gives the placement).
            const auto textureTileMode = target.tileMode == ColorTileMode::Linear ? TextureTileMode::kLinear : TextureTileMode::RenderTarget64KB;
            const std::uint32_t canonicalFormat = colorFormat.bytesPerPixel == 1 ? 128u : colorFormat.bytesPerPixel == 2 ? 133u : colorFormat.bytesPerPixel == 4 ? 56u : colorFormat.bytesPerPixel == 8 ? 64u : 77u;
            const auto mips = ComputeMipLayout(textureTileMode, canonicalFormat, baseExtent.width, baseExtent.height, maxMip + 1u);
            Require(mipLevel < mips.size(), "color mip level is outside the surface layout");
            const auto sliceBytes = ComputeSurfaceSize(mips, 1);
            target.address += static_cast<std::uint64_t>(sliceStart) * sliceBytes + mips[mipLevel].tiledOffset;
            target.bytes = static_cast<std::size_t>(std::max<std::uint64_t>(mips[mipLevel].tiledSize, 256u));
            target.gpuOnly = true;
            checkRange(256u);
        }
        target.componentMapping = 0xe4u;
    };
    if (result.hasColorTarget) {
        decodeTarget(0, result.color);
        for (std::uint32_t n = 1; n < targetCount; ++n) {
            ColorTarget target{};
            decodeTarget(n, target);
            Require(target.extent.width == result.color.extent.width && target.extent.height == result.color.extent.height, "color targets of a draw differ in size");
            result.extraColors.push_back(target);
        }
        result.renderExtent = result.color.extent;
    } else if (result.hasDepthTarget) {
        result.renderExtent = result.depth.extent;
    } else {
        const auto screenBottomRight = read(cx, 0xd);
        result.renderExtent = {screenBottomRight & 0xffffu, screenBottomRight >> 16u};
        Require(result.renderExtent.width != 0 && result.renderExtent.height != 0, "empty framebuffer extent for a draw without color writes");
    }
    if (result.hasDepthTarget) {
        // The depth surface must cover the rendered area; a larger surface is fine.
        Require(result.depth.extent.width >= result.renderExtent.width && result.depth.extent.height >= result.renderExtent.height, "depth target is smaller than the render extent");
        result.depth.extent = result.renderExtent;
    }
    const auto xs = readFloat(cx, 0x10f);
    const auto xo = readFloat(cx, 0x110);
    const auto ys = readFloat(cx, 0x111);
    const auto yo = readFloat(cx, 0x112);
    const auto zs = readFloat(cx, 0x113);
    const auto zo = readFloat(cx, 0x114);
    const auto minDepth = result.negativeOneToOne ? zo - zs : zo;
    const auto maxDepth = zo + zs;
    if (!(xs > 0 && ys != 0 && std::isfinite(minDepth) && std::isfinite(maxDepth))) {
        std::ostringstream message;
        message << "AGC graphics: unsupported viewport transform: scale=(" << xs << ", " << ys << ", " << zs << "), offset=(" << xo << ", " << yo << ", " << zo << "), depth=(" << minDepth << ", " << maxDepth << "), negativeOneToOne=" << result.negativeOneToOne;
        throw std::runtime_error(message.str());
    }
    Require(readFloat(cx, 0xb4) <= readFloat(cx, 0xb5), "inverted viewport depth clamp bounds");
    float mappedMin = minDepth;
    float mappedMax = maxDepth;
    {
        // The console stores whatever the viewport produces in a float depth buffer (GL-style
        // clip space yields [-1, 1]). The host depth buffer is private to the driver and only ever
        // compared against itself, so an affine map onto [0, 1] keeps every test and clear exact.
        const float low = std::min(minDepth, maxDepth);
        const float high = std::max(minDepth, maxDepth);
        if (low < 0.0f || high > 1.0f) {
            const float span = high > low ? high - low : 1.0f;
            const auto remap = [&](float value) { return std::clamp((value - low) / span, 0.0f, 1.0f); };
            mappedMin = remap(minDepth);
            mappedMax = remap(maxDepth);
            result.depthState.depthClear = remap(result.depthState.depthClear);
        }
    }
    if (!std::isfinite(result.depthState.depthClear) || !std::isfinite(mappedMin) || !std::isfinite(mappedMax)) {
        static std::once_flag once;
        std::call_once(once, [&] { APS5_LOG_OUT("non-finite depth state: clear %g (DB_DEPTH_CLEAR 0x%08x, clear requested %d) viewport depth %g..%g (scale %g offset %g, negativeOneToOne %d)", result.depthState.depthClear, readOr(cx, 0xb, 0), result.depthState.clearDepth ? 1 : 0, mappedMin, mappedMax, zs, zo, result.negativeOneToOne ? 1 : 0); });
    }
    result.viewport = {xo - xs + static_cast<float>(windowX), yo - ys + static_cast<float>(windowY), 2 * xs, 2 * ys, mappedMin, mappedMax};
    result.scissor = {{0, 0}, result.renderExtent};
    intersect(result.scissor, cx, 0xc, true);
    intersect(result.scissor, cx, 0x81, false, windowX, windowY);
    intersect(result.scissor, cx, 0x90, false, windowX, windowY);
    if ((read(cx, 0x292) & 2u) != 0) intersect(result.scissor, cx, 0x94, false, windowX, windowY);
    {
        static const bool disableDepth = std::getenv("ANYPS5_DEBUG_NO_DEPTH") != nullptr; // diagnostics: no depth test
        if (disableDepth) { result.depthState.test = false; result.depthState.write = false; result.depthState.compare = VK_COMPARE_OP_ALWAYS; }
    }
    // CB_BLENDn_CONTROL per target; the blend constants are shared.
    const auto decodeBlend = [&](std::uint32_t n, VkPipelineColorBlendAttachmentState& state) {
        const auto blend = read(cx, 0x1e0 + n);
        Require((blend & 0x0000e000u) == 0, "reserved blend control bits");
        state.colorWriteMask = ((targetMask >> (4u * n)) & 0xfu) & ((shaderMask >> (4u * n)) & 0xfu);
        state.blendEnable = (blend >> 30u) & 1u;
        static const bool disableBlending = std::getenv("ANYPS5_DEBUG_NO_BLEND") != nullptr; // diagnostics: draw blended geometry opaque
        if (disableBlending) state.blendEnable = 0;
        if (state.blendEnable) {
            Require((read(cx, 0x31c + n * 0xfu) & 0x10000u) == 0, "blend bypass conflicts with enabled blending");
            state.srcColorBlendFactor = blendFactor(blend & 0x1fu);
            state.dstColorBlendFactor = blendFactor((blend >> 8u) & 0x1fu);
            state.colorBlendOp = blendOp((blend >> 5u) & 7u);
            const auto alpha = (blend & 0x20000000u) != 0 ? blend >> 16u : blend;
            state.srcAlphaBlendFactor = blendFactor(alpha & 0x1fu);
            state.dstAlphaBlendFactor = blendFactor((alpha >> 8u) & 0x1fu);
            state.alphaBlendOp = blendOp((alpha >> 5u) & 7u);
            for (std::uint32_t i = 0; i < 4; ++i) result.blendConstants[i] = readFloat(cx, 0x105 + i);
        }
    };
    if (result.hasColorTarget) {
        decodeBlend(0, result.blend);
        for (std::uint32_t n = 0; n < result.extraColors.size(); ++n) {
            VkPipelineColorBlendAttachmentState state{};
            decodeBlend(n + 1, state);
            result.extraBlends.push_back(state);
        }
    }
    return result;
}

}

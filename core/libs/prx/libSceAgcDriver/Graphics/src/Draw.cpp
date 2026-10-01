#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Execution/include/GpuJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GraphicsPipelineCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferCache.hpp"
#include <optional>
#include <span>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <limits>
#include <memory>

namespace AgcDriver::Graphics {
namespace {

struct DrawStorage {
    std::unique_ptr<Buffer> indices;
    std::vector<std::byte> indexCopy;
    std::vector<std::unique_ptr<Buffer>> vertices;
    std::vector<std::shared_ptr<Buffer>> hostViews;
    std::shared_ptr<ResidentColor> color;
    std::vector<std::shared_ptr<ResidentColor>> extraColors;
    std::shared_ptr<DepthImage> depth;
    std::shared_ptr<Pipeline> pipeline;
};

bool samplesAttachment(const std::vector<std::shared_ptr<Texture>>& textures, DrawStorage& storage) {
    for (const auto& texture : textures) {
        const auto image = texture->Image();
        if (image == VK_NULL_HANDLE) continue;
        if (storage.color && storage.color->Target().Image() == image) return true;
        if (std::any_of(storage.extraColors.begin(), storage.extraColors.end(), [&](const auto& extra) { return extra->Target().Image() == image; })) return true;
        if (storage.depth && (storage.depth->Image() == image || std::find(texture->DepthSources().begin(), texture->DepthSources().end(), storage.depth) != texture->DepthSources().end())) return true;
    }
    return false;
}

}

bool WritesDepthStencil(const DepthState& depthState) {
    const auto writesFace = [](const VkStencilOpState& face) { return face.writeMask != 0 && (face.failOp != VK_STENCIL_OP_KEEP || face.passOp != VK_STENCIL_OP_KEEP || face.depthFailOp != VK_STENCIL_OP_KEEP); };
    return depthState.write || depthState.clearDepth || depthState.clearStencil || (depthState.stencilTest && (writesFace(depthState.front) || writesFace(depthState.back)));
}

void Draw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots) {
    PerformanceTimer timing("Graphics.Draw");
    {
        static const char* skipList = std::getenv("ANYPS5_DEBUG_SKIP_DRAW");
        if (skipList != nullptr) {
            std::string list = skipList;
            std::size_t at = 0;
            while (at < list.size()) {
                const auto comma = list.find(',', at);
                const auto item = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
                const auto colon = item.find(':');
                if (colon != std::string::npos && std::strtoul(item.c_str(), nullptr, 10) == draw.indexCount && std::strtoul(item.c_str() + colon + 1, nullptr, 10) == (state.blend.blendEnable ? 1u : 0u)) return;
                if (comma == std::string::npos) break;
                at = comma + 1;
            }
        }
    }
    Require(draw.indexed ? draw.flags == 0 : (draw.flags & ~0x20u) == 0, "draw modifiers are unsupported");
    if (draw.indexed) {
        Require(draw.indexSize == 2 || draw.indexSize == 4, "only uint16 and uint32 index buffers are supported");
        Require(!state.stages.mesh || (draw.firstVertex == 0 && draw.firstInstance == 0), "indexed mesh draw offsets are unsupported");
    } else {
        Require(draw.indexAddress == 0 && draw.indexSize == 0, "auto draw must not reference an index buffer");
        if (draw.indexCount == 0 || draw.instanceCount == 0) return;
        Require(draw.firstVertex <= std::numeric_limits<std::uint32_t>::max() - (draw.indexCount - 1u), "auto draw vertex range overflow");
        Require(draw.firstInstance <= std::numeric_limits<std::uint32_t>::max() - (draw.instanceCount - 1u), "auto draw instance range overflow");
    }
    Require(draw.indexCount != 0 && draw.instanceCount != 0, "zero-count indexed draws are unsupported");
    const auto indexBytes = static_cast<std::uint64_t>(draw.indexCount) * draw.indexSize;
    Require(indexBytes <= std::numeric_limits<std::size_t>::max(), "index buffer size overflow");
    if (draw.indexed) GuestMemory::CheckRange(reinterpret_cast<const void*>(draw.indexAddress), static_cast<std::size_t>(indexBytes), draw.indexSize);
    Require(!draw.indexed || !state.hasColorTarget || draw.indexAddress + indexBytes <= state.color.address || state.color.address + state.color.bytes <= draw.indexAddress, "index buffer aliases the render target");
    if (state.rectList) Require(draw.indexCount % 3 == 0, "incomplete rect-list primitive");
    {
        const auto& viewport = state.viewport;
        const float left = std::min(viewport.x, viewport.x + viewport.width), right = std::max(viewport.x, viewport.x + viewport.width);
        const float top = std::min(viewport.y, viewport.y + viewport.height), bottom = std::max(viewport.y, viewport.y + viewport.height);
        const bool outsideTarget = right <= 0.0f || bottom <= 0.0f || left >= static_cast<float>(state.renderExtent.width) || top >= static_cast<float>(state.renderExtent.height);
        const bool outsideDevice = left < context.limits.viewportBoundsRange[0] || right > context.limits.viewportBoundsRange[1] || top < context.limits.viewportBoundsRange[0] || bottom > context.limits.viewportBoundsRange[1];
        if (outsideTarget || outsideDevice) {
            static std::once_flag once;
            std::call_once(once, [&] { APS5_LOG_OUT("skipping draws whose viewport (%g, %g, %g, %g) lies outside the %ux%u target or the device bounds", viewport.x, viewport.y, viewport.width, viewport.height, state.renderExtent.width, state.renderExtent.height); });
            return;
        }
    }
    Require(!shaders.empty() && shaders.front().program != nullptr, "missing compiled shader");
    const auto shaderStages = PipelineStages(shaders);
    std::uint32_t meshGroups = 0;
    if (state.stages.mesh) {
        Require(draw.firstVertex == 0 && draw.firstInstance == 0, "mesh draw offsets are unsupported");
        Require(context.meshShader, "device does not support mesh shaders");
        const auto& mesh = *state.stages.mesh;
        const auto inputSize = mesh.inputPrimitive == 1 ? 1u : mesh.inputPrimitive == 2 ? 2u : 3u;
        Require(draw.indexCount >= inputSize && mesh.primitivesPerGroup != 0, "mesh draw contains no complete primitive");
        const auto step = mesh.inputPrimitive == 6 ? 1u : inputSize;
        const auto primitives = (draw.indexCount - inputSize) / step + 1u;
        meshGroups = (primitives - 1u) / mesh.primitivesPerGroup + 1u;
        Require(meshGroups <= context.meshLimits.maxMeshWorkGroupCount[0] && draw.instanceCount <= context.meshLimits.maxMeshWorkGroupCount[1] && static_cast<std::uint64_t>(meshGroups) * draw.instanceCount <= context.meshLimits.maxMeshWorkGroupTotalCount, "mesh draw exceeds workgroup count limits");
    }
    if (state.stages.tessellation) Require(draw.indexCount % state.stages.tessellation->inputControlPoints == 0, "incomplete tessellation patch");
    timing.Mark("validate");
    Require(context.renderCache != nullptr && context.drawQueue != nullptr && context.graphicsPipelines != nullptr, "device graphics execution caches are unavailable");
    const auto& attributes = shaders.front().program->vertexAttributes;
    auto storage = std::make_shared<DrawStorage>();
    auto& indices = storage->indices;
    std::uint32_t maxIndex = draw.indexed ? 0u : draw.firstVertex + draw.indexCount - 1u;
    VkBuffer indexHandle = VK_NULL_HANDLE;
    VkDeviceSize indexOffset = 0;
    if (draw.indexed) {
        std::span<const std::byte> indexData;
        auto view = context.guestBufferCache != nullptr ? context.guestBufferCache->HostRange(draw.indexAddress, indexBytes) : GuestBufferCache::HostView{};
        if (view.buffer && view.offset % 4u == 0) {
            indexHandle = view.buffer->Handle();
            indexOffset = view.offset;
            if (view.resident) {
                storage->indexCopy.resize(static_cast<std::size_t>(indexBytes));
                if (indexBytes != 0) GuestMemory::Read(draw.indexAddress, storage->indexCopy);
                indexData = storage->indexCopy;
            } else {
                indexData = view.buffer->Bytes().subspan(static_cast<std::size_t>(view.offset), static_cast<std::size_t>(indexBytes));
            }
            storage->hostViews.push_back(std::move(view.buffer));
        } else {
            indices = std::make_unique<Buffer>(context, static_cast<std::size_t>(indexBytes), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            GuestMemory::Read(draw.indexAddress, indices->Bytes(), draw.indexSize);
            indexHandle = indices->Handle();
            indexData = indices->Bytes();
        }
        if (!attributes.empty() || context.limits.maxDrawIndexedIndexValue != std::numeric_limits<std::uint32_t>::max()) {
            for (std::size_t offset = 0; offset < indexBytes; offset += draw.indexSize) {
                std::uint32_t index = 0;
                if (draw.indexSize == 2) {
                    std::uint16_t value = 0;
                    std::memcpy(&value, indexData.data() + offset, sizeof(value));
                    index = value;
                } else {
                    std::memcpy(&index, indexData.data() + offset, sizeof(index));
                }
                Require(index <= context.limits.maxDrawIndexedIndexValue, "index exceeds the device's indexed draw limit");
                maxIndex = std::max(maxIndex, index);
            }
            const auto baseVertex = static_cast<std::int64_t>(static_cast<std::int32_t>(draw.firstVertex));
            const auto highest = static_cast<std::int64_t>(maxIndex) + baseVertex;
            Require(highest >= 0 && highest <= static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()), "indexed draw base vertex moves the fetch range outside the vertex domain");
            maxIndex = static_cast<std::uint32_t>(highest);
        }
    }
    timing.Mark("index_upload");
    auto& vertexBuffers = storage->vertices;
    std::vector<VkBuffer> vertexHandles;
    std::vector<VkDeviceSize> vertexOffsets(attributes.size(), 0);
    for (const auto& attribute : attributes) {
        const auto bytes = VertexBufferReadSize(attribute, maxIndex, draw.instanceCount, draw.firstInstance);
        if (IsNullVertexAttribute(attribute)) {
            auto buffer = std::make_unique<Buffer>(context, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            std::memset(buffer->Bytes().data(), 0, bytes);
            vertexHandles.push_back(buffer->Handle());
            vertexBuffers.push_back(std::move(buffer));
            continue;
        }
        const auto& fields = attribute.resource.fields;
        const auto address = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
        Require(!state.hasColorTarget || address + bytes <= state.color.address || state.color.address + state.color.bytes <= address, "vertex buffer aliases the render target");
        GuestMemory::CheckRange(reinterpret_cast<const void*>(address), bytes, 1);
        if (auto view = context.guestBufferCache != nullptr ? context.guestBufferCache->HostRange(address, bytes) : GuestBufferCache::HostView{}; view.buffer && view.offset % 4u == 0) {
            vertexOffsets[vertexHandles.size()] = view.offset;
            vertexHandles.push_back(view.buffer->Handle());
            storage->hostViews.push_back(std::move(view.buffer));
            continue;
        }
        auto buffer = std::make_unique<Buffer>(context, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        GuestMemory::Read(address, buffer->Bytes(), 1);
        {
            static const bool dumpVertices = std::getenv("ANYPS5_DUMP_FRAME") != nullptr;
            static int reported = 0;
            if (dumpVertices && draw.indexed && draw.indexCount <= 12 && reported < 12) {
                ++reported;
                std::string text;
                const auto* words = reinterpret_cast<const std::uint32_t*>(buffer->Bytes().data());
                for (std::size_t i = 0; i < std::min<std::size_t>(bytes / 4, 16); ++i) {
                    float value;
                    std::memcpy(&value, words + i, 4);
                    char item[40];
                    std::snprintf(item, sizeof(item), " %08x(%g)", words[i], value);
                    text += item;
                }
                APS5_LOG_OUT("draw vertices: attribute %u at 0x%llx stride %u format 0x%x %zu bytes, indices max %u:%s", attribute.location, static_cast<unsigned long long>(address), (fields[1] >> 16u) & 0x3fffu, (fields[3] >> 12u) & 0x7fu, bytes, maxIndex, text.c_str());
            }
        }
        vertexHandles.push_back(buffer->Handle());
        vertexBuffers.push_back(std::move(buffer));
    }
    timing.Mark("vertex_upload");
    if (state.hasDepthTarget) storage->depth = context.renderCache->GetDepth(state.depth);
    auto resources = std::make_shared<ShaderResources>(context, shaders, state.color, draw.indexAddress, static_cast<std::size_t>(indexBytes), snapshots, storage->depth.get());
    timing.Mark("shader_resources");
    {
        static const bool journalTextures = std::getenv("ANYPS5_DUMP_FRAME") != nullptr;
        static const bool journalAllTextures = std::getenv("ANYPS5_JOURNAL_TEXTURES") != nullptr;
        if (journalAllTextures || (journalTextures && draw.indexed && draw.indexCount <= 12)) {
            char head[200];
            std::snprintf(head, sizeof(head), "draw %u%s -> 0x%llx %ux%u vk%u+%zu blend %u(%u,%u) depth %s mask 0x%x; textures:", draw.indexCount, draw.indexed ? "i" : "a", state.hasColorTarget ? static_cast<unsigned long long>(state.color.address) : 0ull, state.renderExtent.width, state.renderExtent.height, state.hasColorTarget ? static_cast<unsigned>(state.color.format) : 0u, state.extraColors.size(), state.blend.blendEnable, static_cast<unsigned>(state.blend.srcColorBlendFactor), static_cast<unsigned>(state.blend.dstColorBlendFactor), state.hasDepthTarget ? (state.depthState.write ? "rw" : "r") : "-", state.blend.colorWriteMask);
            GpuJournal::Record(head + resources->DescribeTextures());
        }
        static const char* debugInputsValue = std::getenv("ANYPS5_DEBUG_GDS_INPUTS");
        static const auto debugStart = std::chrono::steady_clock::now();
        const bool debugInputs = debugInputsValue != nullptr && std::chrono::duration<double>(std::chrono::steady_clock::now() - debugStart).count() >= std::atof(debugInputsValue);
        const bool finalBlit = !draw.indexed && draw.indexCount == 3 && state.hasColorTarget && state.color.extent.width == 3840 && state.color.format == VK_FORMAT_B8G8R8A8_SRGB;
        if (debugInputs && ((draw.indexed && draw.indexCount >= 80000) || finalBlit || state.extraColors.size() >= 3)) {
            if (context.drawQueue) { context.drawQueue->Flush(); context.drawQueue->Wait(); }
            APS5_LOG_OUT("[draw-input] draw of %u indices%s (target vk%u +%zu extra, blend %u)", draw.indexCount, finalBlit ? " (final blit)" : "", state.hasColorTarget ? static_cast<unsigned>(state.color.format) : 0u, state.extraColors.size(), state.blend.blendEnable);
            for (const auto& shader : shaders) {
                if (shader.program == nullptr) continue;
                std::string dumped;
                if (const char* dumpDirectory = std::getenv("ANYPS5_DUMP_GDS_SHADERS")) {
                    std::uint64_t hash = 1469598103934665603ull;
                    for (const auto word : shader.program->spirv) hash = (hash ^ word) * 1099511628211ull;
                    char name[64];
                    std::snprintf(name, sizeof(name), "/gbuf_%u_%016llx.spv", static_cast<unsigned>(shader.stage), static_cast<unsigned long long>(hash));
                    dumped = std::string(dumpDirectory) + name;
                    if (FILE* file = std::fopen(dumped.c_str(), "wb")) { std::fwrite(shader.program->spirv.data(), sizeof(std::uint32_t), shader.program->spirv.size(), file); std::fclose(file); }
                }
                APS5_LOG_OUT("[draw-input]   stage %u %s", static_cast<unsigned>(shader.stage), dumped.c_str());
                for (const auto& binding : shader.program->bindings) {
                    if (binding.role != ShaderRecompiler::DescriptorRole::GuestImages) continue;
                    const auto& d = binding.guestDescriptor;
                    for (std::size_t element = 0; element + 8 <= d.size(); element += 8) {
                        APS5_LOG_OUT("[draw-input]   b%u[%zu] T# %08x %08x %08x %08x %08x %08x %08x %08x", binding.binding, element / 8, d[element], d[element + 1], d[element + 2], d[element + 3], d[element + 4], d[element + 5], d[element + 6], d[element + 7]);
                    }
                }
            }
            for (const auto& texture : resources->Textures()) {
                if (!finalBlit && texture->Extent().width * texture->Extent().height > 64 * 64) continue;
                APS5_LOG_OUT("[draw-input]   0x%llx %ux%u vk%u%s:%s", static_cast<unsigned long long>(texture->GuestAddress()), texture->Extent().width, texture->Extent().height, static_cast<unsigned>(texture->GuestFormat()), texture->IsDirectView() ? " direct" : "", context.textureCache->DescribeContents(*texture).c_str());
            }
        }
    }
    std::shared_ptr<ResidentColor> reportedTarget;
    GuestTextureResource reportedView{};
    std::vector<unsigned char> reportedBefore;
    VkRect2D reportedScissor{};
    if (state.hasColorTarget) {
        const ColorTargetLayout colorLayout(state.color.extent.width, state.color.extent.height, state.color.tileMode, state.color.bytesPerPixel);
        Require(state.color.gpuOnly || state.color.bytes == colorLayout.Bytes(), "color target transfer size mismatch");
        storage->color = context.renderCache->Get(state.color, state.blend.blendEnable != 0);
        {
            static const char* debugValue = std::getenv("ANYPS5_DEBUG_GDS_INPUTS");
            static const auto debugStart = std::chrono::steady_clock::now();
            const bool debugDraw = debugValue != nullptr && std::chrono::duration<double>(std::chrono::steady_clock::now() - debugStart).count() >= std::atof(debugValue);
            static const char* scissoredValue = std::getenv("ANYPS5_DEBUG_SCISSORED_DRAWS");
            static int scissoredReports = 0;
            const bool tinyTarget = state.renderExtent.width == 2u && state.renderExtent.height == 1u;
            static const bool stenciledOnly = std::getenv("ANYPS5_DEBUG_REPORT_STENCILED") != nullptr;
            const bool stenciled = state.depthState.stencilTest && state.depthState.front.compareOp == VK_COMPARE_OP_EQUAL;
            const bool scissoredDraw = scissoredValue != nullptr && (stenciledOnly ? stenciled && std::chrono::duration<double>(std::chrono::steady_clock::now() - debugStart).count() >= std::atof(scissoredValue) : (tinyTarget || (std::chrono::duration<double>(std::chrono::steady_clock::now() - debugStart).count() >= std::atof(scissoredValue) && state.scissor.extent.width < state.renderExtent.width && state.blend.blendEnable != 0))) && scissoredReports < 12;
            if (scissoredDraw) ++scissoredReports;
            if (scissoredDraw || (debugDraw && (state.color.format == VK_FORMAT_B10G11R11_UFLOAT_PACK32 || state.color.format == VK_FORMAT_R16G16B16A16_SFLOAT) && state.color.extent.width == 3840 && !draw.indexed)) {
                if (context.drawQueue) { context.drawQueue->Flush(); context.drawQueue->Wait(); }
                GuestTextureResource synthetic{};
                synthetic.baseAddress = state.color.address;
                synthetic.width = state.color.extent.width;
                synthetic.height = state.color.extent.height;
                synthetic.mipCount = 1;
                synthetic.tileMode = state.color.tileMode == ColorTileMode::RenderTarget ? TextureTileMode::RenderTarget64KB : TextureTileMode::kLinear;
                synthetic.dimension = synthetic.viewDimension = TextureDimension::k2D;
                synthetic.format = state.color.format == VK_FORMAT_R16G16B16A16_SFLOAT ? 0x47 : 0x24;
                synthetic.dstSelX = 4; synthetic.dstSelY = 5; synthetic.dstSelZ = 6; synthetic.dstSelW = 7;
                try {
                    Texture view(context, storage->color, synthetic, VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY}, Texture::DirectView{});
                    if (scissoredDraw) {
                        reportedTarget = storage->color;
                        reportedView = synthetic;
                        reportedBefore = context.textureCache->Contents(view);
                        reportedScissor = state.scissor;
                    }
                    APS5_LOG_OUT("[draw-target] draw of %u indices (%s) into 0x%llx %ux%u vk%u blend %u vp %g,%g %gx%g sc %d,%d %ux%u:%s", draw.indexCount, draw.indexed ? "indexed" : "auto", static_cast<unsigned long long>(state.color.address), state.renderExtent.width, state.renderExtent.height, static_cast<unsigned>(state.color.format), state.blend.blendEnable, state.viewport.x, state.viewport.y, state.viewport.width, state.viewport.height, state.scissor.offset.x, state.scissor.offset.y, state.scissor.extent.width, state.scissor.extent.height, context.textureCache->DescribeContents(view).c_str());
                } catch (const std::exception& error) {
                    APS5_LOG_OUT("[draw-target] 0x%llx: %s", static_cast<unsigned long long>(state.color.address), error.what());
                }
                for (const auto& texture : resources->Textures()) {
                    if (!scissoredDraw && texture->Extent().width * texture->Extent().height < 64 * 64) continue;
                    std::string guest;
                    if (scissoredDraw) {
                        std::array<std::uint32_t, 4> words{};
                        try {
                            GuestMemory::Read(texture->GuestAddress(), std::as_writable_bytes(std::span(words)), 1);
                            guest = " guest";
                            for (const auto word : words) { char item[12]; std::snprintf(item, sizeof(item), " %08x", word); guest += item; }
                        } catch (...) { guest = " guest unreadable"; }
                    }
                    APS5_LOG_OUT("[draw-target]   reads 0x%llx %ux%u vk%u%s:%s%s", static_cast<unsigned long long>(texture->GuestAddress()), texture->Extent().width, texture->Extent().height, static_cast<unsigned>(texture->GuestFormat()), texture->IsDirectView() ? " direct" : "", context.textureCache->DescribeContents(*texture).c_str(), guest.c_str());
                }
                if (draw.indexed && draw.indexCount <= 6) {
                    for (std::size_t v = 0; v < vertexBuffers.size(); ++v) {
                        const auto bytes = vertexBuffers[v]->Bytes();
                        std::string text;
                        for (std::size_t i = 0; i < std::min<std::size_t>(bytes.size() / 4, 24); ++i) {
                            std::uint32_t word; std::memcpy(&word, bytes.data() + i * 4, 4);
                            float value; std::memcpy(&value, &word, 4);
                            char item[48]; std::snprintf(item, sizeof(item), " %08x(%g)", word, value); text += item;
                        }
                        APS5_LOG_OUT("[draw-target]   vertex buffer %zu (%zu bytes):%s", v, bytes.size(), text.c_str());
                    }
                }
                for (const auto& shader : shaders) {
                    if (shader.program == nullptr) continue;
                    std::string buffers;
                    for (const auto& binding : shader.program->bindings) {
                        if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
                        const auto& d = binding.guestDescriptor;
                        for (std::size_t element = 0; element + 4 <= d.size(); element += 4) {
                            const auto base = d[element] | (static_cast<std::uint64_t>(d[element + 1] & 0xffffu) << 32u);
                            const auto stride = (d[element + 1] >> 16u) & 0x3fffu;
                            char item[96];
                            std::snprintf(item, sizeof(item), " [b%u base 0x%llx stride %u records %u w3 %08x", binding.binding, static_cast<unsigned long long>(base), stride, d[element + 2], d[element + 3]);
                            buffers += item;
                            if (base != 0 && static_cast<std::uint64_t>(std::max(stride, 1u)) * d[element + 2] <= 4096) {
                                std::vector<std::uint32_t> words(static_cast<std::uint64_t>(std::max(stride, 1u)) * d[element + 2] <= 128 ? 32 : 8);
                                try {
                                    GuestMemory::Read(base, std::as_writable_bytes(std::span(words)), 1);
                                    buffers += " =";
                                    for (const auto word : words) { std::snprintf(item, sizeof(item), " %08x", word); buffers += item; }
                                } catch (...) { buffers += " (unreadable)"; }
                            }
                            buffers += "]";
                        }
                    }
                    std::string dumped;
                    if (const char* dumpDirectory = std::getenv("ANYPS5_DUMP_GDS_SHADERS")) {
                        std::uint64_t hash = 1469598103934665603ull;
                        for (const auto word : shader.program->spirv) hash = (hash ^ word) * 1099511628211ull;
                        char name[64];
                        std::snprintf(name, sizeof(name), "/draw_%u_%016llx.spv", static_cast<unsigned>(shader.stage), static_cast<unsigned long long>(hash));
                        dumped = std::string(dumpDirectory) + name;
                        if (FILE* file = std::fopen(dumped.c_str(), "wb")) { std::fwrite(shader.program->spirv.data(), sizeof(std::uint32_t), shader.program->spirv.size(), file); std::fclose(file); }
                    }
                    APS5_LOG_OUT("[draw-target]   stage %u %s buffers:%s", static_cast<unsigned>(shader.stage), dumped.c_str(), buffers.c_str());
                }
            }
        }
        {
            static const char* traceValue = std::getenv("ANYPS5_DEBUG_DRAW_TARGETS");
            static const auto traceStart = std::chrono::steady_clock::now();
            static int remaining = [] {
                if (traceValue == nullptr) return 0;
                const char* colon = std::strchr(traceValue, ':');
                return colon != nullptr ? std::atoi(colon + 1) : 40;
            }();
            const bool rgba8 = state.color.format == VK_FORMAT_R8G8B8A8_UNORM || state.color.format == VK_FORMAT_R8G8B8A8_SRGB || state.color.format == VK_FORMAT_B8G8R8A8_UNORM || state.color.format == VK_FORMAT_B8G8R8A8_SRGB;
            if (traceValue != nullptr && remaining > 0 && rgba8 && std::chrono::duration<double>(std::chrono::steady_clock::now() - traceStart).count() >= std::atof(traceValue)) {
                --remaining;
                if (context.drawQueue) { context.drawQueue->Flush(); context.drawQueue->Wait(); }
                GuestTextureResource synthetic{};
                synthetic.baseAddress = state.color.address;
                synthetic.width = state.color.extent.width;
                synthetic.height = state.color.extent.height;
                synthetic.mipCount = 1;
                synthetic.tileMode = state.color.tileMode == ColorTileMode::RenderTarget ? TextureTileMode::RenderTarget64KB : TextureTileMode::kLinear;
                synthetic.dimension = synthetic.viewDimension = TextureDimension::k2D;
                synthetic.format = 0x38;
                synthetic.dstSelX = 4; synthetic.dstSelY = 5; synthetic.dstSelZ = 6; synthetic.dstSelW = 7;
                try {
                    Texture view(context, storage->color, synthetic, VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY}, Texture::DirectView{});
                    APS5_LOG_OUT("[draw-trace] draw of %u indices into 0x%llx vk%u blend %u (%u,%u,%u) mask 0x%x cull %u front %u; target before:%s", draw.indexCount, static_cast<unsigned long long>(state.color.address), static_cast<unsigned>(state.color.format), state.blend.blendEnable,
                        static_cast<unsigned>(state.blend.srcColorBlendFactor), static_cast<unsigned>(state.blend.dstColorBlendFactor), static_cast<unsigned>(state.blend.colorBlendOp), static_cast<unsigned>(state.blend.colorWriteMask), static_cast<unsigned>(state.cullMode), static_cast<unsigned>(state.frontFace), context.textureCache->DescribeContents(view).c_str());
                } catch (const std::exception& error) {
                    APS5_LOG_OUT("[draw-trace] 0x%llx: %s", static_cast<unsigned long long>(state.color.address), error.what());
                }
                for (const auto& texture : resources->Textures()) {
                    APS5_LOG_OUT("[draw-trace]   reads 0x%llx %ux%u vk%u%s:%s", static_cast<unsigned long long>(texture->GuestAddress()), texture->Extent().width, texture->Extent().height, static_cast<unsigned>(texture->GuestFormat()), texture->IsDirectView() ? " direct" : "", context.textureCache->DescribeContents(*texture).c_str());
                }
                {
                    std::string text;
                    for (const auto& attribute : shaders.front().program->vertexAttributes) {
                        const auto& fields = attribute.resource.fields;
                        char item[64]; std::snprintf(item, sizeof(item), " loc%u@0x%llx", attribute.location, static_cast<unsigned long long>(fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u)));
                        text += item;
                    }
                    APS5_LOG_OUT("[draw-trace]   attributes (cache hit %d):%s", shaders.front().program->cacheHit ? 1 : 0, text.c_str());
                    const auto& attributes0 = shaders.front().program->vertexAttributes;
                    if (!attributes0.empty() && !vertexBuffers.empty()) {
                        const auto& fields = attributes0.front().resource.fields;
                        const auto address = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
                        std::array<float, 16> now{};
                        try { GuestMemory::Read(address, std::as_writable_bytes(std::span(now)), 1); } catch (...) {}
                        std::string fresh;
                        for (const auto value : now) { char item[24]; std::snprintf(item, sizeof(item), " %g", value); fresh += item; }
                        APS5_LOG_OUT("[draw-trace]   vertex memory now:%s", fresh.c_str());
                    }
                }
                if (draw.indexed && storage->indices) {
                    std::string text;
                    for (std::size_t offset = 0; offset < std::min<std::size_t>(indexBytes, 24 * draw.indexSize); offset += draw.indexSize) {
                        std::uint32_t index = 0;
                        std::memcpy(&index, storage->indices->Bytes().data() + offset, draw.indexSize);
                        char item[16]; std::snprintf(item, sizeof(item), " %u", index); text += item;
                    }
                    APS5_LOG_OUT("[draw-trace]   indices (size %u, first vertex %u, instances %u):%s", draw.indexSize, draw.firstVertex, draw.instanceCount, text.c_str());
                }
                for (std::size_t v = 0; v < vertexBuffers.size() && draw.indexCount <= 6; ++v) {
                    const auto bytes = vertexBuffers[v]->Bytes();
                    std::string text;
                    for (std::size_t i = 0; i < std::min<std::size_t>(bytes.size() / 4, v == 0 ? 56 : 4); ++i) {
                        float value; std::memcpy(&value, bytes.data() + i * 4, 4);
                        char item[24]; std::snprintf(item, sizeof(item), " %g", value); text += item;
                    }
                    APS5_LOG_OUT("[draw-trace]   vertex buffer %zu:%s", v, text.c_str());
                }
                {
                    const auto push = AssemblePushConstants(shaders);
                    std::string text;
                    for (std::size_t i = 0; i + 4 <= push.size() && i < 48 * 4; i += 4) {
                        std::uint32_t word; std::memcpy(&word, push.data() + i, 4);
                        char item[16]; std::snprintf(item, sizeof(item), " %08x", word); text += item;
                    }
                    APS5_LOG_OUT("[draw-trace]   push constants:%s", text.c_str());
                    const auto dumpAt = [&](std::uint64_t address, const char* label) {
                        std::array<std::uint32_t, 48> words{};
                        try { GuestMemory::Read(address, std::as_writable_bytes(std::span(words)), 4); } catch (...) { APS5_LOG_OUT("[draw-trace]     %s 0x%llx unreadable", label, static_cast<unsigned long long>(address)); return words; }
                        std::string line;
                        for (std::size_t i = 0; i < words.size(); ++i) { float value; std::memcpy(&value, &words[i], 4); char item[32]; std::snprintf(item, sizeof(item), " %08x(%.4g)", words[i], value); line += item; }
                        APS5_LOG_OUT("[draw-trace]     %s 0x%llx:%s", label, static_cast<unsigned long long>(address), line.c_str());
                        return words;
                    };
                    for (std::size_t i = 0; i + 8 <= push.size() && i < 16 * 4; i += 8) {
                        std::uint32_t low, high; std::memcpy(&low, push.data() + i, 4); std::memcpy(&high, push.data() + i + 4, 4);
                        if (high == 0 || high > 0xff) continue;
                        const auto words = dumpAt(low | (static_cast<std::uint64_t>(high) << 32), "points to");
                        const auto inner = words[0] | (static_cast<std::uint64_t>(words[1]) << 32);
                        if (words[1] != 0 && words[1] <= 0xff) dumpAt(inner, "  which points to");
                    }
                }
                for (const auto& shader : shaders) {
                    if (shader.program == nullptr) continue;
                    std::string buffers;
                    for (const auto& binding : shader.program->bindings) {
                        if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
                        const auto& d = binding.guestDescriptor;
                        for (std::size_t element = 0; element + 4 <= d.size(); element += 4) {
                            const auto base = d[element] | (static_cast<std::uint64_t>(d[element + 1] & 0xffffu) << 32u);
                            char item[80];
                            std::snprintf(item, sizeof(item), " [b%u 0x%llx x%u", binding.binding, static_cast<unsigned long long>(base), d[element + 2]);
                            buffers += item;
                            std::array<float, 8> words{};
                            try {
                                GuestMemory::Read(base, std::as_writable_bytes(std::span(words)), 1);
                                buffers += " =";
                                for (const auto word : words) { std::snprintf(item, sizeof(item), " %g", word); buffers += item; }
                            } catch (...) { buffers += " (unreadable)"; }
                            buffers += "]";
                        }
                    }
                    for (const auto& binding : shader.program->bindings) {
                        if (binding.role != ShaderRecompiler::DescriptorRole::FlattenedSrt && binding.role != ShaderRecompiler::DescriptorRole::ShaderData) continue;
                        std::string text;
                        for (std::size_t i = 0; i < std::min<std::size_t>(binding.guestDescriptor.size(), 40); ++i) {
                            float value; std::memcpy(&value, &binding.guestDescriptor[i], 4);
                            char item[40]; std::snprintf(item, sizeof(item), " [%zu]%08x(%g)", i, binding.guestDescriptor[i], value); text += item;
                        }
                        APS5_LOG_OUT("[draw-trace]   stage %u %s b%u (%zu dwords):%s", static_cast<unsigned>(shader.stage), binding.role == ShaderRecompiler::DescriptorRole::FlattenedSrt ? "srt" : "data", binding.binding, binding.guestDescriptor.size(), text.c_str());
                    }
                    std::string dumped;
                    if (const char* dumpDirectory = std::getenv("ANYPS5_DUMP_GDS_SHADERS")) {
                        std::uint64_t hash = 1469598103934665603ull;
                        for (const auto word : shader.program->spirv) hash = (hash ^ word) * 1099511628211ull;
                        char name[64];
                        std::snprintf(name, sizeof(name), "/trace_%u_%016llx.spv", static_cast<unsigned>(shader.stage), static_cast<unsigned long long>(hash));
                        dumped = std::string(dumpDirectory) + name;
                        if (FILE* file = std::fopen(dumped.c_str(), "wb")) { std::fwrite(shader.program->spirv.data(), sizeof(std::uint32_t), shader.program->spirv.size(), file); std::fclose(file); }
                    }
                    APS5_LOG_OUT("[draw-trace]   stage %u %s buffers:%s", static_cast<unsigned>(shader.stage), dumped.c_str(), buffers.c_str());
                }
            }
        }
        for (std::size_t i = 0; i < state.extraColors.size(); ++i) {
            const auto& extra = state.extraColors[i];
            const ColorTargetLayout extraLayout(extra.extent.width, extra.extent.height, extra.tileMode, extra.bytesPerPixel);
            Require(extra.gpuOnly || extra.bytes == extraLayout.Bytes(), "color target transfer size mismatch");
            storage->extraColors.push_back(context.renderCache->Get(extra, state.extraBlends[i].blendEnable != 0));
        }
    }
    timing.Mark("render_target_cache");
    if (FrameTiming::Enabled() && samplesAttachment(resources->Textures(), *storage)) timing.Mark("feedback");
    RequireValidViewport(context, state);
    std::optional<State> preservedStencil;
    if (storage->depth && storage->depth->Description().stencil && !state.depth.stencil) {
        preservedStencil = state;
        preservedStencil->depth.format = storage->depth->Description().format;
        preservedStencil->depth.stencil = true;
        preservedStencil->depth.stencilAddress = storage->depth->Description().stencilAddress;
    }
    storage->pipeline = context.graphicsPipelines->Get(preservedStencil ? *preservedStencil : state, storage->color, storage->extraColors, storage->depth, *resources, shaders);
    auto& pipeline = *storage->pipeline;
    timing.Mark("pipeline_cache");
    RenderPassKey passKey;
    if (storage->color) passKey.views[passKey.viewCount++] = storage->color->Target().View();
    for (const auto& extra : storage->extraColors) passKey.views[passKey.viewCount++] = extra->Target().View();
    if (storage->depth) passKey.views[passKey.viewCount++] = storage->depth->View();
    passKey.extent = state.renderExtent;
    const bool clears = state.hasDepthTarget && (state.depthState.clearDepth || (state.depth.stencil && state.depthState.clearStencil));
    const bool depthWrites = WritesDepthStencil(state.depthState);
    const bool writes = resources->Writes();
    if (static const bool traceWrites = std::getenv("ANYPS5_TRACE_WAITS") != nullptr; traceWrites && writes) {
        static int reported = 0;
        if (reported++ < 400) {
            std::string ranges;
            for (const auto& [begin, end] : resources->WriteRanges()) {
                char item[64];
                std::snprintf(item, sizeof(item), " 0x%llx+0x%llx", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin));
                ranges += item;
            }
            std::uint64_t hash = 1469598103934665603ull;
            for (const auto& shader : shaders) for (const auto word : shader.program->spirv) hash = (hash ^ word) * 1099511628211ull;
            std::fprintf(stderr, "[draw-writes] %u %s indices, target 0x%llx %ux%u vk%u, shaders %016llx, writes%s\n", draw.indexCount, draw.indexed ? "indexed" : "auto", state.hasColorTarget ? static_cast<unsigned long long>(state.color.address) : 0ull, state.renderExtent.width, state.renderExtent.height, state.hasColorTarget ? static_cast<unsigned>(state.color.format) : 0u, static_cast<unsigned long long>(hash), ranges.c_str());
            std::fflush(stderr);
        }
    }
    static const bool mergePasses = std::getenv("ANYPS5_NO_PASS_MERGE") == nullptr;
    const bool attached = (!storage->color || storage->color->Attached()) && std::all_of(storage->extraColors.begin(), storage->extraColors.end(), [](const auto& extra) { return extra->Attached(); }) && (!storage->depth || storage->depth->Attached());
    VkCommandBuffer commands = mergePasses && !clears && !writes && attached ? context.drawQueue->ContinuePass(passKey) : VK_NULL_HANDLE;
    if (commands != VK_NULL_HANDLE) {
        if (storage->color) storage->color->Continue();
        for (const auto& extra : storage->extraColors) extra->Continue();
        if (storage->depth) storage->depth->Continue(depthWrites);
    } else {
        commands = context.drawQueue->Begin(context);
        VkMemoryBarrier upload{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        upload.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        upload.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | shaderStages, 0, 1, &upload, 0, nullptr, 0, nullptr);
        if (storage->color) storage->color->Begin(commands);
        for (const auto& extra : storage->extraColors) extra->Begin(commands);
        if (storage->depth) storage->depth->Prepare(commands, depthWrites);
        pipeline.BeginPass(commands, state.renderExtent);
        context.drawQueue->OpenPass(context, passKey, state.hasColorTarget ? state.color.address : 0, state.hasDepthTarget ? state.depth.address : 0);
    }
    if (context.drawQueue->BoundPipeline() != pipeline.Handle()) {
        pipeline.Bind(commands);
        context.drawQueue->SetBoundPipeline(pipeline.Handle());
    }
    pipeline.SetViewport(commands, state);
    resources->Bind(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Layout());
    pipeline.PushConstants(commands, shaders);
    if (state.stages.mesh) {
        context.Function<PFN_vkCmdDrawMeshTasksEXT>("vkCmdDrawMeshTasksEXT")(commands, meshGroups, draw.instanceCount, 1);
    } else {
        if (!vertexHandles.empty()) context.Function<PFN_vkCmdBindVertexBuffers>("vkCmdBindVertexBuffers")(commands, 0, static_cast<std::uint32_t>(vertexHandles.size()), vertexHandles.data(), vertexOffsets.data());
        if (draw.indexed) {
            context.Function<PFN_vkCmdBindIndexBuffer>("vkCmdBindIndexBuffer")(commands, indexHandle, indexOffset, draw.indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
            context.Function<PFN_vkCmdDrawIndexed>("vkCmdDrawIndexed")(commands, draw.indexCount, draw.instanceCount, 0, static_cast<std::int32_t>(draw.firstVertex), draw.firstInstance);
        } else {
            context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, draw.indexCount, draw.instanceCount, draw.firstVertex, draw.firstInstance);
        }
    }
    if (GpuJournal::CommandLabel != nullptr) {
        char text[80];
        std::snprintf(text, sizeof(text), "draw 0x%llx n%u target 0x%llx", static_cast<unsigned long long>(GpuJournal::CurrentProgram), draw.indexCount, static_cast<unsigned long long>(state.hasColorTarget ? state.color.address : 0));
        GpuJournal::Label(commands, text);
    }
    if (writes || !mergePasses) context.drawQueue->EndPass();
    timing.Mark("command_record");
    const auto probedDepth = storage->depth;
    context.drawQueue->Enqueue(std::move(resources), std::move(storage));
    if (static const char* probe = std::getenv("ANYPS5_DEBUG_STENCIL_PROBE"); probe != nullptr && probedDepth && state.depth.address == std::strtoull(probe, nullptr, 16)) {
        static int probes = 0;
        if (probes++ < 600) {
            context.drawQueue->Flush();
            APS5_LOG_OUT("[stencil-probe] after %u indices (target 0x%llx, stencil %s ref %u ops %u/%u/%u wmask 0x%x, clear %d):%s", draw.indexCount, static_cast<unsigned long long>(state.hasColorTarget ? state.color.address : 0), state.depthState.stencilTest ? "on" : "off", state.depthState.front.reference, static_cast<unsigned>(state.depthState.front.failOp), static_cast<unsigned>(state.depthState.front.passOp), static_cast<unsigned>(state.depthState.front.depthFailOp), state.depthState.front.writeMask, state.depthState.clearStencil ? 1 : 0, context.renderCache->DescribeStencil(*probedDepth).c_str());
        }
    }
    if (reportedTarget) {
        context.drawQueue->Flush();
        context.drawQueue->Wait();
        try {
            Texture view(context, reportedTarget, reportedView, VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY}, Texture::DirectView{});
            APS5_LOG_OUT("[draw-target]   after the draw:%s", context.textureCache->DescribeContents(view).c_str());
            const auto after = context.textureCache->Contents(view);
            const auto texel = view.GuestTexelBytes();
            const auto width = view.Extent().width;
            std::size_t inside = 0, changed = 0;
            std::string first;
            if (texel != 0 && after.size() == reportedBefore.size()) {
                for (std::uint32_t y = reportedScissor.offset.y; y < reportedScissor.offset.y + reportedScissor.extent.height && y < view.Extent().height; ++y) {
                    for (std::uint32_t x = reportedScissor.offset.x; x < reportedScissor.offset.x + reportedScissor.extent.width && x < width; ++x) {
                        const std::size_t at = (static_cast<std::size_t>(y) * width + x) * texel;
                        ++inside;
                        if (std::memcmp(after.data() + at, reportedBefore.data() + at, texel) == 0) continue;
                        if (++changed <= 3) {
                            char item[64]; std::snprintf(item, sizeof(item), " @%u,%u ", x, y); first += item;
                            for (std::uint32_t b = texel; b-- > 0;) { std::snprintf(item, sizeof(item), "%02x", reportedBefore[at + b]); first += item; }
                            first += "->";
                            for (std::uint32_t b = texel; b-- > 0;) { std::snprintf(item, sizeof(item), "%02x", after[at + b]); first += item; }
                        }
                    }
                }
            }
            APS5_LOG_OUT("[draw-target]   scissor changed %zu of %zu pixels%s", changed, inside, first.c_str());
        } catch (const std::exception& error) {
            APS5_LOG_OUT("[draw-target]   after the draw: %s", error.what());
        }
    }
    timing.Mark("enqueue");
}

}

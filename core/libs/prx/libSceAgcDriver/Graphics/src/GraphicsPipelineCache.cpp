#include "prx/libSceAgcDriver/Graphics/include/GraphicsPipelineCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlowPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include <chrono>
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <type_traits>
#include <algorithm>
#include <iterator>

namespace AgcDriver::Graphics {
namespace {

template<typename TValue>
void append(std::string& key, const TValue& value) {
    static_assert(std::is_trivially_copyable_v<TValue>);
    key.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

std::string makeKey(const Context& context, const State& state, const std::shared_ptr<ResidentColor>& target, std::span<const std::shared_ptr<ResidentColor>> extraTargets, const std::shared_ptr<DepthImage>& depth, const ShaderResources& resources, std::span<const CompiledShader> shaders) {
    PerformanceTimer timing("Graphics.PipelineKey");
    std::string key;
    key.reserve(512);
    append(key, target ? target->Target().View() : VK_NULL_HANDLE);
    append(key, state.hasColorTarget);
    append(key, static_cast<std::uint32_t>(extraTargets.size()));
    for (std::size_t i = 0; i < extraTargets.size(); ++i) {
        append(key, extraTargets[i]->Target().View());
        append(key, state.extraColors[i].format);
        append(key, state.extraBlends[i]);
    }
    append(key, depth ? depth->View() : VK_NULL_HANDLE);
    append(key, state.hasDepthTarget);
    if (state.hasDepthTarget) {
        append(key, state.depth.format);
        const auto& ds = state.depthState;
        append(key, ds.test); append(key, ds.write); append(key, ds.compare); append(key, ds.stencilTest);
        append(key, ds.front); append(key, ds.back);
        append(key, ds.clearDepth); append(key, ds.depthClear); append(key, ds.clearStencil); append(key, ds.stencilClear);
    }
    append(key, state.rectList);
    append(key, state.renderExtent.width);
    append(key, state.renderExtent.height);
    append(key, state.topology);
    append(key, state.viewport.x);
    append(key, state.viewport.y);
    append(key, state.viewport.width);
    append(key, state.viewport.height);
    append(key, state.viewport.minDepth);
    append(key, state.viewport.maxDepth);
    append(key, state.negativeOneToOne);
    append(key, state.scissor.offset.x);
    append(key, state.scissor.offset.y);
    append(key, state.scissor.extent.width);
    append(key, state.scissor.extent.height);
    append(key, state.cullMode);
    append(key, state.frontFace);
    append(key, state.depthClamp);
    append(key, state.depthBias);
    append(key, state.depthBiasConstant);
    append(key, state.depthBiasSlope);
    append(key, state.blend.blendEnable);
    append(key, state.blend.srcColorBlendFactor);
    append(key, state.blend.dstColorBlendFactor);
    append(key, state.blend.colorBlendOp);
    append(key, state.blend.srcAlphaBlendFactor);
    append(key, state.blend.dstAlphaBlendFactor);
    append(key, state.blend.alphaBlendOp);
    append(key, state.blend.colorWriteMask);
    for (const auto value : state.blendConstants) append(key, value);
    append(key, state.stages.mesh.has_value());
    append(key, state.stages.tessellation.has_value());
    if (state.stages.mesh) {
        const auto& mesh = *state.stages.mesh;
        append(key, mesh.inputPrimitive);
        append(key, mesh.primitivesPerGroup);
        append(key, mesh.verticesPerGroup);
        append(key, mesh.maxVertices);
        append(key, mesh.maxPrimitives);
        append(key, mesh.threadsPerGroup);
        append(key, mesh.ldsSizeDwords);
        append(key, mesh.provokingVertex);
    }
    if (state.stages.tessellation) {
        const auto& tessellation = *state.stages.tessellation;
        append(key, tessellation.inputControlPoints);
        append(key, tessellation.outputControlPoints);
        append(key, tessellation.domain);
        append(key, tessellation.partitioning);
        append(key, tessellation.outputTopology);
    }
    timing.Mark("state");
    const auto input = BuildVertexInputLayout(context, shaders.front().program->vertexAttributes);
    timing.Mark("vertex_layout");
    append(key, input.bindings.size());
    for (const auto& binding : input.bindings) {
        append(key, binding.binding);
        append(key, binding.stride);
        append(key, binding.inputRate);
    }
    append(key, input.attributes.size());
    for (const auto& attribute : input.attributes) {
        append(key, attribute.location);
        append(key, attribute.binding);
        append(key, attribute.format);
        append(key, attribute.offset);
    }
    append(key, resources.LayoutKey().size());
    const auto layout = std::span(resources.LayoutKey());
    if (!layout.empty()) key.append(reinterpret_cast<const char*>(layout.data()), layout.size_bytes());
    append(key, PushConstantStages(shaders));
    append(key, shaders.size());
    timing.Mark("resources");
    std::uint64_t shaderBytes = 0;
    for (const auto& shader : shaders) {
        append(key, shader.stage);
        append(key, shader.program->spirv.size());
        if (shader.program->spirvHash != 0) {
            append(key, shader.program->spirvHash);
            continue;
        }
        const auto bytes = std::as_bytes(std::span(shader.program->spirv));
        if (!bytes.empty()) key.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        shaderBytes += bytes.size();
    }
    timing.Mark("shader_code", shaderBytes);
    return key;
}

}

std::shared_ptr<Pipeline> GraphicsPipelineCache::Get(const State& state, const std::shared_ptr<ResidentColor>& target, std::span<const std::shared_ptr<ResidentColor>> extraTargets, const std::shared_ptr<DepthImage>& depth, const ShaderResources& resources, std::span<const CompiledShader> shaders) {
    PerformanceTimer timing("Graphics.PipelineCache");
    auto key = makeKey(context, state, target, extraTargets, depth, resources, shaders);
    timing.Mark("key");
    const auto found = lookup.find(key);
    if (found != lookup.end()) {
        const auto it = found->second;
        auto pipeline = it->pipeline;
        entries.splice(entries.end(), entries, it);
        timing.Mark("hit");
        return pipeline;
    }
    timing.Mark("miss");
    const auto creationStart = std::chrono::steady_clock::now();
    std::vector<const RenderTarget*> extraViews;
    for (const auto& extra : extraTargets) extraViews.push_back(&extra->Target());
    auto pipeline = std::make_shared<Pipeline>(context, state, target ? &target->Target() : nullptr, extraViews, depth.get(), resources, shaders);
    timing.Mark("create");
    ReportSlowPipeline("graphics", creationStart, shaders);
    if (context.pipelineCacheOwner && std::chrono::steady_clock::now() - creationStart > std::chrono::milliseconds(100)) context.pipelineCacheOwner->Save();
    entries.push_back({std::move(key), target, std::vector<std::shared_ptr<ResidentColor>>(extraTargets.begin(), extraTargets.end()), depth, pipeline});
    try {
        const auto it = std::prev(entries.end());
        Require(lookup.emplace(it->key, it).second, "duplicate graphics pipeline cache key");
    } catch (...) {
        entries.pop_back();
        throw;
    }
    while (entries.size() > 128) {
        const auto it = std::find_if(entries.begin(), entries.end(), [](const auto& entry) { return entry.pipeline.use_count() == 1; });
        if (it == entries.end()) break;
        lookup.erase(it->key);
        entries.erase(it);
    }
    return pipeline;
}

}

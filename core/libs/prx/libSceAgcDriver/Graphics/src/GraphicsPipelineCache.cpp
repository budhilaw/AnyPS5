#include "prx/libSceAgcDriver/Graphics/include/GraphicsPipelineCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/KeyHash.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlowPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include <chrono>
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/SlowOperation.hpp"
#include <type_traits>
#include <algorithm>
#include <array>
#include <bit>

namespace AgcDriver::Graphics {
namespace {

template<typename THandle>
std::uint64_t handleBits(THandle handle) {
    if constexpr (std::is_pointer_v<THandle>) return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
    else return static_cast<std::uint64_t>(handle);
}

}

std::uint64_t GraphicsPipelineCache::Key(const Context& context, const State& state, VkImageView target, std::span<const VkImageView> extraTargets, VkImageView depth, std::span<const std::uint32_t> layoutKey, std::span<const CompiledShader> shaders, std::vector<std::uint64_t>& words) {
    Require(extraTargets.size() == state.extraColors.size() && state.extraBlends.size() == state.extraColors.size(), "extra render targets do not match decoded color state");
    Require(!shaders.empty() && shaders.front().program != nullptr, "missing compiled shader");
    words.clear();
    auto hash = KeyHashSeed;
    const auto add = [&](std::uint64_t value) {
        words.push_back(value);
        hash = MixKeyHash(hash, value);
    };
    const auto addFloat = [&](float value) { add(std::bit_cast<std::uint32_t>(value)); };
    const auto addBlend = [&](const VkPipelineColorBlendAttachmentState& blend) {
        add(blend.blendEnable);
        add(blend.srcColorBlendFactor);
        add(blend.dstColorBlendFactor);
        add(blend.colorBlendOp);
        add(blend.srcAlphaBlendFactor);
        add(blend.dstAlphaBlendFactor);
        add(blend.alphaBlendOp);
        add(blend.colorWriteMask);
    };
    const auto addStencil = [&](const VkStencilOpState& face) {
        add(face.failOp);
        add(face.passOp);
        add(face.depthFailOp);
        add(face.compareOp);
        add(face.compareMask);
        add(face.writeMask);
        add(face.reference);
    };
    const auto addPacked = [&](std::span<const std::uint32_t> values) {
        add(values.size());
        for (std::size_t index = 0; index < values.size(); index += 2) add(values[index] | (index + 1 < values.size() ? static_cast<std::uint64_t>(values[index + 1]) << 32u : 0u));
    };
    add(handleBits(target));
    add(state.hasColorTarget);
    add(extraTargets.size());
    for (std::size_t i = 0; i < extraTargets.size(); ++i) {
        add(handleBits(extraTargets[i]));
        add(state.extraColors[i].format);
        addBlend(state.extraBlends[i]);
    }
    add(handleBits(depth));
    add(state.hasDepthTarget);
    if (state.hasDepthTarget) {
        add(state.depth.format);
        const auto& ds = state.depthState;
        add(ds.test); add(ds.write); add(ds.compare); add(ds.stencilTest);
        addStencil(ds.front); addStencil(ds.back);
        add(ds.clearDepth); addFloat(ds.depthClear); add(ds.clearStencil); add(ds.stencilClear);
    }
    add(state.rectList);
    add(state.renderExtent.width);
    add(state.renderExtent.height);
    add(state.topology);
    add(state.negativeOneToOne);
    add(state.cullMode);
    add(state.frontFace);
    add(state.depthClamp);
    add(state.depthBias);
    addFloat(state.depthBiasConstant);
    addFloat(state.depthBiasSlope);
    addBlend(state.blend);
    for (const auto value : state.blendConstants) addFloat(value);
    add(state.stages.mesh.has_value());
    add(state.stages.tessellation.has_value());
    if (state.stages.mesh) {
        const auto& mesh = *state.stages.mesh;
        add(mesh.inputPrimitive);
        add(mesh.primitivesPerGroup);
        add(mesh.verticesPerGroup);
        add(mesh.maxVertices);
        add(mesh.maxPrimitives);
        add(mesh.threadsPerGroup);
        add(mesh.ldsSizeDwords);
        add(mesh.provokingVertex);
    }
    if (state.stages.tessellation) {
        const auto& tessellation = *state.stages.tessellation;
        add(tessellation.inputControlPoints);
        add(tessellation.outputControlPoints);
        add(tessellation.domain);
        add(tessellation.partitioning);
        add(tessellation.outputTopology);
    }
    const auto& attributes = shaders.front().program->vertexAttributes;
    add(attributes.size());
    if (!attributes.empty()) {
        const auto input = BuildVertexInputLayout(context, attributes);
        add(input.bindings.size());
        for (const auto& binding : input.bindings) {
            add(binding.binding);
            add(binding.stride);
            add(binding.inputRate);
        }
        add(input.attributes.size());
        for (const auto& attribute : input.attributes) {
            add(attribute.location);
            add(attribute.binding);
            add(attribute.format);
            add(attribute.offset);
        }
    }
    addPacked(layoutKey);
    add(PushConstantStages(shaders));
    add(shaders.size());
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        add(static_cast<std::uint64_t>(shader.stage));
        add(shader.program->spirv.size());
        add(shader.program->spirvHash != 0);
        if (shader.program->spirvHash != 0) add(shader.program->spirvHash);
        else addPacked(shader.program->spirv.Words());
    }
    return FinishKeyHash(hash);
}

std::shared_ptr<Pipeline> GraphicsPipelineCache::Get(const State& state, const std::shared_ptr<ResidentColor>& target, std::span<const std::shared_ptr<ResidentColor>> extraTargets, const std::shared_ptr<DepthImage>& depth, const ShaderResources& resources, std::span<const CompiledShader> shaders) {
    PerformanceTimer timing("Graphics.PipelineCache");
    Require(extraTargets.size() <= MaxExtraTargets, "a draw binds more color targets than a pipeline supports");
    std::array<VkImageView, MaxExtraTargets> extraViews{};
    for (std::size_t i = 0; i < extraTargets.size(); ++i) extraViews[i] = extraTargets[i]->Target().View();
    const auto hash = Key(context, state, target ? target->Target().View() : VK_NULL_HANDLE, std::span<const VkImageView>(extraViews.data(), extraTargets.size()), depth ? depth->View() : VK_NULL_HANDLE, resources.LayoutKey(), shaders, key);
    timing.Mark("key");
    if (memoized && memo->key == hash && memo->value.key == key) {
        entries.Touch(memo);
        timing.Mark("memo_hit");
        return memo->value.pipeline;
    }
    memoized = false;
    if (const auto found = entries.Find(hash); found != entries.End()) {
        if (found->value.key == key) {
            memo = found;
            memoized = true;
            timing.Mark("hit");
            return found->value.pipeline;
        }
        entries.Erase(found);
    }
    timing.Mark("miss");
    const auto creationStart = std::chrono::steady_clock::now();
    std::vector<const RenderTarget*> extraTargetViews;
    for (const auto& extra : extraTargets) extraTargetViews.push_back(&extra->Target());
    std::shared_ptr<Pipeline> pipeline;
    {
        SlowOperationTimer creation("graphics pipeline create");
        pipeline = std::make_shared<Pipeline>(context, state, target ? &target->Target() : nullptr, extraTargetViews, depth.get(), resources, shaders);
    }
    timing.Mark("create");
    ReportSlowPipeline("graphics", creationStart, shaders);
    if (context.pipelineCacheOwner) context.pipelineCacheOwner->NoteCreated();
    entries.EraseIf([](const auto& entry) {
        const auto& cached = entry.value;
        if (cached.pipeline.use_count() != 1) return false;
        if (cached.target && cached.target.use_count() == 1) return true;
        if (cached.depth && cached.depth.use_count() == 1) return true;
        return std::any_of(cached.extraTargets.begin(), cached.extraTargets.end(), [](const auto& extra) { return extra.use_count() == 1; });
    });
    const auto [position, inserted] = entries.Insert(hash, Entry{key, target, std::vector<std::shared_ptr<ResidentColor>>(extraTargets.begin(), extraTargets.end()), depth, pipeline}, [](const auto&) {
        SlowOperationRecord_nid_no_patch("graphics pipeline evict", 0);
    });
    Require(inserted, "duplicate graphics pipeline cache key");
    memo = position;
    memoized = true;
    return pipeline;
}

}

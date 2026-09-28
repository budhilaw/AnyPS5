#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {

Pipeline::Pipeline(const Context& context, const State& state, const RenderTarget* target, std::span<const RenderTarget* const> extraTargets, const DepthImage* depth, const ShaderResources& resources, std::span<const CompiledShader> shaders) : context(context), _modules(shaders.size()) {
    Require((target != nullptr) == state.hasColorTarget, "render target does not match decoded color state");
    Require(extraTargets.size() == state.extraColors.size() && state.extraBlends.size() == state.extraColors.size(), "extra render targets do not match decoded color state");
    Require((depth != nullptr) == state.hasDepthTarget, "depth target does not match decoded depth state");
    Require(state.renderExtent.width != 0 && state.renderExtent.height != 0 && state.renderExtent.width <= context.limits.maxFramebufferWidth && state.renderExtent.height <= context.limits.maxFramebufferHeight, "framebuffer extent exceeds device limits");
    Require(state.hasColorTarget || (context.limits.framebufferNoAttachmentsSampleCounts & VK_SAMPLE_COUNT_1_BIT) != 0, "device does not support single-sample rendering without attachments");
    ValidateShaders(shaders, state, context.subgroup, context.fragmentShaderBarycentric);
    Require(!state.negativeOneToOne || context.depthClipControl, "negative-one-to-one depth clipping requires VK_EXT_depth_clip_control with depthClipControl enabled");
    if (state.rectList) Require(context.tessellationShader && context.limits.maxTessellationPatchSize >= 4, "rect-list requires tessellation with four output control points");
    if (state.stages.tessellation) {
        Require(context.tessellationShader, "device does not support tessellation shaders");
        Require(state.stages.tessellation->inputControlPoints <= context.limits.maxTessellationPatchSize && state.stages.tessellation->outputControlPoints <= context.limits.maxTessellationPatchSize, "tessellation patch exceeds device limits");
    }
    if (state.stages.mesh) {
        Require(context.meshShader, "device does not support VK_EXT_mesh_shader");
        const auto& mesh = *state.stages.mesh;
        Require(mesh.threadsPerGroup <= context.meshLimits.maxMeshWorkGroupInvocations && mesh.threadsPerGroup <= context.meshLimits.maxMeshWorkGroupSize[0], "mesh workgroup exceeds device limits");
        Require(mesh.maxVertices <= context.meshLimits.maxMeshOutputVertices && mesh.maxPrimitives <= context.meshLimits.maxMeshOutputPrimitives && static_cast<std::uint64_t>(mesh.ldsSizeDwords) * 4 <= context.meshLimits.maxMeshSharedMemorySize, "mesh output or LDS exceeds device limits");
    }
    const auto& viewport = state.viewport;
    Require(std::isfinite(viewport.minDepth) && std::isfinite(viewport.maxDepth), "non-finite viewport depth range");
    Require(context.depthRangeUnrestricted || (viewport.minDepth >= 0 && viewport.minDepth <= 1 && viewport.maxDepth >= 0 && viewport.maxDepth <= 1), "viewport depth [" + std::to_string(viewport.minDepth) + ", " + std::to_string(viewport.maxDepth) + "] outside [0, 1] requires VK_EXT_depth_range_unrestricted (negativeOneToOne=" + std::to_string(state.negativeOneToOne) + ")");
    Require(std::isfinite(viewport.x) && std::isfinite(viewport.y) && std::isfinite(viewport.width) && std::isfinite(viewport.height), "viewport arithmetic overflow");
    Require(viewport.width <= context.limits.maxViewportDimensions[0] && std::abs(viewport.height) <= context.limits.maxViewportDimensions[1], "viewport dimensions exceed device limits");
    if (!(viewport.x >= context.limits.viewportBoundsRange[0] && viewport.x + viewport.width <= context.limits.viewportBoundsRange[1]) || !(std::min(viewport.y, viewport.y + viewport.height) >= context.limits.viewportBoundsRange[0] && std::max(viewport.y, viewport.y + viewport.height) <= context.limits.viewportBoundsRange[1])) {
        throw std::runtime_error("AGC graphics: viewport exceeds device bounds: x=" + std::to_string(viewport.x) + " y=" + std::to_string(viewport.y) + " width=" + std::to_string(viewport.width) + " height=" + std::to_string(viewport.height) + " bounds [" + std::to_string(context.limits.viewportBoundsRange[0]) + ", " + std::to_string(context.limits.viewportBoundsRange[1]) + "] render extent " + std::to_string(state.renderExtent.width) + "x" + std::to_string(state.renderExtent.height));
    }
    const auto pushStages = PushConstantStages(shaders);
    Require(pushStages == 0 || context.limits.maxPushConstantsSize >= PipelinePushConstantBytes, "graphics push constant range exceeds device limit");
    try {
        std::vector<VkPipelineShaderStageCreateInfo> stages(shaders.size());
        for (std::uint32_t i = 0; i < shaders.size(); ++i) {
            const auto& shader = *shaders[i].program;
            VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            module.codeSize = shader.spirv.size() * sizeof(std::uint32_t);
            module.pCode = shader.spirv.data();
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &module, nullptr, &_modules[i]), "vkCreateShaderModule graphics");
            const auto stage = VulkanStage(shaders[i].stage);
            stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].stage = stage;
            stages[i].module = _modules[i];
            stages[i].pName = "main";
        }
        const auto setLayout = resources.Layout();
        const VkPushConstantRange push{pushStages, 0, PipelinePushConstantBytes};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &setLayout;
        layoutInfo.pushConstantRangeCount = pushStages != 0 ? 1 : 0;
        layoutInfo.pPushConstantRanges = pushStages != 0 ? &push : nullptr;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout graphics");
        const auto colorAttachment = [](VkFormat format) {
            VkAttachmentDescription color{};
            color.format = format;
            color.samples = VK_SAMPLE_COUNT_1_BIT;
            color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            return color;
        };
        const std::uint32_t colorCount = state.hasColorTarget ? 1u + static_cast<std::uint32_t>(state.extraColors.size()) : 0u;
        std::vector<VkAttachmentReference> references;
        for (std::uint32_t i = 0; i < colorCount; ++i) references.push_back({i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        // The depth attachment follows the color one; a depth clear requested through
        // DB_RENDER_CONTROL clears the whole surface at render pass start.
        VkAttachmentDescription depthAttachment{};
        const auto& ds = state.depthState;
        depthAttachment.format = state.depth.format;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = ds.clearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = !state.depth.stencil ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : ds.clearStencil ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.stencilStoreOp = state.depth.stencil ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        const std::uint32_t depthIndex = colorCount;
        VkAttachmentReference depthReference{depthIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkAttachmentDescription attachments[9];
        std::uint32_t attachmentCount = 0;
        if (state.hasColorTarget) attachments[attachmentCount++] = colorAttachment(state.color.format);
        for (const auto& extra : state.extraColors) attachments[attachmentCount++] = colorAttachment(extra.format);
        if (state.hasDepthTarget) attachments[attachmentCount++] = depthAttachment;
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = colorCount;
        subpass.pColorAttachments = colorCount != 0 ? references.data() : nullptr;
        subpass.pDepthStencilAttachment = state.hasDepthTarget ? &depthReference : nullptr;
        VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        passInfo.attachmentCount = attachmentCount;
        passInfo.pAttachments = attachmentCount != 0 ? attachments : nullptr;
        passInfo.subpassCount = 1;
        passInfo.pSubpasses = &subpass;
        Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &renderPass), "vkCreateRenderPass");
        VkImageView views[9];
        std::uint32_t viewCount = 0;
        if (state.hasColorTarget) views[viewCount++] = target->View();
        for (const auto* extra : extraTargets) views[viewCount++] = extra->View();
        if (state.hasDepthTarget) views[viewCount++] = depth->View();
        clearValueCount = attachmentCount;
        if (state.hasDepthTarget) {
            clearValues[depthIndex].depthStencil = {ds.depthClear, ds.stencilClear};
        }
        VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = viewCount;
        framebufferInfo.pAttachments = viewCount != 0 ? views : nullptr;
        framebufferInfo.width = state.renderExtent.width;
        framebufferInfo.height = state.renderExtent.height;
        framebufferInfo.layers = 1;
        Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        const auto vertexInput = BuildVertexInputLayout(context, shaders.front().program->vertexAttributes);
        input.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vertexInput.bindings.size());
        input.pVertexBindingDescriptions = vertexInput.bindings.data();
        input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertexInput.attributes.size());
        input.pVertexAttributeDescriptions = vertexInput.attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = state.topology;
        VkPipelineViewportStateCreateInfo viewports{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        VkPipelineViewportDepthClipControlCreateInfoEXT depthClip{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT};
        depthClip.negativeOneToOne = state.negativeOneToOne;
        if (state.negativeOneToOne) viewports.pNext = &depthClip;
        viewports.viewportCount = 1;
        viewports.pViewports = &state.viewport;
        viewports.scissorCount = 1;
        viewports.pScissors = &state.scissor;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = state.cullMode;
        raster.frontFace = state.frontFace;
        raster.lineWidth = 1;
        Require(!state.depthClamp || context.depthClamp, "depth clamping requires the depthClamp device feature");
        raster.depthClampEnable = state.depthClamp ? VK_TRUE : VK_FALSE;
        raster.depthBiasEnable = state.depthBias ? VK_TRUE : VK_FALSE;
        raster.depthBiasConstantFactor = state.depthBiasConstant;
        raster.depthBiasSlopeFactor = state.depthBiasSlope;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = ds.test || ds.write;
        depthStencil.depthWriteEnable = ds.write;
        depthStencil.depthCompareOp = ds.test ? ds.compare : VK_COMPARE_OP_ALWAYS;
        depthStencil.stencilTestEnable = ds.stencilTest;
        depthStencil.front = ds.front;
        depthStencil.back = ds.back;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        std::vector<VkPipelineColorBlendAttachmentState> blendStates;
        if (state.hasColorTarget) blendStates.push_back(state.blend);
        blendStates.insert(blendStates.end(), state.extraBlends.begin(), state.extraBlends.end());
        blend.attachmentCount = static_cast<std::uint32_t>(blendStates.size());
        blend.pAttachments = blendStates.empty() ? nullptr : blendStates.data();
        std::copy(state.blendConstants.begin(), state.blendConstants.end(), blend.blendConstants);
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        VkPipelineTessellationStateCreateInfo tessellation{VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};
        if (state.rectList || state.stages.tessellation) {
            tessellation.patchControlPoints = state.rectList ? 3u : state.stages.tessellation->inputControlPoints;
            pipelineInfo.pTessellationState = &tessellation;
        }
        pipelineInfo.pVertexInputState = state.stages.mesh ? nullptr : &input;
        pipelineInfo.pInputAssemblyState = state.stages.mesh ? nullptr : &assembly;
        pipelineInfo.pViewportState = &viewports;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &samples;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDepthStencilState = state.hasDepthTarget ? &depthStencil : nullptr;
        pipelineInfo.layout = layout;
        pipelineInfo.renderPass = renderPass;
        Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateGraphicsPipelines");
    } catch (...) {
        release();
        throw;
    }
}

Pipeline::~Pipeline() {
    release();
}

void Pipeline::release() noexcept {
    if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
    if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
    if (renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, renderPass, nullptr);
    if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
    for (auto module : _modules) {
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
    }
}

VkPipelineLayout Pipeline::Layout() const {
    return layout;
}

void Pipeline::BeginPass(VkCommandBuffer commands, VkExtent2D extent) const {
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = renderPass;
    begin.framebuffer = framebuffer;
    begin.renderArea = {{0, 0}, extent};
    begin.clearValueCount = clearValueCount;
    begin.pClearValues = clearValues;
    context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
}

void Pipeline::Bind(VkCommandBuffer commands) const {
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void Pipeline::PushConstants(VkCommandBuffer commands, std::span<const CompiledShader> shaders) const {
    const auto stages = PushConstantStages(shaders);
    if (stages == 0) return;
    const auto bytes = AssemblePushConstants(shaders);
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, layout, stages, 0, PipelinePushConstantBytes, bytes.data());
}

}

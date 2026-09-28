#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINE_HPP

#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"

namespace AgcDriver::Graphics {

class Pipeline {
public:
    Pipeline(const Context& context, const State& state, const RenderTarget* target, std::span<const RenderTarget* const> extraTargets, const DepthImage* depth, const ShaderResources& resources, std::span<const CompiledShader> shaders);
    ~Pipeline();
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    VkPipelineLayout Layout() const;
    // Begins a render pass instance on the pipeline's attachments; pipelines with the same
    // attachment formats are compatible with it and may draw in it.
    void BeginPass(VkCommandBuffer commands, VkExtent2D extent) const;
    void Bind(VkCommandBuffer commands) const;
    VkPipeline Handle() const { return pipeline; }
private:
    VkClearValue clearValues[9]{};
    std::uint32_t clearValueCount = 0;
public:
    void PushConstants(VkCommandBuffer commands, std::span<const CompiledShader> shaders) const;

private:
    void release() noexcept;
    Context context;
    std::vector<VkShaderModule> _modules;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

void ValidateShaderPair(const ShaderRecompiler::RecompileResult& vertex, const ShaderRecompiler::RecompileResult& fragment);
void ValidateShaders(std::span<const CompiledShader> shaders, const State& state, const VkPhysicalDeviceSubgroupProperties& subgroup, bool fragmentShaderBarycentric);

}

#endif

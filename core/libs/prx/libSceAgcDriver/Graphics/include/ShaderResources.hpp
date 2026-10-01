#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERRESOURCES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERRESOURCES_HPP

#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DescriptorCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DispatchBindings.hpp"
#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace AgcDriver::Graphics {

class ShaderResources {
public:
    const std::vector<std::shared_ptr<Texture>>& Textures() const { return textures; }
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& TextureBindings() const { return textureBindings; }
    std::string DescribeTextures() const;
    ShaderResources(const Context& context, const ShaderRecompiler::RecompileResult& vertex, const ShaderRecompiler::RecompileResult& fragment, const ColorTarget& target, std::uint64_t indexAddress, std::size_t indexBytes);
    ShaderResources(const Context& context, std::span<const CompiledShader> shaders, const ColorTarget& target, std::uint64_t indexAddress, std::size_t indexBytes, std::span<const GuestMemorySnapshot> snapshots = {}, const DepthImage* attachedDepth = nullptr);
    ShaderResources(const Context& context, const CompiledShader& compute, std::span<const GuestMemorySnapshot> snapshots = {});
    ~ShaderResources();
    ShaderResources(const ShaderResources&) = delete;
    ShaderResources& operator=(const ShaderResources&) = delete;
    VkDescriptorSetLayout Layout() const;
    void Bind(VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout) const;
    void WriteBack(std::uint64_t sequence = std::numeric_limits<std::uint64_t>::max());
    bool WritesOverlapCopied(std::uint64_t address, std::size_t bytes) const { return guestMemory.WritesOverlapCopied(address, bytes); }
    bool WriteCopied(std::uint64_t address) const { return guestMemory.WriteCopied(address); }
    bool WritesOverlap(std::uint64_t address, std::size_t bytes) const { return guestMemory.WritesOverlap(address, bytes); }
    const std::vector<std::pair<std::uint64_t, std::uint64_t>>& WriteRanges() const { return guestMemory.WriteRanges(); }
    bool HasGuestWrites() const { return guestMemory.HasWrites(); }
    void RecordWriteThrough(VkCommandBuffer commands) const { guestMemory.RecordWriteThrough(commands); }
    bool Writes() const { return guestMemory.HasWrites() || storesImages || usesFaultBuffer; }
    bool UsesGds() const { return usesGds; }
    const std::vector<std::uint32_t>& LayoutKey() const { return layoutKey; }
    DispatchBindings CaptureBindings() const;
    static std::optional<std::vector<VkDescriptorSetLayoutBinding>> LayoutBindings(std::span<const CompiledShader> shaders);
    static std::vector<std::uint32_t> KeyOf(std::span<const VkDescriptorSetLayoutBinding> bindings);

private:
    struct Allocation {
        std::uint64_t address;
        std::size_t size;
        bool guest;
        std::unique_ptr<Buffer> buffer;
        ShaderRecompiler::DescriptorRole role = ShaderRecompiler::DescriptorRole::ShaderData;
        bool zero = false;
    };

    struct Binding {
        VkDescriptorSetLayoutBinding layout;
        std::vector<std::size_t> allocations;
        std::vector<std::size_t> imageAllocations;
    };

    void build(std::span<const CompiledShader> shaders, const ColorTarget* target, std::uint64_t indexAddress, std::size_t indexBytes, const DepthImage* attachedDepth);
    bool usableOptionalBuffer(std::span<const std::uint32_t> words) const;
    std::size_t addGuestBuffer(std::span<const std::uint32_t> words, bool written, bool read, const ColorTarget* target, std::uint64_t indexAddress, std::size_t indexBytes);
    std::size_t addDataBuffer(std::span<const std::uint32_t> words);
    std::size_t addZeroBuffer(std::uint64_t address, std::size_t size);
    void addImageBinding(const ShaderRecompiler::DescriptorBinding& binding, VkShaderStageFlags flags, std::vector<Binding>& bindings, const DepthImage* attachedDepth);
    void release() noexcept;
    void prepareAddressBindings(std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots);
    VkDescriptorBufferInfo descriptor(const Allocation& allocation) const;
    Context context;
    std::vector<std::uint32_t> layoutKey;
    GuestBufferMemory guestMemory;
    std::unique_ptr<BdaResources> bda;
    bool usesBda = false;
    bool usesFaultBuffer = false;
    bool usesGds = false;
    bool storesImages = false;
    VkDescriptorSetLayout _layout = VK_NULL_HANDLE;
    VkDescriptorSet _set = VK_NULL_HANDLE;
    DescriptorCache::Allocation descriptors;
    std::vector<Allocation> allocations;
    std::vector<Binding> boundBindings;
    std::vector<std::shared_ptr<Texture>> textures;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> textureBindings;
    std::vector<std::shared_ptr<Sampler>> samplers;
};

}

#endif

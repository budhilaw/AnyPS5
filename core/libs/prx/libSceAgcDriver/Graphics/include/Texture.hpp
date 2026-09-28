#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"

namespace AgcDriver::Graphics {

class ResidentColor;
class DepthImage;
class CommandBatch;
class Buffer;

// Slices of the surface in guest memory: array layers, cube faces, or the depth of a volume.
std::uint32_t FullArrayLayers(const GuestTextureResource& descriptor);

class Texture {
public:
    Texture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, VkComponentMapping components, std::span<const std::byte> snapshot);
    Texture(const Context& context, const std::shared_ptr<ResidentColor>& source, const GuestTextureResource& descriptor, VkComponentMapping components);
    // A view of a resident render target's own image (no copy): shader loads see the latest draws
    // and dispatches in queue order, and stores land in the target itself.
    struct DirectView {};
    Texture(const Context& context, const std::shared_ptr<ResidentColor>& source, const GuestTextureResource& descriptor, VkComponentMapping components, DirectView);
    bool IsDirectView() const { return directView; }
    // A view of another texture's image: the same guest surface seen through a different descriptor
    // (mip range, array range, sampler shape or swizzle). Shader stores through any view are visible
    // through all of them.
    Texture(const Context& context, const std::shared_ptr<Texture>& shared, const GuestTextureResource& descriptor, VkComponentMapping components);
    // A copy of a resident depth image, sampled as a single-channel texture.
    Texture(const Context& context, const std::shared_ptr<DepthImage>& source, bool stencil, const GuestTextureResource& descriptor, VkComponentMapping components);
    // A 1x1 transparent black image of the given dimension (the value of a null descriptor).
    Texture(const Context& context, TextureDimension dimension);
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    VkImageView View() const;
    // Frees the detiling buffers once the GPU work that uploads the texture has completed.
    void ReleaseUpload();
    // The image layout the texture rests in between uses (descriptors quote it).
    VkImageLayout Layout() const { return layout; }
    // A view for shader image loads and stores (identity swizzle, linear color space), or null when
    // the texture cannot be written by shaders (compressed formats, render target copies).
    VkImageView StorageView();
    VkDeviceSize AllocationBytes() const { return allocationBytes; }
    VkImage Image() const { return image; }
    VkExtent2D Extent() const { return extent; }
    std::uint64_t GuestAddress() const { return guestAddress; }
    VkFormat GuestFormat() const { return guestFormat; }
    bool Promotable() const { return promotable; }
    std::uint32_t GuestTexelBytes() const { return guestTexelBytes; }
    bool Guest2D() const { return guest2D; }
    std::uint32_t GuestLayers() const { return guestLayers; }
    std::uint32_t GuestMipCount() const { return guestMipCount; }
    std::uint32_t GuestDimension() const { return guestDimension; }
    // The texture whose image this one views (itself when it owns the image).
    bool SharesImage() const { return !ownsImage; }
    // Shader stores into a texture copied from a render target must reach the target: the copy
    // is transferred back when the work completes.
    void MarkStored();
    bool Stored() const { return stored; }
    void FlushStores();

private:
    void release() noexcept;
    void createGuestViews(const GuestTextureResource& descriptor, VkComponentMapping components, VkFormat vkFormat, bool storageCapable);

    Context context;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkImageView storageView = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkFormat storageFormat = VK_FORMAT_UNDEFINED;
    VkImageViewType storageViewType = VK_IMAGE_VIEW_TYPE_2D;
    VkImageSubresourceRange storageRange{};
    VkExtent2D extent{};
    bool stored = false;
    VkDeviceSize allocationBytes = 0;
    std::shared_ptr<ResidentColor> source;
    std::shared_ptr<DepthImage> depthSource;
    // The guest surface a detiled texture came from, for promoting shader stores to a resident
    // render target (zero address when not applicable).
    std::uint64_t guestAddress = 0;
    TextureTileMode guestTileMode = TextureTileMode::kLinear;
    VkFormat guestFormat = VK_FORMAT_UNDEFINED;
    bool promotable = false;
    std::uint32_t guestTexelBytes = 0;   // 0 for compressed formats
    bool guest2D = false;
    std::uint32_t guestMipCount = 0;
    std::uint32_t guestDimension = 0;
    std::uint32_t guestLayers = 0;
    std::uint32_t imageLayers = 0;
    std::shared_ptr<Texture> sharedImage;  // keeps the image owner alive for a view
    bool ownsImage = true;
    bool directView = false;
    std::unique_ptr<Buffer> staging;
    std::unique_ptr<Buffer> linear;
    std::unique_ptr<CommandBatch> upload;
};

}

#endif

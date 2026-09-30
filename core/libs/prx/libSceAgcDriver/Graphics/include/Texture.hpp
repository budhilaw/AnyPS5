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

std::uint32_t FullArrayLayers(const GuestTextureResource& descriptor);

class Texture {
public:
    Texture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, VkComponentMapping components, std::span<const std::byte> snapshot);
    Texture(const Context& context, const std::shared_ptr<ResidentColor>& source, const GuestTextureResource& descriptor, VkComponentMapping components);
    struct DirectView {};
    Texture(const Context& context, const std::shared_ptr<ResidentColor>& source, const GuestTextureResource& descriptor, VkComponentMapping components, DirectView);
    bool IsDirectView() const { return directView; }
    Texture(const Context& context, const std::shared_ptr<Texture>& shared, const GuestTextureResource& descriptor, VkComponentMapping components);
    Texture(const Context& context, std::span<const std::shared_ptr<DepthImage>> sources, bool stencil, const GuestTextureResource& descriptor, VkComponentMapping components, bool compare = false);
    Texture(const Context& context, TextureDimension dimension);
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    VkImageView View() const;
    void ReleaseUpload();
    VkImageLayout Layout() const { return layout; }
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
    bool SharesImage() const { return !ownsImage; }
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
    std::vector<std::shared_ptr<DepthImage>> depthSources;
    std::uint64_t guestAddress = 0;
    TextureTileMode guestTileMode = TextureTileMode::kLinear;
    VkFormat guestFormat = VK_FORMAT_UNDEFINED;
    bool promotable = false;
    std::uint32_t guestTexelBytes = 0;
    bool guest2D = false;
    std::uint32_t guestMipCount = 0;
    std::uint32_t guestDimension = 0;
    std::uint32_t guestLayers = 0;
    std::uint32_t imageLayers = 0;
    std::shared_ptr<Texture> sharedImage;
    bool ownsImage = true;
    bool directView = false;
    std::unique_ptr<Buffer> staging;
    std::unique_ptr<Buffer> linear;
    std::unique_ptr<CommandBatch> upload;
};

}

#endif

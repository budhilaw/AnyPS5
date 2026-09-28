#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURECACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURECACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include <array>
#include <list>
#include <map>
#include <memory>
#include <vector>

namespace AgcDriver::Graphics {

class TextureCache {
public:
    explicit TextureCache(const Context& context);
    std::shared_ptr<Texture> Get(std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components);
    // The texture a null descriptor of the given dimension reads as.
    std::shared_ptr<Texture> Null(TextureDimension dimension);
    // Diagnostics: writes every 2D single-level 4-byte texture as <prefix><address>.bmp.
    void DumpTextures(const std::string& prefix);
    // Diagnostics: nonzero texel count and the first nonzero texels of the texture's base level.
    std::string DescribeContents(const Texture& texture);

private:
    // A guest surface brought onto the device: the image (owned by the entry that detiled it) and
    // the guest bytes it was made from. Every descriptor naming the surface views the same image.
    struct Surface {
        GuestTextureResource identity;
        std::vector<std::byte> snapshot;
        std::uint64_t stamp = 0;  // guest memory change stamp of the snapshot (see GuestBufferCache::Track)
        std::weak_ptr<Texture> texture;  // the image owner
        bool stale = false;  // guest memory changed: every view is dropped
    };
    struct Entry {
        std::array<std::uint32_t, 8> descriptor;
        TextureDimension viewDimension;
        std::shared_ptr<Texture> texture;
        std::shared_ptr<Surface> surface;
        std::weak_ptr<ResidentColor> source;
        std::weak_ptr<DepthImage> depthSource;
        std::uint64_t generation = 0;
        std::uint64_t retained = 0;  // bytes this entry charges against the budget
    };
    void trim();
    std::list<Entry>::iterator eraseEntry(std::list<Entry>::iterator it);
    static bool SameSurface(const GuestTextureResource& a, const GuestTextureResource& b);
    Context context;
    std::list<Entry> entries;
    std::map<TextureDimension, std::shared_ptr<Texture>> nulls;
    std::uint64_t retainedBytes = 0;
    std::uint64_t budget;
    static constexpr std::size_t maxEntries = 8192;
};

}

#endif

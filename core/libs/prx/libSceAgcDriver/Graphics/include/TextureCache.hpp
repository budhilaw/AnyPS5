#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURECACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURECACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include <array>
#include <list>
#include <unordered_map>
#include <map>
#include <memory>
#include <vector>

namespace AgcDriver {
class PerformanceTimer;
}

namespace AgcDriver::Graphics {

class TextureCache {
public:
    explicit TextureCache(const Context& context);
    std::shared_ptr<Texture> Get(std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components, bool storage = false, bool compare = false, const DepthImage* attachedDepth = nullptr, bool stores = false);
    std::shared_ptr<Texture> Null(TextureDimension dimension, TextureNumericClass numericClass = TextureNumericClass::Float);
    std::shared_ptr<Texture> NullStorage(TextureDimension dimension, TextureNumericClass numericClass = TextureNumericClass::Float);
    void DumpTextures(const std::string& prefix);
    std::string DescribeContents(const Texture& texture);
    std::vector<unsigned char> Contents(const Texture& texture);

private:
    struct Surface {
        GuestTextureResource identity;
        std::vector<std::byte> snapshot;
        std::uint64_t bytes = 0;
        std::uint64_t stamp = 0;
        std::weak_ptr<Texture> texture;
        std::uint32_t changedUses = 0;
        std::uint32_t volatileUses = 0;
        bool stored = false;
        bool stale = false;
    };
    struct Entry {
        std::array<std::uint32_t, 8> descriptor;
        TextureDimension viewDimension;
        std::shared_ptr<Texture> texture;
        std::shared_ptr<Surface> surface;
        std::weak_ptr<ResidentColor> source;
        std::vector<std::weak_ptr<DepthImage>> depthSources;
        std::uint64_t generation = 0;
        std::uint64_t retained = 0;
        bool compare = false;
        bool depthView = false;
    };
    void trim();
    std::list<Entry>::iterator eraseEntry(std::list<Entry>::iterator it);
    std::list<Entry>::iterator findEntry(const std::array<std::uint32_t, 8>& descriptor, TextureDimension viewDimension, bool compare = false, bool depthView = false);
    void addEntry(Entry entry);
    bool depthResident(const Entry& entry) const;
    bool refresh(Surface& surface, std::uint64_t address, bool track);
    std::shared_ptr<Texture> hostTexture(const GuestTextureResource& resource, VkComponentMapping components, std::uint64_t bytes, std::uint64_t stamp);
    bool hostReadable(std::uint64_t address, std::uint64_t bytes);
    std::shared_ptr<Texture> directDepth(const std::array<std::uint32_t, 8>& key, const std::shared_ptr<DepthImage>& image, bool stencil, const GuestTextureResource& resource, VkComponentMapping components, bool compare, PerformanceTimer& timing);
    static std::uint64_t descriptorHash(const std::array<std::uint32_t, 8>& descriptor);
    static bool SameSurface(const GuestTextureResource& a, const GuestTextureResource& b);
    Context context;
    std::list<Entry> entries;
    std::unordered_multimap<std::uint64_t, std::list<Entry>::iterator> index;
    std::uint32_t lookupsSinceSweep = 0;
    std::map<std::pair<TextureDimension, TextureNumericClass>, std::shared_ptr<Texture>> nulls;
    std::map<std::pair<TextureDimension, TextureNumericClass>, std::shared_ptr<Texture>> storageNulls;
    std::uint64_t retainedBytes = 0;
    std::uint64_t budget;
    static constexpr std::size_t maxEntries = 8192;
    static constexpr std::uint32_t VolatileUses = 8;
    static constexpr std::uint32_t VolatileProbe = 64;
};

}

#endif

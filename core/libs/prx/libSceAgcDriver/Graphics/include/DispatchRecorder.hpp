#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DISPATCHRECORDER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DISPATCHRECORDER_HPP

#include "prx/libSceAgcDriver/Graphics/include/CaptureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DispatchBindings.hpp"
#include "Recompiler.hpp"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {

class DispatchRecorder {
public:
    DispatchRecorder(const Context& context, const CaptureTarget& target, const ShaderRecompiler::RecompileResult& shader, std::array<std::uint32_t, 3> groups, DispatchBindings bindings);
    DispatchRecorder(const DispatchRecorder&) = delete;
    DispatchRecorder& operator=(const DispatchRecorder&) = delete;
    void Before();
    void After();
    std::filesystem::path Finish();
    std::string Summary() const;

private:
    struct ImagePlan {
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        std::vector<std::array<std::uint32_t, 3>> before;
        std::vector<std::array<std::uint32_t, 3>> after;
        bool known = false;
    };

    void planImages();
    std::vector<std::byte> copyImage(const ImagePlan& plan, const CaptureManifest::Image& image, std::span<const std::array<std::uint32_t, 3>> subresources, std::vector<CaptureManifest::Subresource>& layout);
    std::string storeRegion(const DispatchBindings::BoundRegion& region);
    std::string store(std::span<const std::byte> bytes);
    const DispatchBindings::BoundRegion* regionOf(std::uint64_t begin, std::uint64_t end) const;

    const Context& context;
    CaptureTarget target;
    const ShaderRecompiler::RecompileResult& shader;
    std::array<std::uint32_t, 3> groups;
    DispatchBindings bindings;
    std::filesystem::path directory;
    CaptureManifest manifest;
    std::vector<ImagePlan> plans;
    std::uint64_t storedBytes = 0;
    bool before = false;
    bool after = false;
};

}

#endif

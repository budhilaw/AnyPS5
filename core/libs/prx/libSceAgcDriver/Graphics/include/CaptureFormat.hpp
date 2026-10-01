#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_CAPTUREFORMAT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_CAPTUREFORMAT_HPP

#include "prx/libSceAgcDriver/Graphics/include/DispatchBindings.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {

inline constexpr const char* CaptureManifestName = "capture.txt";
inline constexpr const char* CaptureBlobDirectory = "blobs";
inline constexpr const char* CaptureSpirvName = "shader.spv";
inline constexpr const char* CaptureRequestName = "request.cache";
inline constexpr const char* CaptureRequestHeader = "anyps5-shader-requests 1";

struct CaptureTarget {
    std::filesystem::path root;
    std::uint64_t program = 0;
    std::uint64_t codeHash = 0;
    std::uint32_t index = 1;
    std::string request;
};

struct CaptureManifest {
    struct Layout {
        std::uint32_t binding = 0;
        std::uint32_t type = 0;
        std::uint32_t count = 0;
    };
    struct Descriptor {
        std::uint32_t binding = 0;
        std::uint32_t role = 0;
        std::uint32_t kind = 0;
        std::uint32_t count = 0;
        std::uint32_t imageDepthCompare = 0;
        std::vector<std::uint32_t> samplerDepthCompare;
        std::vector<std::uint32_t> words;
    };
    struct Region {
        std::uint64_t begin = 0;
        std::uint64_t end = 0;
        std::uint64_t padding = 0;
        bool writable = false;
        bool imported = false;
        std::uint32_t memory = 0;
        std::string blob;
    };
    struct Write {
        std::uint64_t begin = 0;
        std::uint64_t end = 0;
        std::string blob;
    };
    struct Buffer {
        std::uint32_t binding = 0;
        std::uint32_t element = 0;
        CaptureBufferSource source = CaptureBufferSource::Zero;
        std::uint64_t address = 0;
        std::uint64_t size = 0;
        std::uint32_t memory = 0;
        std::string blob;
        std::string after;
    };
    struct Subresource {
        std::uint32_t aspect = 0;
        std::uint32_t level = 0;
        std::uint32_t layer = 0;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t depth = 0;
    };
    struct Image {
        std::uint32_t type = 0;
        std::uint32_t format = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t depth = 0;
        std::uint32_t levels = 0;
        std::uint32_t layers = 0;
        std::uint32_t flags = 0;
        std::uint32_t usage = 0;
        std::uint32_t layout = 0;
        std::string blob;
        std::vector<Subresource> contents;
        std::string after;
        std::vector<Subresource> written;
    };
    struct View {
        std::uint32_t binding = 0;
        std::uint32_t element = 0;
        std::uint32_t type = 0;
        std::uint32_t image = 0;
        std::uint32_t viewType = 0;
        std::uint32_t format = 0;
        std::array<std::uint32_t, 4> components{};
        std::uint32_t aspect = 0;
        std::uint32_t baseLevel = 0;
        std::uint32_t levels = 0;
        std::uint32_t baseLayer = 0;
        std::uint32_t layers = 0;
        float minLod = 0.0f;
        std::uint32_t usage = 0;
        std::uint32_t layout = 0;
    };
    struct Sampler {
        std::uint32_t binding = 0;
        std::uint32_t element = 0;
        std::uint32_t magFilter = 0;
        std::uint32_t minFilter = 0;
        std::uint32_t mipmapMode = 0;
        std::uint32_t addressU = 0;
        std::uint32_t addressV = 0;
        std::uint32_t addressW = 0;
        float mipLodBias = 0.0f;
        std::uint32_t anisotropyEnable = 0;
        float maxAnisotropy = 0.0f;
        std::uint32_t compareEnable = 0;
        std::uint32_t compareOp = 0;
        float minLod = 0.0f;
        float maxLod = 0.0f;
        std::uint32_t borderColor = 0;
        std::uint32_t unnormalized = 0;
    };

    std::uint64_t program = 0;
    std::uint64_t codeHash = 0;
    std::array<std::uint32_t, 3> groups{};
    std::uint32_t lanes = 1;
    std::string spirv;
    std::string request;
    std::vector<std::byte> push;
    std::vector<Layout> layout;
    std::vector<Descriptor> descriptors;
    std::vector<Region> regions;
    std::vector<Write> writes;
    std::vector<Buffer> buffers;
    std::vector<Image> images;
    std::vector<View> views;
    std::vector<Sampler> samplers;
    std::vector<std::string> notes;
};

struct CaptureFormatBlock {
    std::uint32_t width = 1;
    std::uint32_t height = 1;
    std::uint32_t bytes = 0;
};

std::uint64_t CaptureContentHash(std::span<const std::byte> bytes);
std::filesystem::path CaptureBlobRoot(const std::filesystem::path& directory);
std::string WriteCaptureBlob(const std::filesystem::path& root, std::span<const std::byte> bytes);
std::vector<std::byte> ReadCaptureBlob(const std::filesystem::path& root, const std::string& name);
void WriteCaptureManifest(const std::filesystem::path& directory, const CaptureManifest& manifest);
CaptureManifest ReadCaptureManifest(const std::filesystem::path& directory);
std::optional<CaptureFormatBlock> CaptureBlockOf(VkFormat format, VkImageAspectFlags aspect);
VkImageAspectFlags CaptureFormatAspects(VkFormat format);
std::uint64_t CaptureSubresourceBytes(const CaptureFormatBlock& block, std::uint32_t width, std::uint32_t height, std::uint32_t depth);
std::vector<CaptureManifest::Subresource> CaptureLayoutSubresources(VkFormat format, VkImageType type, VkExtent3D extent, std::span<const std::array<std::uint32_t, 3>> subresources, std::uint64_t& totalBytes);

}

#endif

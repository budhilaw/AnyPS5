#include "prx/libSceAgcDriver/Graphics/include/CaptureFormat.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace AgcDriver::Graphics {

namespace {

constexpr std::string_view ManifestHeader = "anyps5-dispatch-capture 1";

class Line {
public:
    explicit Line(const char* keyword) : text(keyword) {}

    Line& Number(std::uint64_t value) {
        text += ' ';
        text += std::to_string(value);
        return *this;
    }

    Line& Hex(std::uint64_t value) {
        char item[24];
        std::snprintf(item, sizeof(item), " 0x%llx", static_cast<unsigned long long>(value));
        text += item;
        return *this;
    }

    Line& Float(float value) {
        return Hex(std::bit_cast<std::uint32_t>(value));
    }

    Line& Text(std::string_view value) {
        text += ' ';
        text += value.empty() ? std::string_view("-") : value;
        return *this;
    }

    Line& Words(std::span<const std::uint32_t> values) {
        Number(values.size());
        for (const auto value : values) Hex(value);
        return *this;
    }

    std::string text;
};

class Fields {
public:
    Fields(const std::string& line, std::size_t number) : number(number) {
        std::istringstream stream(line);
        for (std::string token; stream >> token;) tokens.push_back(std::move(token));
        rest = line;
    }

    bool Empty() const {
        return tokens.empty();
    }

    const std::string& Keyword() const {
        return tokens.front();
    }

    std::uint64_t Number() {
        const auto& token = next();
        try {
            std::size_t used = 0;
            const auto value = std::stoull(token, &used, 0);
            if (used != token.size()) Fail("malformed number " + token);
            return value;
        } catch (const std::logic_error&) {
            Fail("malformed number " + token);
        }
    }

    std::uint32_t Number32() {
        const auto value = Number();
        if (value > 0xffffffffull) Fail("number out of range");
        return static_cast<std::uint32_t>(value);
    }

    float Float() {
        return std::bit_cast<float>(Number32());
    }

    bool Flag() {
        return Number() != 0;
    }

    std::string Text() {
        const auto& token = next();
        return token == "-" ? std::string() : token;
    }

    std::vector<std::uint32_t> Words() {
        const auto count = Number();
        if (count > tokens.size()) Fail("word count exceeds the line");
        std::vector<std::uint32_t> values;
        values.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t index = 0; index < count; ++index) values.push_back(Number32());
        return values;
    }

    std::string Remainder() const {
        const auto first = rest.find_first_not_of(" \t");
        if (first == std::string::npos) return {};
        const auto separator = rest.find_first_of(" \t", first);
        if (separator == std::string::npos) return {};
        const auto text = rest.find_first_not_of(" \t", separator);
        return text == std::string::npos ? std::string() : rest.substr(text);
    }

    void Finish() const {
        if (cursor != tokens.size()) Fail("unexpected trailing fields");
    }

    [[noreturn]] void Fail(const std::string& reason) const {
        throw std::runtime_error("capture manifest line " + std::to_string(number) + ": " + reason);
    }

private:
    const std::string& next() {
        if (cursor >= tokens.size()) Fail("missing field");
        return tokens[cursor++];
    }

    std::vector<std::string> tokens;
    std::string rest;
    std::size_t cursor = 1;
    std::size_t number;
};

std::string hexBytes(std::span<const std::byte> bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        text += digits[static_cast<unsigned>(byte) >> 4u];
        text += digits[static_cast<unsigned>(byte) & 15u];
    }
    return text;
}

std::vector<std::byte> parseHexBytes(const std::string& text, const Fields& fields) {
    if (text.size() % 2 != 0) fields.Fail("odd hex byte string");
    const auto digit = [&](char c) -> unsigned {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<unsigned>(c - 'A' + 10);
        fields.Fail("malformed hex byte string");
    };
    std::vector<std::byte> bytes(text.size() / 2);
    for (std::size_t index = 0; index < bytes.size(); ++index) bytes[index] = static_cast<std::byte>((digit(text[index * 2]) << 4u) | digit(text[index * 2 + 1]));
    return bytes;
}

void writeSubresource(std::string& out, const char* keyword, std::size_t image, const CaptureManifest::Subresource& subresource) {
    out += Line(keyword).Number(image).Hex(subresource.aspect).Number(subresource.level).Number(subresource.layer).Hex(subresource.offset).Hex(subresource.size).Number(subresource.width).Number(subresource.height).Number(subresource.depth).text + '\n';
}

CaptureManifest::Subresource readSubresource(Fields& fields) {
    CaptureManifest::Subresource subresource;
    subresource.aspect = fields.Number32();
    subresource.level = fields.Number32();
    subresource.layer = fields.Number32();
    subresource.offset = fields.Number();
    subresource.size = fields.Number();
    subresource.width = fields.Number32();
    subresource.height = fields.Number32();
    subresource.depth = fields.Number32();
    return subresource;
}

std::uint64_t alignUp(std::uint64_t value, std::uint64_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

}

std::uint64_t CaptureContentHash(std::span<const std::byte> bytes) {
    std::uint64_t hash = 0x9e3779b97f4a7c15ull;
    std::size_t offset = 0;
    for (; offset + 32 <= bytes.size(); offset += 32) {
        std::uint64_t words[4];
        std::memcpy(words, bytes.data() + offset, sizeof(words));
        for (const auto word : words) {
            hash = (hash ^ word) * 0xff51afd7ed558ccdull;
            hash ^= hash >> 32u;
        }
    }
    for (; offset + 8 <= bytes.size(); offset += 8) {
        std::uint64_t word = 0;
        std::memcpy(&word, bytes.data() + offset, sizeof(word));
        hash = (hash ^ word) * 0xff51afd7ed558ccdull;
        hash ^= hash >> 32u;
    }
    std::uint64_t tail = 0;
    if (offset < bytes.size()) std::memcpy(&tail, bytes.data() + offset, bytes.size() - offset);
    hash = (hash ^ tail) * 0xc4ceb9fe1a85ec53ull;
    hash ^= hash >> 29u;
    hash = (hash ^ bytes.size()) * 0xff51afd7ed558ccdull;
    hash ^= hash >> 32u;
    return hash;
}

std::filesystem::path CaptureBlobRoot(const std::filesystem::path& directory) {
    const auto absolute = std::filesystem::absolute(directory).lexically_normal();
    return (absolute.has_filename() ? absolute : absolute.parent_path()).parent_path();
}

std::string WriteCaptureBlob(const std::filesystem::path& root, std::span<const std::byte> bytes) {
    char name[64];
    std::snprintf(name, sizeof(name), "%016llx-%llx.bin", static_cast<unsigned long long>(CaptureContentHash(bytes)), static_cast<unsigned long long>(bytes.size()));
    const auto directory = root / CaptureBlobDirectory;
    std::filesystem::create_directories(directory);
    const auto path = directory / name;
    std::error_code error;
    if (std::filesystem::is_regular_file(path, error)) {
        if (std::filesystem::file_size(path, error) == bytes.size() && !error) return name;
        std::filesystem::remove(path, error);
    }
    const auto partial = directory / (std::string(name) + ".partial");
    {
        std::ofstream file(partial, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!file) throw std::runtime_error("cannot write capture blob " + partial.string());
    }
    std::filesystem::rename(partial, path);
    return name;
}

std::vector<std::byte> ReadCaptureBlob(const std::filesystem::path& root, const std::string& name) {
    if (name.empty()) throw std::runtime_error("capture blob name is empty");
    const auto path = root / CaptureBlobDirectory / name;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("cannot open capture blob " + path.string());
    const auto size = static_cast<std::size_t>(file.tellg());
    std::vector<std::byte> bytes(size);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    if (!file) throw std::runtime_error("cannot read capture blob " + path.string());
    return bytes;
}

void WriteCaptureManifest(const std::filesystem::path& directory, const CaptureManifest& manifest) {
    std::string out(ManifestHeader);
    out += '\n';
    out += Line("program").Hex(manifest.program).text + '\n';
    out += Line("code-hash").Hex(manifest.codeHash).text + '\n';
    out += Line("groups").Number(manifest.groups[0]).Number(manifest.groups[1]).Number(manifest.groups[2]).text + '\n';
    out += Line("lanes").Number(manifest.lanes).text + '\n';
    out += Line("spirv").Text(manifest.spirv).text + '\n';
    out += Line("request").Text(manifest.request).text + '\n';
    out += Line("push").Text(hexBytes(manifest.push)).text + '\n';
    for (const auto& layout : manifest.layout) out += Line("layout").Number(layout.binding).Number(layout.type).Number(layout.count).text + '\n';
    for (const auto& descriptor : manifest.descriptors) {
        out += Line("descriptor").Number(descriptor.binding).Number(descriptor.role).Number(descriptor.kind).Number(descriptor.count).Number(descriptor.imageDepthCompare).Words(descriptor.samplerDepthCompare).Words(descriptor.words).text + '\n';
    }
    for (const auto& region : manifest.regions) {
        out += Line("region").Hex(region.begin).Hex(region.end).Hex(region.padding).Number(region.writable ? 1 : 0).Number(region.imported ? 1 : 0).Hex(region.memory).Text(region.blob).text + '\n';
    }
    for (const auto& write : manifest.writes) out += Line("write").Hex(write.begin).Hex(write.end).Text(write.blob).text + '\n';
    for (const auto& buffer : manifest.buffers) {
        out += Line("buffer").Number(buffer.binding).Number(buffer.element).Number(static_cast<std::uint32_t>(buffer.source)).Hex(buffer.address).Hex(buffer.size).Hex(buffer.memory).Text(buffer.blob).Text(buffer.after).text + '\n';
    }
    for (std::size_t index = 0; index < manifest.images.size(); ++index) {
        const auto& image = manifest.images[index];
        out += Line("image").Number(image.type).Number(image.format).Number(image.width).Number(image.height).Number(image.depth).Number(image.levels).Number(image.layers).Hex(image.flags).Hex(image.usage).Number(image.layout).Text(image.blob).Text(image.after).text + '\n';
        for (const auto& subresource : image.contents) writeSubresource(out, "content", index, subresource);
        for (const auto& subresource : image.written) writeSubresource(out, "written", index, subresource);
    }
    for (const auto& view : manifest.views) {
        out += Line("view").Number(view.binding).Number(view.element).Number(view.type).Number(view.image).Number(view.viewType).Number(view.format).Number(view.components[0]).Number(view.components[1]).Number(view.components[2]).Number(view.components[3]).Hex(view.aspect).Number(view.baseLevel).Number(view.levels).Number(view.baseLayer).Number(view.layers).Float(view.minLod).Hex(view.usage).Number(view.layout).text + '\n';
    }
    for (const auto& sampler : manifest.samplers) {
        out += Line("sampler").Number(sampler.binding).Number(sampler.element).Number(sampler.magFilter).Number(sampler.minFilter).Number(sampler.mipmapMode).Number(sampler.addressU).Number(sampler.addressV).Number(sampler.addressW).Float(sampler.mipLodBias).Number(sampler.anisotropyEnable).Float(sampler.maxAnisotropy).Number(sampler.compareEnable).Number(sampler.compareOp).Float(sampler.minLod).Float(sampler.maxLod).Number(sampler.borderColor).Number(sampler.unnormalized).text + '\n';
    }
    for (const auto& note : manifest.notes) {
        auto text = note;
        std::replace(text.begin(), text.end(), '\n', ' ');
        out += "note " + text + '\n';
    }
    std::filesystem::create_directories(directory);
    const auto partial = directory / (std::string(CaptureManifestName) + ".partial");
    {
        std::ofstream file(partial, std::ios::binary | std::ios::trunc);
        file << out;
        if (!file) throw std::runtime_error("cannot write capture manifest " + partial.string());
    }
    std::error_code error;
    std::filesystem::remove(directory / CaptureManifestName, error);
    std::filesystem::rename(partial, directory / CaptureManifestName);
}

CaptureManifest ReadCaptureManifest(const std::filesystem::path& directory) {
    const auto path = directory / CaptureManifestName;
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open capture manifest " + path.string());
    CaptureManifest manifest;
    std::string line;
    std::size_t number = 0;
    bool header = false;
    while (std::getline(file, line)) {
        ++number;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (!header) {
            if (line != ManifestHeader) throw std::runtime_error(path.string() + " is not a dispatch capture manifest");
            header = true;
            continue;
        }
        Fields fields(line, number);
        if (fields.Empty()) continue;
        const auto keyword = fields.Keyword();
        if (keyword == "note") {
            manifest.notes.push_back(fields.Remainder());
            continue;
        }
        if (keyword == "program") manifest.program = fields.Number();
        else if (keyword == "code-hash") manifest.codeHash = fields.Number();
        else if (keyword == "groups") {
            for (auto& group : manifest.groups) group = fields.Number32();
        } else if (keyword == "lanes") manifest.lanes = fields.Number32();
        else if (keyword == "spirv") manifest.spirv = fields.Text();
        else if (keyword == "request") manifest.request = fields.Text();
        else if (keyword == "push") manifest.push = parseHexBytes(fields.Text(), fields);
        else if (keyword == "layout") {
            CaptureManifest::Layout layout;
            layout.binding = fields.Number32();
            layout.type = fields.Number32();
            layout.count = fields.Number32();
            manifest.layout.push_back(layout);
        } else if (keyword == "descriptor") {
            CaptureManifest::Descriptor descriptor;
            descriptor.binding = fields.Number32();
            descriptor.role = fields.Number32();
            descriptor.kind = fields.Number32();
            descriptor.count = fields.Number32();
            descriptor.imageDepthCompare = fields.Number32();
            descriptor.samplerDepthCompare = fields.Words();
            descriptor.words = fields.Words();
            manifest.descriptors.push_back(std::move(descriptor));
        } else if (keyword == "region") {
            CaptureManifest::Region region;
            region.begin = fields.Number();
            region.end = fields.Number();
            region.padding = fields.Number();
            region.writable = fields.Flag();
            region.imported = fields.Flag();
            region.memory = fields.Number32();
            region.blob = fields.Text();
            if (region.end <= region.begin) fields.Fail("empty region");
            manifest.regions.push_back(std::move(region));
        } else if (keyword == "write") {
            CaptureManifest::Write write;
            write.begin = fields.Number();
            write.end = fields.Number();
            write.blob = fields.Text();
            if (write.end <= write.begin) fields.Fail("empty write range");
            manifest.writes.push_back(std::move(write));
        } else if (keyword == "buffer") {
            CaptureManifest::Buffer buffer;
            buffer.binding = fields.Number32();
            buffer.element = fields.Number32();
            const auto source = fields.Number32();
            if (source > static_cast<std::uint32_t>(CaptureBufferSource::Fault)) fields.Fail("unknown buffer source");
            buffer.source = static_cast<CaptureBufferSource>(source);
            buffer.address = fields.Number();
            buffer.size = fields.Number();
            buffer.memory = fields.Number32();
            buffer.blob = fields.Text();
            buffer.after = fields.Text();
            manifest.buffers.push_back(std::move(buffer));
        } else if (keyword == "image") {
            CaptureManifest::Image image;
            image.type = fields.Number32();
            image.format = fields.Number32();
            image.width = fields.Number32();
            image.height = fields.Number32();
            image.depth = fields.Number32();
            image.levels = fields.Number32();
            image.layers = fields.Number32();
            image.flags = fields.Number32();
            image.usage = fields.Number32();
            image.layout = fields.Number32();
            image.blob = fields.Text();
            image.after = fields.Text();
            manifest.images.push_back(std::move(image));
        } else if (keyword == "content" || keyword == "written") {
            const auto index = fields.Number();
            if (index >= manifest.images.size()) fields.Fail("subresource of an unknown image");
            auto& image = manifest.images[static_cast<std::size_t>(index)];
            (keyword == "content" ? image.contents : image.written).push_back(readSubresource(fields));
        } else if (keyword == "view") {
            CaptureManifest::View view;
            view.binding = fields.Number32();
            view.element = fields.Number32();
            view.type = fields.Number32();
            view.image = fields.Number32();
            view.viewType = fields.Number32();
            view.format = fields.Number32();
            for (auto& component : view.components) component = fields.Number32();
            view.aspect = fields.Number32();
            view.baseLevel = fields.Number32();
            view.levels = fields.Number32();
            view.baseLayer = fields.Number32();
            view.layers = fields.Number32();
            view.minLod = fields.Float();
            view.usage = fields.Number32();
            view.layout = fields.Number32();
            manifest.views.push_back(view);
        } else if (keyword == "sampler") {
            CaptureManifest::Sampler sampler;
            sampler.binding = fields.Number32();
            sampler.element = fields.Number32();
            sampler.magFilter = fields.Number32();
            sampler.minFilter = fields.Number32();
            sampler.mipmapMode = fields.Number32();
            sampler.addressU = fields.Number32();
            sampler.addressV = fields.Number32();
            sampler.addressW = fields.Number32();
            sampler.mipLodBias = fields.Float();
            sampler.anisotropyEnable = fields.Number32();
            sampler.maxAnisotropy = fields.Float();
            sampler.compareEnable = fields.Number32();
            sampler.compareOp = fields.Number32();
            sampler.minLod = fields.Float();
            sampler.maxLod = fields.Float();
            sampler.borderColor = fields.Number32();
            sampler.unnormalized = fields.Number32();
            manifest.samplers.push_back(sampler);
        } else {
            fields.Fail("unknown record " + keyword);
        }
        fields.Finish();
    }
    if (!header) throw std::runtime_error(path.string() + " is empty");
    for (const auto& view : manifest.views) {
        if (view.image >= manifest.images.size()) throw std::runtime_error(path.string() + ": a view refers to an unknown image");
    }
    return manifest;
}

std::optional<CaptureFormatBlock> CaptureBlockOf(VkFormat format, VkImageAspectFlags aspect) {
    if (aspect == VK_IMAGE_ASPECT_STENCIL_BIT) {
        if (format == VK_FORMAT_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT) return CaptureFormatBlock{1, 1, 1};
        return std::nullopt;
    }
    if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT) {
        switch (format) {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D16_UNORM_S8_UINT: return CaptureFormatBlock{1, 1, 2};
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT: return CaptureFormatBlock{1, 1, 4};
            default: return std::nullopt;
        }
    }
    if (aspect != VK_IMAGE_ASPECT_COLOR_BIT) return std::nullopt;
    const auto value = static_cast<std::uint32_t>(format);
    const auto texel = [](std::uint32_t bytes) { return std::optional<CaptureFormatBlock>(CaptureFormatBlock{1, 1, bytes}); };
    const auto block = [](std::uint32_t bytes) { return std::optional<CaptureFormatBlock>(CaptureFormatBlock{4, 4, bytes}); };
    if (value == 1 || (value >= 9 && value <= 15)) return texel(1);
    if ((value >= 2 && value <= 8) || (value >= 16 && value <= 22) || (value >= 70 && value <= 76)) return texel(2);
    if (value >= 23 && value <= 36) return texel(3);
    if ((value >= 37 && value <= 69) || (value >= 77 && value <= 83) || (value >= 98 && value <= 100) || value == 122 || value == 123) return texel(4);
    if (value >= 84 && value <= 90) return texel(6);
    if ((value >= 91 && value <= 97) || (value >= 101 && value <= 103) || (value >= 110 && value <= 112)) return texel(8);
    if (value >= 104 && value <= 106) return texel(12);
    if ((value >= 107 && value <= 109) || (value >= 113 && value <= 115)) return texel(16);
    if (value >= 116 && value <= 118) return texel(24);
    if (value >= 119 && value <= 121) return texel(32);
    if ((value >= 131 && value <= 134) || value == 139 || value == 140) return block(8);
    if ((value >= 135 && value <= 138) || (value >= 141 && value <= 146)) return block(16);
    return std::nullopt;
}

VkImageAspectFlags CaptureFormatAspects(VkFormat format) {
    switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT: return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_S8_UINT: return VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default: return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

std::uint64_t CaptureSubresourceBytes(const CaptureFormatBlock& block, std::uint32_t width, std::uint32_t height, std::uint32_t depth) {
    const std::uint64_t columns = (width + block.width - 1) / block.width;
    const std::uint64_t rows = (height + block.height - 1) / block.height;
    return columns * rows * depth * block.bytes;
}

std::vector<CaptureManifest::Subresource> CaptureLayoutSubresources(VkFormat format, VkImageType type, VkExtent3D extent, std::span<const std::array<std::uint32_t, 3>> subresources, std::uint64_t& totalBytes) {
    std::vector<CaptureManifest::Subresource> result;
    result.reserve(subresources.size());
    totalBytes = 0;
    for (const auto& [aspect, level, layer] : subresources) {
        const auto block = CaptureBlockOf(format, aspect);
        if (!block) throw std::runtime_error("image format " + std::to_string(static_cast<std::uint32_t>(format)) + " aspect " + std::to_string(aspect) + " has no known texel size");
        CaptureManifest::Subresource subresource;
        subresource.aspect = aspect;
        subresource.level = level;
        subresource.layer = layer;
        subresource.width = std::max(extent.width >> level, 1u);
        subresource.height = type == VK_IMAGE_TYPE_1D ? 1u : std::max(extent.height >> level, 1u);
        subresource.depth = type == VK_IMAGE_TYPE_3D ? std::max(extent.depth >> level, 1u) : 1u;
        subresource.size = CaptureSubresourceBytes(*block, subresource.width, subresource.height, subresource.depth);
        subresource.offset = alignUp(totalBytes, static_cast<std::uint64_t>(block->bytes) * 4u);
        totalBytes = subresource.offset + subresource.size;
        result.push_back(subresource);
    }
    return result;
}

}

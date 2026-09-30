#include <algorithm>
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/General.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <limits>
#include <set>
#include <string>

namespace AgcDriver::Graphics {
namespace {

constexpr std::size_t EmptyBufferBytes = 16;

bool overlap(std::uint64_t first, std::size_t firstSize, std::uint64_t second, std::size_t secondSize) {
    return first < second + secondSize && second < first + firstSize;
}

VkComponentSwizzle ComponentSwizzleFor(std::uint8_t dstSel) {
    switch (dstSel) {
        case 0: return VK_COMPONENT_SWIZZLE_ZERO;
        case 1: return VK_COMPONENT_SWIZZLE_ONE;
        case 4: return VK_COMPONENT_SWIZZLE_R;
        case 5: return VK_COMPONENT_SWIZZLE_G;
        case 6: return VK_COMPONENT_SWIZZLE_B;
        case 7: return VK_COMPONENT_SWIZZLE_A;
        default: throw std::runtime_error("AGC graphics: guest texture descriptor has an invalid destination channel selector " + std::to_string(dstSel));
    }
}

const char* roleName(ShaderRecompiler::DescriptorRole role) {
    switch (role) {
        case ShaderRecompiler::DescriptorRole::GuestBuffers: return "GuestBuffers";
        case ShaderRecompiler::DescriptorRole::GuestImages: return "GuestImages";
        case ShaderRecompiler::DescriptorRole::GuestSamplers: return "GuestSamplers";
        case ShaderRecompiler::DescriptorRole::Gds: return "Gds";
        case ShaderRecompiler::DescriptorRole::BdaPagetable: return "BdaPagetable";
        case ShaderRecompiler::DescriptorRole::FaultBuffer: return "FaultBuffer";
        case ShaderRecompiler::DescriptorRole::FlattenedSrt: return "FlattenedSrt";
        case ShaderRecompiler::DescriptorRole::ShaderData: return "ShaderData";
    }
    throw std::runtime_error("AGC graphics: unknown descriptor role");
}

const char* kindName(ShaderRecompiler::DescriptorKind kind) {
    switch (kind) {
        case ShaderRecompiler::DescriptorKind::UniformBuffer: return "UniformBuffer";
        case ShaderRecompiler::DescriptorKind::StorageBuffer: return "StorageBuffer";
        case ShaderRecompiler::DescriptorKind::UniformTexelBuffer: return "UniformTexelBuffer";
        case ShaderRecompiler::DescriptorKind::StorageTexelBuffer: return "StorageTexelBuffer";
        case ShaderRecompiler::DescriptorKind::SampledImage: return "SampledImage";
        case ShaderRecompiler::DescriptorKind::StorageImage: return "StorageImage";
        case ShaderRecompiler::DescriptorKind::Sampler: return "Sampler";
    }
    throw std::runtime_error("AGC graphics: unknown descriptor kind");
}

}

ShaderResources::ShaderResources(const Context& context, const ShaderRecompiler::RecompileResult& vertex, const ShaderRecompiler::RecompileResult& fragment, const ColorTarget& target, std::uint64_t indexAddress, std::size_t indexBytes) : ShaderResources(context, std::array<CompiledShader, 2>{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, static_cast<std::uint32_t>(vertex.pushConstants.size())}}}, target, indexAddress, indexBytes) {}

ShaderResources::ShaderResources(const Context& context, std::span<const CompiledShader> shaders, const ColorTarget& target, std::uint64_t indexAddress, std::size_t indexBytes, std::span<const GuestMemorySnapshot> snapshots) : context(context), guestMemory(context) {
    prepareAddressBindings(shaders, snapshots);
    build(shaders, &target, indexAddress, indexBytes);
}

ShaderResources::ShaderResources(const Context& context, const CompiledShader& compute, std::span<const GuestMemorySnapshot> snapshots) : context(context), guestMemory(context) {
    Require(compute.stage == ShaderRecompiler::ShaderStage::Compute, "compute resources require a compute shader");
    prepareAddressBindings(std::span<const CompiledShader>(&compute, 1), snapshots);
    build(std::span<const CompiledShader>(&compute, 1), nullptr, 0, 0);
}

void ShaderResources::build(std::span<const CompiledShader> shaders, const ColorTarget* target, std::uint64_t indexAddress, std::size_t indexBytes) {
    PerformanceTimer timing("Graphics.ShaderResources");
    try {
        Require(!shaders.empty() && context.limits.maxBoundDescriptorSets >= 1, "shader descriptor set exceeds device limits");
        std::vector<Binding> bindings;
        std::vector<std::uint32_t> occupied;
        std::uint64_t storageBuffers = 0;
        for (const auto& shader : shaders) {
            Require(shader.program != nullptr, "missing compiled shader");
            const VkShaderStageFlags flags = VulkanStage(shader.stage);
            std::uint64_t stageDescriptors = 0;
            for (const auto& binding : shader.program->bindings) {
                Require(binding.descriptorSet == 0, "unexpected descriptor set: every shader resource must use descriptor set zero");
                Require(std::find(occupied.begin(), occupied.end(), binding.binding) == occupied.end(), "duplicate shader binding");
                occupied.push_back(binding.binding);
                const bool addressRole = binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable || binding.role == ShaderRecompiler::DescriptorRole::FaultBuffer;
                const bool gdsRole = binding.role == ShaderRecompiler::DescriptorRole::Gds;
                const bool bufferRole = addressRole || gdsRole || binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers || binding.role == ShaderRecompiler::DescriptorRole::ShaderData || binding.role == ShaderRecompiler::DescriptorRole::FlattenedSrt;
                const bool imageRole = binding.role == ShaderRecompiler::DescriptorRole::GuestImages || binding.role == ShaderRecompiler::DescriptorRole::GuestSamplers;
                if (imageRole) {
                    PerformanceTimer imageTiming("Graphics.ImageBinding");
                    addImageBinding(binding, flags, bindings);
                    continue;
                }
                if (!bufferRole) Require(false, std::string("unsupported descriptor role ") + roleName(binding.role));
                if (binding.kind != ShaderRecompiler::DescriptorKind::StorageBuffer) Require(false, std::string("unsupported descriptor kind ") + kindName(binding.kind) + " for role " + roleName(binding.role) + ": only StorageBuffer is supported");
                Require(!binding.readOnly, "read-only descriptors are unsupported because the recompiler emits no NonWritable decoration");
                Require(binding.count != 0, "empty descriptor binding");
                stageDescriptors += binding.count;
                storageBuffers += binding.count;
                Require(stageDescriptors <= context.limits.maxPerStageDescriptorStorageBuffers && stageDescriptors <= context.limits.maxPerStageResources, "shader descriptors exceed per-stage limits");
                Binding item{{binding.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, binding.count, flags, nullptr}, {}};
                if (binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers) {
                    Require(binding.guestDescriptor.size() == static_cast<std::uint64_t>(binding.count) * 4, "guest buffer descriptor must contain four DWORDs per array element");
                    for (std::uint32_t element = 0; element < binding.count; ++element) {
                        static const bool writeBackAll = std::getenv("ANYPS5_DEBUG_WRITEBACK_ALL") != nullptr;
                        const bool written = writeBackAll || element >= binding.elementWritten.size() || binding.elementWritten[element];
                        const bool read = writeBackAll || element >= binding.elementRead.size() || binding.elementRead[element];
                        auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 4, 4);
                        static constexpr std::array<std::uint32_t, 4> nullDescriptor{};
                        if (element < binding.elementOptional.size() && binding.elementOptional[element] && !usableOptionalBuffer(words)) words = nullDescriptor;
                        item.allocations.push_back(addGuestBuffer(words, written, read, target, indexAddress, indexBytes));
                    }
                } else if (addressRole) {
                    item.allocations.push_back(allocations.size());
                    allocations.push_back({0, 0, false, nullptr, binding.role});
                } else if (gdsRole) {
                    Require(context.gds != nullptr, "GDS is unavailable on this device");
                    static const bool traceGds = std::getenv("ANYPS5_TRACE_INDIRECT") != nullptr;
                    if (traceGds) { static int reported = 0; if (reported++ < 40) APS5_LOG_OUT("shader binds GDS (stage flags 0x%x, binding %u)", static_cast<unsigned>(flags), binding.binding); }
                    if (static const char* dumpDirectory = std::getenv("ANYPS5_DUMP_GDS_SHADERS"); dumpDirectory != nullptr) {
                        static std::set<const void*> dumped;
                        if (dumped.insert(shader.program).second) {
                            const std::string name = std::string(dumpDirectory) + "/gds_" + std::to_string(dumped.size()) + ".spv";
                            if (FILE* file = std::fopen(name.c_str(), "wb")) { std::fwrite(shader.program->spirv.data(), sizeof(std::uint32_t), shader.program->spirv.size(), file); std::fclose(file); }
                        }
                    }
                    Require(binding.count == 1, "GDS descriptors must not be arrays");
                    item.allocations.push_back(allocations.size());
                    allocations.push_back({0, context.gds->Bytes().size(), false, nullptr, binding.role});
                    usesGds = true;
                } else {
                    Require(binding.count == 1, "shader data and flattened SRT descriptors must not be arrays");
                    Require(!binding.guestDescriptor.empty(), "empty shader data descriptor");
                    item.allocations.push_back(addDataBuffer(binding.guestDescriptor));
                }
                bindings.push_back(std::move(item));
            }
        }
        Require(storageBuffers <= context.limits.maxDescriptorSetStorageBuffers, "pipeline descriptors exceed device limits");
        timing.Mark("bindings");
        guestMemory.Upload(usesBda);
        if (usesBda) bda = std::make_unique<BdaResources>(context, guestMemory);
        else if (usesFaultBuffer) bda = std::make_unique<BdaResources>(context);
        timing.Mark("memory_upload");
        std::vector<VkDescriptorSetLayoutBinding> description;
        for (const auto& binding : bindings) {
            description.push_back(binding.layout);
            layoutKey.insert(layoutKey.end(), {binding.layout.binding, static_cast<std::uint32_t>(binding.layout.descriptorType), binding.layout.descriptorCount, binding.layout.stageFlags});
        }
        std::vector<VkDescriptorPoolSize> sizes;
        if (storageBuffers != 0) sizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<std::uint32_t>(storageBuffers)});
        std::uint32_t sampledImages = 0, storageImages = 0;
        for (const auto& binding : bindings) {
            if (binding.layout.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) sampledImages += binding.layout.descriptorCount;
            if (binding.layout.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) storageImages += binding.layout.descriptorCount;
        }
        if (sampledImages != 0) sizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImages});
        if (storageImages != 0) sizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storageImages});
        if (!samplers.empty()) sizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, static_cast<std::uint32_t>(samplers.size())});
        if (!context.descriptorCache) context.descriptorCache = std::make_shared<DescriptorCache>();
        descriptors = context.descriptorCache->Take(layoutKey);
        if (!descriptors) descriptors = std::make_unique<DescriptorAllocation>(context, layoutKey, description, sizes);
        _layout = descriptors->Layout();
        _set = descriptors->Set();
        timing.Mark("descriptor_acquire");
        std::vector<VkDescriptorBufferInfo> buffers;
        std::vector<VkDescriptorImageInfo> images;
        std::vector<VkWriteDescriptorSet> writes;
        buffers.reserve(allocations.size());
        images.reserve(textures.size() + samplers.size());
        writes.reserve(bindings.size());
        for (const auto& binding : bindings) {
            const auto bufferOffset = buffers.size();
            const auto imageOffset = images.size();
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = _set;
            write.dstBinding = binding.layout.binding;
            write.descriptorCount = binding.layout.descriptorCount;
            write.descriptorType = binding.layout.descriptorType;
            switch (binding.layout.descriptorType) {
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    for (const auto index : binding.allocations) buffers.push_back(descriptor(allocations[index]));
                    write.pBufferInfo = buffers.data() + bufferOffset;
                    break;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    for (const auto index : binding.imageAllocations) images.push_back({VK_NULL_HANDLE, textures[index]->View(), textures[index]->Layout()});
                    write.pImageInfo = images.data() + imageOffset;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    for (const auto index : binding.imageAllocations) images.push_back({VK_NULL_HANDLE, textures[index]->StorageView(), VK_IMAGE_LAYOUT_GENERAL});
                    write.pImageInfo = images.data() + imageOffset;
                    break;
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    for (const auto index : binding.imageAllocations) images.push_back({samplers[index]->Handle(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED});
                    write.pImageInfo = images.data() + imageOffset;
                    break;
                default: throw std::runtime_error("AGC graphics: ShaderResources encountered an unknown descriptor type while writing the descriptor set");
            }
            writes.push_back(write);
        }
        if (!writes.empty()) context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
        timing.Mark("descriptor_update");
    } catch (...) {
        release();
        throw;
    }
}

bool ShaderResources::usableOptionalBuffer(std::span<const std::uint32_t> words) const {
    const ShaderRecompiler::ShaderBufferResource descriptor{{words[0], words[1], words[2], words[3]}};
    const auto address = descriptor.Base48();
    const auto bytes = descriptor.GetSize();
    return address != 0 && bytes != 0 && bytes <= (64ull << 20u) && !guestMemory.OverlapsImmutable(address, bytes);
}

std::size_t ShaderResources::addGuestBuffer(std::span<const std::uint32_t> words, bool written, bool read, const ColorTarget* target, std::uint64_t indexAddress, std::size_t indexBytes) {
    Require(words.size() == 4, "buffer descriptor must contain four DWORDs");
    Require((words[1] & 0x40000000u) == 0, "buffer descriptor has reserved bits set");
    const ShaderRecompiler::ShaderBufferResource descriptor{{words[0], words[1], words[2], words[3]}};
    if (descriptor.Type() != 0u && descriptor.Type() != 3u) {
        char message[160];
        std::snprintf(message, sizeof(message), "buffer descriptor uses an unsupported type %u: {%08x %08x %08x %08x}", descriptor.Type(), words[0], words[1], words[2], words[3]);
        throw std::runtime_error(std::string("AGC graphics: ") + message);
    }
    const auto address = descriptor.Base48();
    const auto byteSize = descriptor.GetSize();
    if (address == 0 || byteSize == 0) return addZeroBuffer(address, EmptyBufferBytes);
    Require(byteSize <= context.limits.maxStorageBufferRange, "shader buffer exceeds descriptor range limit");
    Require(byteSize <= std::numeric_limits<std::size_t>::max(), "shader buffer size exceeds host address space");
    const auto size = static_cast<std::size_t>(byteSize);
    static const bool keepDownloads = std::getenv("ANYPS5_NO_WRITE_ONLY_DISCARD") != nullptr;
    if (written && !read && !keepDownloads && context.renderCache != nullptr) context.renderCache->DiscardCovered(address, size);
    try {
        const GuestMemory::MemoryAccessScope deferred(nullptr, nullptr);
        if (context.renderCache == nullptr) GuestMemory::CheckRange(reinterpret_cast<const void*>(address), size, 1, written);
        else for (const auto& [from, to] : context.renderCache->UnwatchedRanges(address, address + size)) GuestMemory::CheckRange(reinterpret_cast<const void*>(from), static_cast<std::size_t>(to - from), 1, written);
    } catch (const std::exception& error) {
        static std::once_flag once;
        std::call_once(once, [&] { APS5_LOG_OUT("shader buffer at 0x%llx (%zu bytes) is not mapped (%s); binding zeros", static_cast<unsigned long long>(address), size, error.what()); });
        return addZeroBuffer(address, size);
    }
    Require(target == nullptr || !overlap(address, size, target->address, target->bytes), "shader buffer aliases the render target");
    Require(!written || !overlap(address, size, indexAddress, indexBytes), "writable shader buffer aliases the index buffer");
    if (written) guestMemory.AddWritable(address, size, true);
    else guestMemory.AddReadOnly(address, size, true);
    allocations.push_back({address, size, true, nullptr});
    return allocations.size() - 1;
}

std::size_t ShaderResources::addZeroBuffer(std::uint64_t address, std::size_t size) {
    const auto padded = size + static_cast<std::size_t>(address % GuestBufferMemory::ViewAlignment);
    auto buffer = std::make_unique<Buffer>(context, padded, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memset(buffer->Bytes().data(), 0, padded);
    allocations.push_back({0, padded, false, std::move(buffer)});
    return allocations.size() - 1;
}

std::size_t ShaderResources::addDataBuffer(std::span<const std::uint32_t> words) {
    const auto size = words.size() * sizeof(std::uint32_t);
    Require(size <= context.limits.maxStorageBufferRange, "shader data buffer exceeds descriptor range limit");
    auto buffer = std::make_unique<Buffer>(context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memcpy(buffer->Bytes().data(), words.data(), size);
    allocations.push_back({0, size, false, std::move(buffer)});
    return allocations.size() - 1;
}

void ShaderResources::addImageBinding(const ShaderRecompiler::DescriptorBinding& binding, VkShaderStageFlags flags, std::vector<Binding>& bindings) {
    Require(binding.count != 0, "empty descriptor binding");
    const bool sampledImage = binding.kind == ShaderRecompiler::DescriptorKind::SampledImage;
    const bool storageImage = binding.kind == ShaderRecompiler::DescriptorKind::StorageImage;
    const bool samplerKind = binding.kind == ShaderRecompiler::DescriptorKind::Sampler;
    if (!sampledImage && !storageImage && !samplerKind) Require(false, std::string("unsupported descriptor kind ") + kindName(binding.kind) + " for role " + roleName(binding.role));
    Require(((sampledImage || storageImage) && binding.role == ShaderRecompiler::DescriptorRole::GuestImages) || (samplerKind && binding.role == ShaderRecompiler::DescriptorRole::GuestSamplers), "guest image descriptor role disagrees with its kind");
    Require(binding.guestDescriptor.size() % binding.count == 0, "guest image descriptor size is not a multiple of the binding count");
    const auto elementWords = binding.guestDescriptor.size() / binding.count;

    Binding item{{binding.binding, sampledImage ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : storageImage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER, binding.count, flags, nullptr}, {}, {}};

    if (sampledImage || storageImage) {
        Require(elementWords == 8, "guest texture descriptor must contain 8 dwords");
        Require(binding.imageShape.has_value(), "guest image binding is missing an image shape");
        Require(context.detiler != nullptr, "device texture detiler is unavailable");
        Require(context.textureCache != nullptr, "device texture cache is unavailable");
        Require(!storageImage || context.storageImages, "device does not support format-less storage images");
        Require(binding.count <= (storageImage ? context.limits.maxPerStageDescriptorStorageImages : context.limits.maxPerStageDescriptorSampledImages), "shader image descriptors exceed per-stage limits");
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords);
            if (words[0] == 0 && (words[1] & 0xffu) == 0) {
                TextureDimension dimension = TextureDimension::k2D;
                for (const auto candidate : {TextureDimension::k2D, TextureDimension::k2DArray, TextureDimension::kCube, TextureDimension::k1D}) if (MatchesGuestDimension(*binding.imageShape, candidate)) { dimension = candidate; break; }
                textures.push_back(context.textureCache->Null(dimension));
                textureBindings.emplace_back(binding.binding, element);
                item.imageAllocations.push_back(textures.size() - 1);
                continue;
            }
            auto resource = DecodeTextureResource(words);
            if (!MatchesGuestDimension(*binding.imageShape, resource.dimension)) {
                using Shape = ShaderRecompiler::DescriptorImageShape;
                const auto shape = *binding.imageShape;
                bool converted = true;
                if (shape == Shape::Image2DArray && (resource.dimension == TextureDimension::k2D || resource.dimension == TextureDimension::kCube)) resource.viewDimension = TextureDimension::k2DArray;
                else if (shape == Shape::Image2D && (resource.dimension == TextureDimension::k2DArray || resource.dimension == TextureDimension::kCube)) resource.viewDimension = TextureDimension::k2D;
                else if (shape == Shape::ImageCube && resource.dimension == TextureDimension::k2DArray && (resource.depthOrLastArray + 1u) % 6u == 0) resource.viewDimension = TextureDimension::kCube;
                else converted = false;
                if (!converted) throw std::runtime_error("AGC graphics: guest texture dimension " + std::to_string(static_cast<int>(resource.dimension)) + " disagrees with the shader's declared image shape " + std::to_string(static_cast<int>(shape)));
                static std::once_flag once;
                std::call_once(once, [&] { APS5_LOG_OUT("sampling textures through a differently shaped sampler (dimension %d as shape %d)", static_cast<int>(resource.dimension), static_cast<int>(shape)); });
            }
            const VkComponentMapping components{ComponentSwizzleFor(resource.dstSelX), ComponentSwizzleFor(resource.dstSelY), ComponentSwizzleFor(resource.dstSelZ), ComponentSwizzleFor(resource.dstSelW)};
            static const std::uint64_t replaced = std::getenv("ANYPS5_DEBUG_NULL_TEXTURE") != nullptr ? std::strtoull(std::getenv("ANYPS5_DEBUG_NULL_TEXTURE"), nullptr, 16) : 0u;
            textures.push_back(replaced != 0 && resource.baseAddress == replaced ? context.textureCache->Null(resource.viewDimension) : context.textureCache->Get(words, resource, components, storageImage, binding.imageDepthCompare));
            textureBindings.emplace_back(binding.binding, element);
            if (storageImage && textures.back()->StorageView() == VK_NULL_HANDLE) {
                char message[200];
                std::snprintf(message, sizeof(message), "AGC graphics: storage image 0x%llx (%ux%u format 0x%x tile %u dimension %u mips %u) does not support shader stores", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, static_cast<unsigned>(resource.tileMode), static_cast<unsigned>(resource.dimension), resource.mipCount);
                throw std::runtime_error(message);
            }
            if (storageImage) {
                textures.back()->MarkStored();
                storesImages = true;
            }
            item.imageAllocations.push_back(textures.size() - 1);
        }
        Require(textures.size() <= context.limits.maxDescriptorSetSampledImages, "pipeline image descriptors exceed device limits");
    } else {
        Require(elementWords == 4, "guest sampler descriptor must contain 4 dwords");
        Require(binding.count <= context.limits.maxPerStageDescriptorSamplers, "shader sampler descriptors exceed per-stage limits");
        Require(binding.samplerDepthCompare.size() == binding.count, "guest sampler binding is missing depth comparison metadata");
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords);
            auto resource = DecodeSamplerResource(words);
            resource.compareEnable = binding.samplerDepthCompare.at(element);
            if (!context.samplerCache) context.samplerCache = std::make_shared<SamplerCache>();
            samplers.push_back(context.samplerCache->Get(context, words, resource));
            item.imageAllocations.push_back(samplers.size() - 1);
        }
        Require(samplers.size() <= context.limits.maxDescriptorSetSamplers, "pipeline sampler descriptors exceed device limits");
    }

    bindings.push_back(std::move(item));
}

ShaderResources::~ShaderResources() {
    release();
}

void ShaderResources::release() noexcept {
    if (descriptors) context.descriptorCache->Put(std::move(descriptors));
    _layout = VK_NULL_HANDLE;
    _set = VK_NULL_HANDLE;
}

VkDescriptorSetLayout ShaderResources::Layout() const {
    return _layout;
}

void ShaderResources::Bind(VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout) const {
    if (_set == VK_NULL_HANDLE) return;
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, bindPoint, layout, 0, 1, &_set, 0, nullptr);
}

std::string ShaderResources::DescribeTextures() const {
    std::string text;
    for (std::size_t i = 0; i < textures.size(); ++i) {
        const auto& texture = textures[i];
        char item[112];
        std::snprintf(item, sizeof(item), " b%u[%u]=0x%llx(%ux%u vk%u m%u l%u d%u%s%s)", textureBindings.at(i).first, textureBindings.at(i).second, static_cast<unsigned long long>(texture->GuestAddress()), texture->Extent().width, texture->Extent().height, static_cast<unsigned>(texture->GuestFormat()), texture->GuestMipCount(), texture->GuestLayers(), texture->GuestDimension(), texture->SharesImage() ? " view" : "", texture->Stored() ? " store" : "");
        text += item;
    }
    return text;
}

void ShaderResources::WriteBack(std::uint64_t sequence) {
    if (bda) bda->CheckFault(guestMemory);
    guestMemory.WriteBack(sequence);
    for (auto& texture : textures) texture->FlushStores();
}

}

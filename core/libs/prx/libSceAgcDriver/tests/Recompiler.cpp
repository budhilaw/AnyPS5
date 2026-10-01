#include "../../../../shader/recompiler/BdaAbi.hpp"
#include "../../../../shader/recompiler/Recompiler.hpp"
#include "../../../../shader/recompiler/tests/SyntheticPrograms.hpp"
#include "RecompilerRequests.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace ShaderRecompiler;

using TextureDescriptor = std::array<std::uint32_t, 8>;

constexpr std::uint64_t CodeAddress = 0x20000u;
constexpr std::uint64_t TableAddress = 0x10000u;
constexpr std::uint32_t LightStride = 0x18cu;
constexpr std::uint32_t ProjectorOffset = 0xb8u;
constexpr std::uint32_t LightCount = 4u;
constexpr std::uint32_t PointerTableOffset = 0x40u;
constexpr std::size_t PointerTableEntries = 256u;

constexpr std::uint32_t ReadFirstLaneS8FromV0 = 0x7e100500u;
constexpr std::uint32_t ShiftS8By5IntoS9 = 0x8f098508u;
constexpr std::uint32_t ShiftS8By4IntoS9 = 0x8f098408u;
constexpr std::uint32_t LoadDescriptorFromS0PlusS9 = 0xf40c0400u;

constexpr std::uint32_t SampleLz2D = 0xf09c0f08u;
constexpr std::uint32_t SampleLz3D = 0xf09c0f10u;
constexpr std::uint32_t SampleLzCube = 0xf09c0f18u;
constexpr std::uint32_t SampleL2D = 0xf0900f08u;
constexpr std::uint32_t SampleL3D = 0xf0900f10u;
constexpr std::uint32_t SampleL1DArray = 0xf0900f20u;
constexpr std::uint32_t SampleL2DArray = 0xf0900f28u;

constexpr std::uint32_t OneBits = 0x3f800000u;
constexpr std::uint32_t TwoBits = 0x40000000u;

constexpr std::uint32_t OpConstant = 43u;
constexpr std::uint32_t OpCopyObject = 83u;
constexpr std::uint32_t OpImageSampleExplicitLod = 88u;
constexpr std::uint32_t OpBitcast = 124u;
constexpr std::uint32_t ImageOperandsLod = 2u;
constexpr std::uint32_t CapabilityInt64 = 11u;
constexpr std::uint32_t CapabilityStorageBuffer8BitAccess = 4448u;
constexpr std::uint32_t CapabilityPhysicalStorageBufferAddresses = 5347u;
constexpr std::array<std::uint32_t, 3> BdaCapabilities{CapabilityInt64, CapabilityPhysicalStorageBufferAddresses, CapabilityStorageBuffer8BitAccess};
constexpr std::array<std::string_view, 2> BdaExtensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};

constexpr std::uint32_t BufferLoadFormatX = 0xe0002000u;
constexpr std::uint32_t BufferLoadFormatXy = 0xe0042000u;
constexpr std::uint32_t BufferLoadDword = 0xe0302000u;
constexpr std::uint32_t BufferStoreDword = 0xe0702000u;
constexpr std::uint32_t BufferStoreDwordx2 = 0xe0742000u;
constexpr std::uint32_t InputOperands = 0x80000100u;
constexpr std::uint32_t OutputOperands = 0x80010100u;
constexpr std::uint32_t EndProgram = 0xbf810000u;
constexpr std::uint32_t InputBufferAddress = 0x40000u;
constexpr std::uint32_t Format16Float = 13u;
constexpr std::uint32_t Format16x2Float = 29u;
constexpr std::uint32_t Format32Float = 22u;
constexpr std::size_t VariantLimit = 16u;

constexpr TextureDescriptor NullTexture{};
constexpr TextureDescriptor Texture1D{0x00000050u, 56u << 20u, 0u, 0x80000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor Texture2D{0x00000020u, 56u << 20u, 0u, 0x90000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor TextureCube{0x00000010u, 56u << 20u, 0u, 0xb0000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor Texture3D{0x00000040u, 56u << 20u, 0u, 0xa0000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor Texture1DArray{0x00000060u, 56u << 20u, 0u, 0xc0000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor Texture2DArray{0x00000070u, 56u << 20u, 0u, 0xd0000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor HostPointers{0x3a4b5c60u, 0x0000020fu, 0x3a4b5d00u, 0x0000020fu, 0x3a4b5e00u, 0x0000020fu, 1u, 0u};
constexpr TextureDescriptor LightDataAboveApplicationMemory{0x10000000u, 0x201d67a0u, 0x00000010u, 0xaae0ffffu, 0u, 0u, 0u, 0u};
constexpr std::array<std::uint32_t, 4> OutputBuffer{0x30000u, 0u, 16u, 0x00000facu};

constexpr TextureDescriptor atAddress(TextureDescriptor texture, std::uint32_t address) {
    texture[0] = address;
    return texture;
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string failureReason(std::string_view message) {
    return std::string(message.substr(0, message.find("\nRecompileRequest:")));
}

RecompileRequest computeRequest(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, std::span<const MemoryRegion> memory) {
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, CodeAddress, code, 0u, {}};
    request.context.waveSize = 32u;
    request.context.userDataBaseRegister = 0u;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.context.memory = memory;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32u;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024u;
    request.target.maxWorkgroupSharedMemoryBytes = 65536u;
    request.layout.pushConstantSizeBytes = 128u;
    request.useCache = false;
    return request;
}

RecompileResult sampleLightTable(std::uint32_t sample, const std::array<TextureDescriptor, LightCount>& projectors) {
    std::vector<std::byte> table(static_cast<std::size_t>(LightCount) * LightStride);
    for (std::size_t light = 0; light < projectors.size(); ++light) {
        std::memcpy(table.data() + light * LightStride + ProjectorOffset, projectors[light].data(), sizeof(TextureDescriptor));
    }
    const std::array<MemoryRegion, 1> memory{MemoryRegion{TableAddress, table}};
    const std::array<std::uint32_t, 16> userData{
        static_cast<std::uint32_t>(TableAddress), 0u, LightCount * LightStride, 0x00000facu,
        0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u,
        OutputBuffer[0], OutputBuffer[1], OutputBuffer[2], OutputBuffer[3]};
    const std::array<std::uint32_t, 14> code{
        0x9309ff08u, LightStride,
        0xf4200600u, 0x12000000u,
        0xf42c0400u, 0x12000000u | ProjectorOffset,
        0x7e000218u, 0x7e0202f0u, 0x7e0402f2u,
        sample, 0x00240400u,
        0xe0780000u, 0x80030400u,
        0xbf810000u};
    return Recompile(computeRequest(code, userData, memory));
}

std::vector<std::byte> tableBytes(std::span<const TextureDescriptor> entries) {
    std::vector<std::byte> bytes(entries.size() * sizeof(TextureDescriptor));
    std::memcpy(bytes.data(), entries.data(), bytes.size());
    return bytes;
}

RecompileResult samplePointerTableMemory(std::uint32_t sample, std::uint32_t scale, std::span<const MemoryRegion> memory) {
    const std::array<std::uint32_t, 16> userData{
        static_cast<std::uint32_t>(TableAddress), 0u, 0u, 0u,
        0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u,
        OutputBuffer[0], OutputBuffer[1], OutputBuffer[2], OutputBuffer[3]};
    const std::array<std::uint32_t, 12> code{
        ReadFirstLaneS8FromV0,
        scale,
        LoadDescriptorFromS0PlusS9, 0x12000000u | PointerTableOffset,
        0x7e0002f0u, 0x7e0202f0u, 0x7e0402f2u,
        sample, 0x00240400u,
        0xe0780000u, 0x80030400u,
        0xbf810000u};
    auto request = computeRequest(code, userData, memory);
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.supportedCapabilities = BdaCapabilities;
    request.target.supportedExtensions = BdaExtensions;
    return Recompile(request);
}

RecompileResult samplePointerTable(std::uint32_t sample, std::span<const TextureDescriptor> entries) {
    const auto bytes = tableBytes(entries);
    const std::array<MemoryRegion, 1> memory{MemoryRegion{TableAddress + PointerTableOffset, bytes}};
    return samplePointerTableMemory(sample, ShiftS8By5IntoS9, memory);
}

RecompileResult sampleTexture(std::uint32_t sample, const TextureDescriptor& texture) {
    const std::array<std::uint32_t, 16> userData{
        texture[0], texture[1], texture[2], texture[3], texture[4], texture[5], texture[6], texture[7],
        0u, 0u, 0u, 0u,
        OutputBuffer[0], OutputBuffer[1], OutputBuffer[2], OutputBuffer[3]};
    const std::array<std::uint32_t, 9> code{
        0x7e0002f0u, 0x7e0202f0u, 0x7e0402f2u, 0x7e0602f4u,
        sample, 0x00400400u,
        0xe0780000u, 0x80030400u,
        0xbf810000u};
    return Recompile(computeRequest(code, userData, {}));
}

const DescriptorBinding& imageBinding(const RecompileResult& result) {
    const DescriptorBinding* found = nullptr;
    for (const auto& binding : result.bindings) {
        if (binding.role == DescriptorRole::GuestImages) {
            require(found == nullptr, "the shader binds more than one image");
            found = &binding;
        }
    }
    require(found != nullptr, "the shader binds no image");
    return *found;
}

void requireImage(const RecompileResult& result, const TextureDescriptor& texture, DescriptorImageShape shape, const char* message) {
    const auto& binding = imageBinding(result);
    require(binding.imageShape == shape, message);
    require(std::ranges::equal(binding.guestDescriptor, texture), message);
}

std::uint32_t constantBits(std::span<const std::uint32_t> spirv, std::uint32_t id) {
    for (std::size_t word = 5; word < spirv.size();) {
        const auto opcode = spirv[word] & 0xffffu;
        const auto count = spirv[word] >> 16u;
        require(count != 0u && word + count <= spirv.size(), "the SPIR-V module is malformed");
        if ((opcode == OpConstant || opcode == OpBitcast || opcode == OpCopyObject) && count == 4u && spirv[word + 2u] == id) {
            return opcode == OpConstant ? spirv[word + 3u] : constantBits(spirv, spirv[word + 3u]);
        }
        word += count;
    }
    throw std::runtime_error("the sampled LOD is not a constant");
}

std::uint32_t sampledLodBits(const RecompileResult& result) {
    const std::span<const std::uint32_t> spirv(result.spirv.data(), result.spirv.size());
    for (std::size_t word = 5; word < spirv.size();) {
        const auto opcode = spirv[word] & 0xffffu;
        const auto count = spirv[word] >> 16u;
        require(count != 0u && word + count <= spirv.size(), "the SPIR-V module is malformed");
        if (opcode == OpImageSampleExplicitLod && count == 7u && spirv[word + 5u] == ImageOperandsLod) {
            return constantBits(spirv, spirv[word + 6u]);
        }
        word += count;
    }
    throw std::runtime_error("the shader has no explicit-LOD sample");
}

using BufferDescriptor = std::array<std::uint32_t, 4>;

constexpr std::uint32_t selectors(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t w) {
    return x | (y << 3u) | (z << 6u) | (w << 9u);
}

constexpr BufferDescriptor bufferDescriptor(std::uint32_t stride, std::uint32_t format, std::uint32_t swizzle) {
    return {InputBufferAddress, stride << 16u, 64u, (format << 12u) | swizzle};
}

constexpr std::array<std::uint32_t, 5> bufferProgram(std::uint32_t load, std::uint32_t store, std::uint32_t storeOffset) {
    return {load, InputOperands, store | storeOffset, OutputOperands, EndProgram};
}

RecompileResult compileBuffer(std::span<const std::uint32_t> code, const BufferDescriptor& input) {
    const std::array<std::uint32_t, 8> userData{input[0], input[1], input[2], input[3], OutputBuffer[0], OutputBuffer[1], OutputBuffer[2], OutputBuffer[3]};
    auto request = computeRequest(code, userData, {});
    request.useCache = true;
    return Recompile(request);
}

void requireTwoVariants(std::span<const std::uint32_t> code, const BufferDescriptor& firstInput, const BufferDescriptor& secondInput, const std::string& field) {
    const auto first = compileBuffer(code, firstInput);
    const auto second = compileBuffer(code, secondInput);
    require(!second.cacheHit, field + " reused a variant compiled for another value");
    require(!(first.spirv == second.spirv), field + " did not change the compiled program");
}

void formattedLoadKeepsAVariantPerSelectorItReads() {
    const auto code = bufferProgram(BufferLoadFormatX, BufferStoreDword, 0x10u);
    requireTwoVariants(code, bufferDescriptor(4u, Format32Float, selectors(4u, 5u, 6u, 7u)), bufferDescriptor(4u, Format32Float, selectors(1u, 5u, 6u, 7u)), "the X selector of a buffer read with buffer_load_format_x");
}

void formattedPairLoadKeepsAVariantPerSelectorItReads() {
    const auto code = bufferProgram(BufferLoadFormatXy, BufferStoreDwordx2, 0x20u);
    requireTwoVariants(code, bufferDescriptor(4u, Format16x2Float, selectors(4u, 5u, 6u, 7u)), bufferDescriptor(4u, Format16x2Float, selectors(4u, 0u, 6u, 7u)), "the Y selector of a buffer read with buffer_load_format_xy");
}

void formattedLoadKeepsAVariantPerFormat() {
    const auto code = bufferProgram(BufferLoadFormatX, BufferStoreDword, 0x30u);
    requireTwoVariants(code, bufferDescriptor(4u, Format32Float, selectors(4u, 5u, 6u, 7u)), bufferDescriptor(4u, Format16Float, selectors(4u, 5u, 6u, 7u)), "the format of a buffer read with buffer_load_format_x");
}

void variantLimitEvictsTheLeastRecentlyUsedVariant() {
    const auto code = bufferProgram(BufferLoadDword, BufferStoreDword, 0x40u);
    const auto compileStride = [&code](std::uint32_t stride) {
        return compileBuffer(code, bufferDescriptor(stride, 0u, selectors(4u, 5u, 6u, 7u)));
    };
    std::vector<RecompileResult> first;
    for (std::uint32_t variant = 1; variant <= VariantLimit; ++variant) {
        first.push_back(compileStride(4u * variant));
        require(!first.back().cacheHit, "a new buffer stride reused a variant");
    }
    require(compileStride(4u).cacheHit, "a resident variant was compiled again");
    require(!compileStride(4u * (VariantLimit + 1u)).cacheHit, "a buffer stride past the variant limit reused a variant");
    require(compileStride(4u).cacheHit, "the most recently used variant was evicted");
    const auto recompiled = compileStride(8u);
    require(!recompiled.cacheHit, "the least recently used variant was not evicted");
    require(recompiled.spirv == first[1].spirv && recompiled.spirvHash == first[1].spirvHash, "compiling an evicted variant again changed its SPIR-V");
    require(!compileStride(12u).cacheHit, "the next least recently used variant was not evicted");
    require(compileStride(20u).cacheHit, "a variant used after the evicted ones was evicted");
}

void skippedLightingProgramsCompile() {
    for (const auto request : SkippedLightingRequests) {
        const auto result = RecompileSerialized(request);
        require(!result.spirv.empty() && !result.bindings.empty(), "a skipped lighting program compiled to an empty module");
    }
}

void planeSampleOfCubeMajorityTableBindsItsPlaneEntry() {
    const auto result = sampleLightTable(SampleLz2D, {TextureCube, TextureCube, Texture2D, TextureCube});
    requireImage(result, Texture2D, DescriptorImageShape::Image2D, "a 2D sample of a cube-majority table did not bind the table's 2D entry");
}

void cubeSampleOfCubeMajorityTableBindsACubeEntry() {
    const auto result = sampleLightTable(SampleLzCube, {TextureCube, TextureCube, Texture2D, TextureCube});
    requireImage(result, TextureCube, DescriptorImageShape::ImageCube, "a cube sample of a cube-majority table did not bind a cube entry");
}

void tableWithoutAnEntryOfTheSampleShapeBindsANullImage() {
    const auto result = sampleLightTable(SampleLz2D, {Texture3D, TextureCube, NullTexture, TextureCube});
    requireImage(result, NullTexture, DescriptorImageShape::Image2D, "a 2D sample of a table without 2D entries did not bind a null image");
}

void planeSampleOfLineArrayMajorityTableBindsItsPlaneEntry() {
    const auto result = sampleLightTable(SampleLz2D, {Texture1DArray, Texture1DArray, Texture2D, Texture1DArray});
    requireImage(result, Texture2D, DescriptorImageShape::Image2D, "a 2D sample of a table whose most common entry is a 1D array did not bind the table's 2D entry");
}

void cubeSampleOfPlaneMajorityTableBindsItsCubeEntry() {
    const auto result = sampleLightTable(SampleLzCube, {Texture2D, TextureCube, Texture2D, Texture2D});
    requireImage(result, TextureCube, DescriptorImageShape::ImageCube, "a cube sample of a table whose most common entry is 2D did not prefer the table's cube entry");
}

void volumeSampleOfTableSkipsDataAboveApplicationMemory() {
    const auto result = sampleLightTable(SampleLz3D, {LightDataAboveApplicationMemory, Texture3D, LightDataAboveApplicationMemory, NullTexture});
    requireImage(result, Texture3D, DescriptorImageShape::Image3D, "a 3D sample of a light table bound CPU data that decodes as a 3D image above 1 TiB");
}

void pointerTableSampleBindsTheMostCommonEntry() {
    const auto first = atAddress(Texture2D, 0x21u);
    const auto common = atAddress(Texture2D, 0x22u);
    const std::array<TextureDescriptor, 7> entries{first, common, common, HostPointers, first, first, first};
    const auto result = samplePointerTable(SampleLz2D, entries);
    requireImage(result, common, DescriptorImageShape::Image2D, "a 2D sample through a pointer table did not bind the most common 2D entry before the first entry it cannot sample");
    require(result.unresolvedImages == 0u, "a 2D sample through a pointer table was reported as a null texture");
}

void pointerTableEndsAtANullEntry() {
    const auto first = atAddress(Texture2D, 0x21u);
    const auto late = atAddress(Texture2D, 0x22u);
    const std::array<TextureDescriptor, 4> entries{first, NullTexture, late, late};
    const auto result = samplePointerTable(SampleLz2D, entries);
    requireImage(result, first, DescriptorImageShape::Image2D, "a pointer table did not end at its first null entry");
}

void pointerTableStartingWithANullEntryReadsAsNull() {
    const auto first = atAddress(Texture2D, 0x21u);
    const std::array<TextureDescriptor, 3> entries{NullTexture, first, first};
    const auto result = samplePointerTable(SampleLz2D, entries);
    requireImage(result, NullTexture, DescriptorImageShape::Image2D, "a pointer table whose first entry is null was read past that entry");
}

void volumeSampleOfPointerTableEndsAtDataAboveApplicationMemory() {
    const std::array<TextureDescriptor, 3> entries{Texture3D, LightDataAboveApplicationMemory, LightDataAboveApplicationMemory};
    const auto result = samplePointerTable(SampleLz3D, entries);
    requireImage(result, Texture3D, DescriptorImageShape::Image3D, "a 3D sample through a pointer table bound CPU data that decodes as a 3D image above 1 TiB");
}

void pointerTableEndsAtItsFirstUnreadableEntry() {
    const auto first = atAddress(Texture2D, 0x21u);
    const auto late = atAddress(Texture2D, 0x22u);
    const std::array<TextureDescriptor, 3> head{first, first, late};
    const std::array<TextureDescriptor, 6> tail{late, late, late, late, late, late};
    const auto headBytes = tableBytes(head);
    const auto tailBytes = tableBytes(tail);
    const std::array<MemoryRegion, 2> memory{{
        {TableAddress + PointerTableOffset, headBytes},
        {TableAddress + PointerTableOffset + 4u * sizeof(TextureDescriptor), tailBytes},
    }};
    const auto result = samplePointerTableMemory(SampleLz2D, ShiftS8By5IntoS9, memory);
    requireImage(result, first, DescriptorImageShape::Image2D, "a pointer table did not end at its first unreadable entry");
}

void pointerTableIsReadUpTo256Entries() {
    const auto first = atAddress(Texture2D, 0x21u);
    const auto late = atAddress(Texture2D, 0x22u);
    std::vector<TextureDescriptor> entries(PointerTableEntries + 44u, late);
    std::fill(entries.begin(), entries.begin() + PointerTableEntries / 2u + 1u, first);
    const auto result = samplePointerTable(SampleLz2D, entries);
    requireImage(result, first, DescriptorImageShape::Image2D, "a pointer table was read past 256 entries");
}

void cubeSampleOfPointerTableEndsAtAnotherImageType() {
    const auto array = atAddress(Texture2DArray, 0x71u);
    const auto laterCube = atAddress(TextureCube, 0x11u);
    const std::array<TextureDescriptor, 6> entries{TextureCube, array, array, array, laterCube, laterCube};
    const auto result = samplePointerTable(SampleLzCube, entries);
    requireImage(result, TextureCube, DescriptorImageShape::ImageCube, "a cube sample through a pointer table did not end the table at its first entry of another image type");
}

void descriptorOutsideATableStillReadsAsNull() {
    const std::array<TextureDescriptor, 4> entries{Texture2D, Texture2D, Texture2D, Texture2D};
    const auto bytes = tableBytes(entries);
    const std::array<MemoryRegion, 1> memory{MemoryRegion{TableAddress + PointerTableOffset, bytes}};
    const auto result = samplePointerTableMemory(SampleLz2D, ShiftS8By4IntoS9, memory);
    requireImage(result, NullTexture, DescriptorImageShape::Image2D, "a descriptor read at a 16-byte index scale was resolved as a table of image descriptors");
    require(result.unresolvedImages == 1u, "an image whose descriptor is selected at run time and cannot be resolved was not reported");
}

void arraySampleOfPlaneTextureReadsTheLodAfterTheSlice() {
    const auto result = sampleTexture(SampleL2DArray, Texture2D);
    requireImage(result, Texture2D, DescriptorImageShape::Image2DArray, "a 2D-array sample of a 2D texture was not specialized as an array");
    require(sampledLodBits(result) == TwoBits, "a 2D-array sample of a 2D texture did not read the LOD after the slice");
}

void planeSampleOfLineTextureKeepsTheLineShape() {
    const auto result = sampleTexture(SampleL2D, Texture1D);
    requireImage(result, Texture1D, DescriptorImageShape::Image1D, "a 2D sample of a 1D texture did not keep the 1D shape");
    require(sampledLodBits(result) == OneBits, "a 2D sample of a 1D texture did not read the LOD after both coordinates");
}

void lineArraySampleOfLineTextureKeepsTheLineShape() {
    const auto result = sampleTexture(SampleL1DArray, Texture1D);
    requireImage(result, Texture1D, DescriptorImageShape::Image1D, "a 1D-array sample of a 1D texture did not keep the 1D shape");
    require(sampledLodBits(result) == OneBits, "a 1D-array sample of a 1D texture did not read the LOD after the slice");
}

void volumeSampleOfPlaneTexturesKeepsThePlaneShape() {
    for (const auto& texture : {Texture2D, Texture2DArray}) {
        const auto result = sampleTexture(SampleL3D, texture);
        requireImage(result, texture, DescriptorImageShape::Image2D, "a 3D sample of a 2D or 2D-array texture did not keep a 2D shape");
        require(sampledLodBits(result) == TwoBits, "a 3D sample of a 2D or 2D-array texture did not read the LOD after all three coordinates");
    }
}

void planeSampleReadsTheLodAfterBothCoordinates() {
    const auto result = sampleTexture(SampleL2D, Texture2D);
    requireImage(result, Texture2D, DescriptorImageShape::Image2D, "a 2D sample of a 2D texture was not specialized as 2D");
    require(sampledLodBits(result) == OneBits, "a 2D sample of a 2D texture did not read the LOD after both coordinates");
}

void planeSampleOfCubeTextureKeepsTheInstructionShape() {
    const auto result = sampleTexture(SampleLz2D, TextureCube);
    requireImage(result, TextureCube, DescriptorImageShape::Image2D, "a 2D sample of a cube texture was not specialized as 2D");
}

void planeSampleOfVolumeTextureKeepsTheInstructionShape() {
    const auto result = sampleTexture(SampleLz2D, Texture3D);
    requireImage(result, Texture3D, DescriptorImageShape::Image2D, "a 2D sample of a 3D texture was not specialized as 2D");
}

namespace Synthetic = ShaderRecompiler::SyntheticPrograms;

constexpr std::uint32_t OpExecutionMode = 16u;
constexpr std::uint32_t OpCapability = 17u;
constexpr std::uint32_t OpTypeInt = 21u;
constexpr std::uint32_t OpTypeVector = 23u;
constexpr std::uint32_t OpTypeRuntimeArray = 29u;
constexpr std::uint32_t OpDecorate = 71u;
constexpr std::uint32_t ExecutionModeLocalSize = 17u;
constexpr std::uint32_t DecorationArrayStride = 6u;
constexpr std::uint32_t CapabilityGroupNonUniform = 61u;
constexpr std::uint64_t SyntheticInput = 0x40000u;
constexpr std::uint64_t SyntheticOutput = 0x50000u;
constexpr std::uint32_t SyntheticRecords = 4096u;

template<typename TVisit>
void forEachInstruction(const RecompileResult& result, TVisit&& visit) {
    const std::span<const std::uint32_t> spirv(result.spirv.data(), result.spirv.size());
    for (std::size_t word = 5; word < spirv.size();) {
        const auto count = spirv[word] >> 16u;
        require(count != 0u && word + count <= spirv.size(), "the SPIR-V module is malformed");
        visit(spirv[word] & 0xffffu, spirv.subspan(word, count));
        word += count;
    }
}

std::array<std::uint32_t, 3> localSize(const RecompileResult& result) {
    std::array<std::uint32_t, 3> size{};
    forEachInstruction(result, [&](std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
        if (opcode == OpExecutionMode && instruction.size() == 6u && instruction[2] == ExecutionModeLocalSize) {
            size = {instruction[3], instruction[4], instruction[5]};
        }
    });
    return size;
}

bool declaresCapability(const RecompileResult& result, std::uint32_t capability) {
    bool found = false;
    forEachInstruction(result, [&](std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
        found = found || (opcode == OpCapability && instruction[1] == capability);
    });
    return found;
}

bool declaresVectorView(const RecompileResult& result, std::uint32_t components) {
    std::vector<std::uint32_t> uintTypes;
    std::vector<std::uint32_t> vectorTypes;
    std::vector<std::uint32_t> arrays;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> strides;
    forEachInstruction(result, [&](std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
        if (opcode == OpTypeInt && instruction.size() == 4u && instruction[2] == 32u && instruction[3] == 0u) {
            uintTypes.push_back(instruction[1]);
        } else if (opcode == OpTypeVector && instruction.size() == 4u && instruction[3] == components && std::ranges::find(uintTypes, instruction[2]) != uintTypes.end()) {
            vectorTypes.push_back(instruction[1]);
        } else if (opcode == OpTypeRuntimeArray && instruction.size() == 3u && std::ranges::find(vectorTypes, instruction[2]) != vectorTypes.end()) {
            arrays.push_back(instruction[1]);
        } else if (opcode == OpDecorate && instruction.size() == 4u && instruction[2] == DecorationArrayStride) {
            strides.emplace_back(instruction[1], instruction[3]);
        }
    });
    return std::ranges::any_of(arrays, [&](std::uint32_t array) {
        return std::ranges::find(strides, std::make_pair(array, components * 4u)) != strides.end();
    });
}

RecompileResult compileSynthetic(const Synthetic::ComputeProgram& program, std::uint32_t waveSize, bool dualLane) {
    return Recompile(program.Request(waveSize, 32u, dualLane));
}

void perThreadWave64ProgramRunsOneLanePerInvocation() {
    const auto program = Synthetic::PerThreadProgram({16u, 16u, 1u}, 300u, 100u, SyntheticInput, SyntheticOutput, SyntheticRecords);
    const auto result = compileSynthetic(program, 64u, false);
    require(result.lanesPerInvocation == 1u, "a wave64 program without wave operations or LDS was compiled with two guest lanes per invocation");
    require(localSize(result) == std::array<std::uint32_t, 3>{16u, 16u, 1u}, "a single-lane wave64 program did not keep the guest workgroup size");
    require(!declaresCapability(result, CapabilityGroupNonUniform), "a single-lane wave64 program still requires subgroup operations");
}

void dualLaneSwitchKeepsTwoLanesPerInvocation() {
    const auto program = Synthetic::PerThreadProgram({16u, 16u, 1u}, 300u, 100u, SyntheticInput, SyntheticOutput, SyntheticRecords);
    const auto result = compileSynthetic(program, 64u, true);
    require(result.lanesPerInvocation == 2u, "the dual-lane switch did not keep two guest lanes per invocation");
    require(localSize(result) == std::array<std::uint32_t, 3>{128u, 1u, 1u}, "a dual-lane wave64 program did not fold its workgroup into half as many invocations");
    require(!(result.spirv == compileSynthetic(program, 64u, false).spirv), "the dual-lane switch did not change the compiled program");
}

void readFirstLaneKeepsTwoLanesPerInvocation() {
    const auto result = compileSynthetic(Synthetic::ReadFirstLaneProgram(SyntheticInput, SyntheticOutput, SyntheticRecords), 64u, false);
    require(result.lanesPerInvocation == 2u, "a wave64 program with v_readfirstlane was compiled with one guest lane per invocation");
    require(localSize(result) == std::array<std::uint32_t, 3>{32u, 1u, 1u}, "a dual-lane wave64 program did not fold its workgroup into half as many invocations");
}

void execReadAsScalarKeepsTwoLanesPerInvocation() {
    const auto result = compileSynthetic(Synthetic::ExecMaskProgram(SyntheticInput, SyntheticOutput, SyntheticRecords), 64u, false);
    require(result.lanesPerInvocation == 2u, "a wave64 program that reads EXEC as a scalar was compiled with one guest lane per invocation");
    require(declaresCapability(result, CapabilityGroupNonUniform), "a wave64 program that reads EXEC as a scalar does not ballot the entry EXEC");
}

void sharedMemoryKeepsTwoLanesPerInvocation() {
    const auto result = compileSynthetic(Synthetic::SharedMemoryProgram(SyntheticInput, SyntheticOutput, SyntheticRecords), 64u, false);
    require(result.lanesPerInvocation == 2u, "a wave64 program that uses LDS was compiled with one guest lane per invocation");
}

void vccBranchKeepsTwoLanesPerInvocation() {
    const auto result = compileSynthetic(Synthetic::VccBranchProgram({64u, 1u, 1u}, 100u, SyntheticInput, SyntheticOutput, SyntheticRecords), 64u, false);
    require(result.lanesPerInvocation == 2u, "a wave64 program that branches on VCC was compiled with one guest lane per invocation");
}

void barrierKeepsTwoLanesPerInvocation() {
    const auto result = compileSynthetic(Synthetic::BarrierProgram(SyntheticInput, SyntheticOutput, SyntheticRecords), 64u, false);
    require(result.lanesPerInvocation == 2u, "a wave64 program with s_barrier was compiled with one guest lane per invocation");
}

void workgroupAboveDeviceLimitsKeepsTwoLanesPerInvocation() {
    const auto program = Synthetic::PerThreadProgram({1u, 1u, 128u}, 128u, 64u, SyntheticInput, SyntheticOutput, SyntheticRecords);
    const auto result = compileSynthetic(program, 64u, false);
    require(result.lanesPerInvocation == 2u, "a wave64 workgroup deeper than the device allows was compiled with one guest lane per invocation");
    require(localSize(result) == std::array<std::uint32_t, 3>{64u, 1u, 1u}, "a dual-lane wave64 program did not fold its workgroup into half as many invocations");
}

void wave32ProgramRunsOneLanePerInvocation() {
    const auto program = Synthetic::PerThreadProgram({8u, 8u, 1u}, 64u, 32u, SyntheticInput, SyntheticOutput, SyntheticRecords);
    const auto result = compileSynthetic(program, 32u, false);
    require(result.lanesPerInvocation == 1u && localSize(result) == std::array<std::uint32_t, 3>{8u, 8u, 1u}, "a wave32 program on a 32-wide subgroup did not keep one lane per invocation");
}

void rawWideLoadsReadThroughVectorViews() {
    for (const std::uint32_t dwords : {2u, 4u}) {
        const auto program = Synthetic::WideLoadProgram(dwords, {64u, 1u, 1u}, 4u, 0u, SyntheticInput, 256u, SyntheticOutput, 1024u);
        const auto result = compileSynthetic(program, 64u, false);
        require(declaresVectorView(result, dwords), "a raw buffer_load_dwordx" + std::to_string(dwords) + " did not read through a vector view");
    }
    const auto program = Synthetic::WideLoadProgram(3u, {64u, 1u, 1u}, 4u, 0u, SyntheticInput, 256u, SyntheticOutput, 1024u);
    const auto result = compileSynthetic(program, 64u, false);
    require(!declaresVectorView(result, 3u) && !declaresVectorView(result, 4u), "a raw buffer_load_dwordx3 declared a vector view");
}

}

int main() {
    const std::array<std::pair<const char*, void (*)()>, 36> tests{{
        {"a wave64 compute program without wave operations or LDS runs one guest lane per invocation with the guest workgroup size", &perThreadWave64ProgramRunsOneLanePerInvocation},
        {"the dual-lane switch keeps two guest lanes per invocation", &dualLaneSwitchKeepsTwoLanesPerInvocation},
        {"a wave64 program with v_readfirstlane keeps two guest lanes per invocation", &readFirstLaneKeepsTwoLanesPerInvocation},
        {"a wave64 program that reads EXEC as a scalar keeps two guest lanes per invocation", &execReadAsScalarKeepsTwoLanesPerInvocation},
        {"a wave64 program that uses LDS keeps two guest lanes per invocation", &sharedMemoryKeepsTwoLanesPerInvocation},
        {"a wave64 program that branches on VCC keeps two guest lanes per invocation", &vccBranchKeepsTwoLanesPerInvocation},
        {"a wave64 program with s_barrier keeps two guest lanes per invocation", &barrierKeepsTwoLanesPerInvocation},
        {"a wave64 workgroup above the device limits keeps two guest lanes per invocation", &workgroupAboveDeviceLimitsKeepsTwoLanesPerInvocation},
        {"a wave32 program on a 32-wide subgroup runs one lane per invocation", &wave32ProgramRunsOneLanePerInvocation},
        {"raw buffer_load_dwordx2 and x4 read through vector views and x3 does not", &rawWideLoadsReadThroughVectorViews},
        {"skipped lighting programs from run 27 compile", &skippedLightingProgramsCompile},
        {"a 2D sample of a cube-majority table binds its 2D entry", &planeSampleOfCubeMajorityTableBindsItsPlaneEntry},
        {"a cube sample of a cube-majority table binds a cube entry", &cubeSampleOfCubeMajorityTableBindsACubeEntry},
        {"a 2D sample of a table without 2D entries binds a null image", &tableWithoutAnEntryOfTheSampleShapeBindsANullImage},
        {"a 2D sample of a 1D-array-majority table binds its 2D entry", &planeSampleOfLineArrayMajorityTableBindsItsPlaneEntry},
        {"a cube sample of a 2D-majority table prefers its cube entry", &cubeSampleOfPlaneMajorityTableBindsItsCubeEntry},
        {"a 3D sample of a light table skips CPU data that decodes as a 3D image above 1 TiB", &volumeSampleOfTableSkipsDataAboveApplicationMemory},
        {"a T# loaded with s_load_dwordx8 from a pointer plus a scaled index binds the most common entry before the first entry it cannot sample", &pointerTableSampleBindsTheMostCommonEntry},
        {"a pointer table ends at its first unreadable entry", &pointerTableEndsAtItsFirstUnreadableEntry},
        {"a pointer table ends at its first null entry", &pointerTableEndsAtANullEntry},
        {"a pointer table whose first entry is null reads as null", &pointerTableStartingWithANullEntryReadsAsNull},
        {"a 3D sample through a pointer table ends at CPU data that decodes as a 3D image above 1 TiB", &volumeSampleOfPointerTableEndsAtDataAboveApplicationMemory},
        {"a pointer table is read up to 256 entries", &pointerTableIsReadUpTo256Entries},
        {"a cube sample through a pointer table ends the table at its first entry of another image type", &cubeSampleOfPointerTableEndsAtAnotherImageType},
        {"a descriptor read at an index scale below 32 bytes still reads as null and is reported", &descriptorOutsideATableStillReadsAsNull},
        {"a 2D sample of a 2D texture reads the LOD after both coordinates", &planeSampleReadsTheLodAfterBothCoordinates},
        {"a 2D-array sample of a 2D texture reads the LOD after the slice", &arraySampleOfPlaneTextureReadsTheLodAfterTheSlice},
        {"a 2D sample of a 1D texture keeps the 1D shape", &planeSampleOfLineTextureKeepsTheLineShape},
        {"a 1D-array sample of a 1D texture keeps the 1D shape", &lineArraySampleOfLineTextureKeepsTheLineShape},
        {"a 3D sample of a 2D or 2D-array texture keeps a 2D shape", &volumeSampleOfPlaneTexturesKeepsThePlaneShape},
        {"a 2D sample of a cube texture keeps the instruction shape", &planeSampleOfCubeTextureKeepsTheInstructionShape},
        {"a 2D sample of a 3D texture keeps the instruction shape", &planeSampleOfVolumeTextureKeepsTheInstructionShape},
        {"buffer_load_format_x keeps a variant per selector it reads", &formattedLoadKeepsAVariantPerSelectorItReads},
        {"buffer_load_format_xy keeps a variant per selector it reads", &formattedPairLoadKeepsAVariantPerSelectorItReads},
        {"buffer_load_format_x keeps a variant per descriptor format", &formattedLoadKeepsAVariantPerFormat},
        {"the variant limit evicts the least recently used variant", &variantLimitEvictsTheLeastRecentlyUsedVariant},
    }};
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::printf("passed: %s\n", name);
        } catch (const std::exception& error) {
            ++failures;
            std::printf("FAILED: %s: %s\n", name, failureReason(error.what()).c_str());
        }
    }
    if (failures != 0) {
        std::printf("%d recompiler test(s) failed\n", failures);
        return 1;
    }
    std::printf("Recompiler tests passed\n");
    return 0;
}

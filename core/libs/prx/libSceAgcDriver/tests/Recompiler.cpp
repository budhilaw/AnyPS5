#include "../../../../shader/recompiler/Recompiler.hpp"
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

constexpr std::uint32_t SampleLz2D = 0xf09c0f08u;
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

constexpr TextureDescriptor NullTexture{};
constexpr TextureDescriptor Texture1D{0x00000050u, 56u << 20u, 0u, 0x80000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor Texture2D{0x00000020u, 56u << 20u, 0u, 0x90000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor TextureCube{0x00000010u, 56u << 20u, 0u, 0xb0000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor Texture3D{0x00000040u, 56u << 20u, 0u, 0xa0000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor Texture1DArray{0x00000060u, 56u << 20u, 0u, 0xc0000facu, 0u, 0u, 0u, 0u};
constexpr TextureDescriptor Texture2DArray{0x00000070u, 56u << 20u, 0u, 0xd0000facu, 0u, 0u, 0u, 0u};
constexpr std::array<std::uint32_t, 4> OutputBuffer{0x30000u, 0u, 16u, 0x00000facu};

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

}

int main() {
    const std::array<std::pair<const char*, void (*)()>, 12> tests{{
        {"skipped lighting programs from run 27 compile", &skippedLightingProgramsCompile},
        {"a 2D sample of a cube-majority table binds its 2D entry", &planeSampleOfCubeMajorityTableBindsItsPlaneEntry},
        {"a cube sample of a cube-majority table binds a cube entry", &cubeSampleOfCubeMajorityTableBindsACubeEntry},
        {"a 2D sample of a table without 2D entries binds a null image", &tableWithoutAnEntryOfTheSampleShapeBindsANullImage},
        {"a 2D sample of a 1D-array-majority table binds its 2D entry", &planeSampleOfLineArrayMajorityTableBindsItsPlaneEntry},
        {"a 2D sample of a 2D texture reads the LOD after both coordinates", &planeSampleReadsTheLodAfterBothCoordinates},
        {"a 2D-array sample of a 2D texture reads the LOD after the slice", &arraySampleOfPlaneTextureReadsTheLodAfterTheSlice},
        {"a 2D sample of a 1D texture keeps the 1D shape", &planeSampleOfLineTextureKeepsTheLineShape},
        {"a 1D-array sample of a 1D texture keeps the 1D shape", &lineArraySampleOfLineTextureKeepsTheLineShape},
        {"a 3D sample of a 2D or 2D-array texture keeps a 2D shape", &volumeSampleOfPlaneTexturesKeepsThePlaneShape},
        {"a 2D sample of a cube texture keeps the instruction shape", &planeSampleOfCubeTextureKeepsTheInstructionShape},
        {"a 2D sample of a 3D texture keeps the instruction shape", &planeSampleOfVolumeTextureKeepsTheInstructionShape},
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
    std::printf("Recompiler image dimension tests passed\n");
    return 0;
}

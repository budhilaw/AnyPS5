#include "Recompiler.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using namespace ShaderRecompiler;

namespace {

using BufferDescriptor = std::array<std::uint32_t, 4>;
using BufferUserData = std::array<std::uint32_t, 8>;

constexpr std::uint64_t CodeAddress = 0x20000u;
constexpr std::uint32_t BufferLoadFormatX = 0xe0002000u;
constexpr std::uint32_t BufferLoadFormatXy = 0xe0042000u;
constexpr std::uint32_t BufferStoreFormatX = 0xe0102000u;
constexpr std::uint32_t BufferLoadDword = 0xe0302000u;
constexpr std::uint32_t BufferStoreDword = 0xe0702000u;
constexpr std::uint32_t BufferStoreDwordx2 = 0xe0742000u;
constexpr std::uint32_t TbufferLoadFormatX32Uint = 0xe8a02000u;
constexpr std::uint32_t InputOperands = 0x80000100u;
constexpr std::uint32_t OutputOperands = 0x80010100u;
constexpr std::uint32_t EndProgram = 0xbf810000u;
constexpr std::uint32_t InputBufferAddress = 0x40000u;
constexpr std::uint32_t IdentitySelectors = DstSel(4u, 5u, 6u, 7u);
constexpr BufferDescriptor OutputBuffer{0x30000u, 0u, 16u, IdentitySelectors};

struct MaterializedRequest {
    ResourceSnapshot snapshot;
    ResourceSpecialization normalized;
    ResourceSpecialization exact;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string failureReason(std::string_view message) {
    return std::string(message.substr(0, message.find("\nRecompileRequest:")));
}

constexpr BufferDescriptor bufferDescriptor(std::uint32_t stride, IrBufferFormat format, std::uint32_t selectors) {
    return {InputBufferAddress, stride << 16u, 64u, (static_cast<std::uint32_t>(format) << 12u) | selectors};
}

constexpr std::array<std::uint32_t, 5> bufferProgram(std::uint32_t load, std::uint32_t store, std::uint32_t storeOffset) {
    return {load, InputOperands, store | storeOffset, OutputOperands, EndProgram};
}

constexpr BufferUserData bufferUserData(const BufferDescriptor& input, const BufferDescriptor& output) {
    return {input[0], input[1], input[2], input[3], output[0], output[1], output[2], output[3]};
}

RecompileRequest bufferRequest(std::span<const std::uint32_t> code, const BufferUserData& userData) {
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, CodeAddress, code, 0u, {}};
    request.context.waveSize = 32u;
    request.context.userDataBaseRegister = 0u;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32u;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024u;
    request.target.maxWorkgroupSharedMemoryBytes = 65536u;
    request.layout.pushConstantSizeBytes = 128u;
    return request;
}

MaterializedRequest materialize(const RecompileRequest& request) {
    const auto plan = GetResourcePlan(request);
    RequestMemoryView memory(request.context.memory);
    const auto runtime = memory.MakeRuntime(request.context.userData, request.shader.codeAddress);
    MaterializedRequest result;
    ResourceMaterializer{}.Materialize(*plan, runtime, result.snapshot, result.normalized);
    result.exact = result.normalized;
    for (std::size_t index = 0; index < result.exact.buffers.size(); ++index) {
        ShaderBufferResource descriptor;
        std::copy_n(result.snapshot.buffers[index].dwords.begin(), descriptor.fields.size(), descriptor.fields.begin());
        const bool formatted = plan->info.buffers[index].formatted;
        result.exact.buffers[index].descriptorFormat = formatted ? descriptor.Format() : IrBufferFormat::Invalid;
        result.exact.buffers[index].descriptorSwizzle = formatted ? descriptor.DstSelXYZW() : IdentitySelectors;
    }
    return result;
}

RecompileResult compileExact(RecompileRequest request, const MaterializedRequest& materialized) {
    request.materializedSnapshot = &materialized.snapshot;
    request.materializedSpecialization = &materialized.exact;
    return Recompile(request);
}

void requireSameProgram(const RecompileResult& left, const RecompileResult& right, bool sameDescriptors, const std::string& message) {
    require(left.spirv == right.spirv && left.spirvHash == right.spirvHash, message + ": the SPIR-V differs");
    require(left.bindings.size() == right.bindings.size(), message + ": the binding count differs");
    for (std::size_t index = 0; index < left.bindings.size(); ++index) {
        const auto& a = left.bindings[index];
        const auto& b = right.bindings[index];
        require(a.kind == b.kind && a.role == b.role && a.descriptorSet == b.descriptorSet && a.binding == b.binding && a.count == b.count && a.readOnly == b.readOnly, message + ": a binding differs");
        require(a.elementWritten == b.elementWritten && a.elementOptional == b.elementOptional && a.elementRead == b.elementRead, message + ": the buffer element metadata differs");
        require(a.imageShape == b.imageShape && a.samplerDepthCompare == b.samplerDepthCompare && a.imageDepthCompare == b.imageDepthCompare, message + ": the image or sampler metadata differs");
        require(!sameDescriptors || a.guestDescriptor == b.guestDescriptor, message + ": the guest descriptors differ");
    }
    require(left.pushConstants.size() == right.pushConstants.size() && (!sameDescriptors || left.pushConstants == right.pushConstants), message + ": the push constants differ");
    require(left.bdaAbiVersion == right.bdaAbiVersion && left.unresolvedImages == right.unresolvedImages && left.vertexOffsetSgpr == right.vertexOffsetSgpr && left.instanceOffsetSgpr == right.instanceOffsetSgpr, message + ": the program metadata differs");
    require(left.parameterExports == right.parameterExports && left.vertexAttributes.size() == right.vertexAttributes.size() && left.fragmentParameters.size() == right.fragmentParameters.size(), message + ": the stage interface differs");
}

void requireSharedVariant(std::span<const std::uint32_t> code, const BufferUserData& firstUserData, const BufferUserData& secondUserData, const std::string& field) {
    const auto first = bufferRequest(code, firstUserData);
    const auto second = bufferRequest(code, secondUserData);
    const auto firstMaterialized = materialize(first);
    const auto secondMaterialized = materialize(second);
    require(!(firstMaterialized.exact == secondMaterialized.exact), field + ": the descriptors share an exact specialization");
    require(firstMaterialized.normalized == secondMaterialized.normalized, field + " is still part of the specialization");
    const auto firstExact = compileExact(first, firstMaterialized);
    const auto secondExact = compileExact(second, secondMaterialized);
    require(!secondExact.cacheHit, field + ": the exact specialization of the second descriptors was not compiled");
    requireSameProgram(firstExact, secondExact, false, field + " changed the compiled program");
    const auto firstShared = Recompile(first);
    const auto secondShared = Recompile(second);
    require(secondShared.cacheHit, field + " compiled a second variant");
    requireSameProgram(firstShared, firstExact, true, field + ": the shared variant differs from the exact variant of the first descriptors");
    requireSameProgram(secondShared, secondExact, true, field + ": the shared variant differs from the exact variant of the second descriptors");
}

void formattedLoadIgnoresSelectorsOfComponentsItDoesNotRead() {
    const auto code = bufferProgram(BufferLoadFormatX, BufferStoreDword, 0x10u);
    const auto first = bufferUserData(bufferDescriptor(4u, IrBufferFormat::Format32Float, IdentitySelectors), OutputBuffer);
    const auto second = bufferUserData(bufferDescriptor(4u, IrBufferFormat::Format32Float, DstSel(4u, 0u, 1u, 0u)), OutputBuffer);
    requireSharedVariant(code, first, second, "the Y, Z and W selectors of a buffer read with buffer_load_format_x");
}

void formattedPairLoadIgnoresTheZAndWSelectors() {
    const auto code = bufferProgram(BufferLoadFormatXy, BufferStoreDwordx2, 0x20u);
    const auto first = bufferUserData(bufferDescriptor(4u, IrBufferFormat::Format16_16Float, IdentitySelectors), OutputBuffer);
    const auto second = bufferUserData(bufferDescriptor(4u, IrBufferFormat::Format16_16Float, DstSel(4u, 5u, 0u, 1u)), OutputBuffer);
    requireSharedVariant(code, first, second, "the Z and W selectors of a buffer read with buffer_load_format_xy");
}

void formattedStoreIgnoresTheSelectors() {
    const auto code = bufferProgram(BufferLoadDword, BufferStoreFormatX, 0x30u);
    const auto input = bufferDescriptor(4u, IrBufferFormat::Invalid, IdentitySelectors);
    const auto first = bufferUserData(input, bufferDescriptor(4u, IrBufferFormat::Format32Float, IdentitySelectors));
    const auto second = bufferUserData(input, bufferDescriptor(4u, IrBufferFormat::Format32Float, DstSel(0u, 1u, 5u, 4u)));
    requireSharedVariant(code, first, second, "the selectors of a buffer written with buffer_store_format_x");
}

void formattedLoadWithoutFormatIgnoresTheSelectors() {
    const auto code = bufferProgram(BufferLoadFormatX, BufferStoreDword, 0x40u);
    const auto first = bufferUserData(bufferDescriptor(4u, IrBufferFormat::Invalid, IdentitySelectors), OutputBuffer);
    const auto second = bufferUserData(bufferDescriptor(4u, IrBufferFormat::Invalid, DstSel(1u, 0u, 0u, 0u)), OutputBuffer);
    requireSharedVariant(code, first, second, "the selectors of a buffer without a format read with buffer_load_format_x");
}

void typedLoadIgnoresTheDescriptorFormatAndSelectors() {
    const auto code = bufferProgram(TbufferLoadFormatX32Uint, BufferStoreDword, 0x50u);
    const auto first = bufferUserData(bufferDescriptor(4u, IrBufferFormat::Format32Float, IdentitySelectors), OutputBuffer);
    const auto second = bufferUserData(bufferDescriptor(4u, IrBufferFormat::Format16_16Float, DstSel(1u, 5u, 0u, 4u)), OutputBuffer);
    requireSharedVariant(code, first, second, "the descriptor format and selectors of a buffer read with tbuffer_load_format_x");
}

}

int main() {
    const std::array<std::pair<const char*, void (*)()>, 5> tests{{
        {"buffer_load_format_x compiles the selectors it does not read to the same program", &formattedLoadIgnoresSelectorsOfComponentsItDoesNotRead},
        {"buffer_load_format_xy compiles the Z and W selectors to the same program", &formattedPairLoadIgnoresTheZAndWSelectors},
        {"buffer_store_format_x compiles every selector to the same program", &formattedStoreIgnoresTheSelectors},
        {"buffer_load_format_x of a buffer without a format compiles every selector to the same program", &formattedLoadWithoutFormatIgnoresTheSelectors},
        {"tbuffer_load_format_x compiles every descriptor format and selector to the same program", &typedLoadIgnoresTheDescriptorFormatAndSelectors},
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
        std::printf("%d specialization test(s) failed\n", failures);
        return 1;
    }
    std::printf("Specialization tests passed\n");
    return 0;
}

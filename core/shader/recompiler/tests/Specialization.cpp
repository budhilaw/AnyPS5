#include "Recompiler.hpp"
#include "BdaAbi.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"

#include <spirv/unified1/spirv.hpp>
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
#include <vector>

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

constexpr std::uint64_t CopyTableAddress = 0x50000u;
constexpr std::uint32_t CopySlots = 8u;
constexpr std::uint32_t CopyDestinationDword = 0x80u / 4u;
constexpr std::uint32_t CopyElementCountDword = 0x100u / 4u;
constexpr std::uint32_t CopyCountDword = 0x120u / 4u;
constexpr std::uint32_t CopyResultDword = 0x130u / 4u;
constexpr std::uint32_t CopyDescriptorWord3 = 0x00014004u;
constexpr std::uint32_t CompareSlotBelowCountU32 = 0xbf0a026bu;
constexpr std::uint32_t CompareSlotBelowCountI32 = 0xbf04026bu;
constexpr std::uint32_t CompareCountAboveSlotU32 = 0xbf086b02u;
constexpr std::uint32_t BranchSccZeroToEnd = 0xbf840015u;
constexpr std::uint32_t BranchSccOneToLoop = 0xbf85ffe7u;
constexpr std::uint32_t Nop = 0xbf800000u;
constexpr BufferDescriptor FirstLeftover{0x10u, 0x145bu << 16u, 1u, 1u};
constexpr BufferDescriptor SecondLeftover{0x20u, 0x0a2cu << 16u, 1u, 1u};
constexpr std::array<std::uint32_t, 3> BdaCapabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
constexpr std::array<std::string_view, 2> BdaExtensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};

using CopyTable = std::array<std::uint32_t, 0x140u / 4u>;

constexpr std::array<std::uint32_t, 32> copyLoopProgram(std::uint32_t compare, bool testFirst) {
    return {0xbfa00001u, 0xd7460000u, 0x04010c02u, 0xbeeb0380u, 0xf4000080u, 0xfa000120u, 0xbf8cc07fu, testFirst ? compare : Nop,
            testFirst ? BranchSccZeroToEnd : Nop, 0x97eaff6bu, 0x00000100u, 0xf40000c0u, 0xd4000000u, 0xbf8cc07fu, 0x7da80003u, 0xbf88000bu,
            0x8f6a846bu, 0xf4080100u, 0xd4000000u, 0xbf8cc07fu, 0xe0002000u, 0x80010100u, 0xf4080200u, 0xd4000080u,
            0xbf8c0070u, 0xe0102000u, 0x80020100u, 0x816b816bu, 0xbefe04c1u, testFirst ? 0xbf82ffe8u : compare, testFirst ? EndProgram : BranchSccOneToLoop, EndProgram};
}

constexpr auto UnsignedCopyLoop = copyLoopProgram(CompareSlotBelowCountU32, true);
constexpr auto SignedCopyLoop = copyLoopProgram(CompareSlotBelowCountI32, true);
constexpr auto SwappedCopyLoop = copyLoopProgram(CompareCountAboveSlotU32, true);
constexpr auto BottomTestedCopyLoop = copyLoopProgram(CompareSlotBelowCountU32, false);
constexpr std::array<std::uint32_t, 23> LoadBeforeTestCopyLoop{
    0xbfa00001u, 0xd7460000u, 0x04010c02u, 0xbeeb0380u, 0xf4000080u, 0xfa000120u, 0xbf8cc07fu,
    0x8f6a846bu, 0xf4080100u, 0xd4000000u, 0xbf8cc07fu, 0xe0002000u, 0x80010100u,
    CompareSlotBelowCountU32, 0xbf840007u, 0xf4080200u, 0xd4000080u, 0xbf8c0070u, 0xe0102000u, 0x80020100u,
    0x816b816bu, 0xbf82fff0u, EndProgram};
constexpr std::array<std::uint32_t, 25> BranchOnCountThenCopyEightSlots{
    0xbfa00001u, 0xd7460000u, 0x04010c02u, 0xbeeb0380u, 0xf4000080u, 0xfa000120u, 0xbf8cc07fu,
    CompareSlotBelowCountU32, 0xbf840001u, Nop,
    0x8f6a846bu, 0xf4080100u, 0xd4000000u, 0xbf8cc07fu, 0xe0002000u, 0x80010100u,
    0xf4080200u, 0xd4000080u, 0xbf8c0070u, 0xe0102000u, 0x80020100u,
    0x816b816bu, 0xbf0a886bu, 0xbf85ffeeu, EndProgram};
constexpr std::array<std::uint32_t, 34> CopyLoopThenReadTheSlotAtTheCount{
    0xbfa00001u, 0xd7460000u, 0x04010c02u, 0xbeeb0380u, 0xf4000080u, 0xfa000120u, 0xbf8cc07fu,
    CompareSlotBelowCountU32, 0xbf84000du, 0x8f6a846bu, 0xf4080100u, 0xd4000000u, 0xbf8cc07fu, 0xe0002000u, 0x80010100u,
    0xf4080200u, 0xd4000080u, 0xbf8c0070u, 0xe0102000u, 0x80020100u, 0x816b816bu, 0xbf82fff0u,
    0x8f6a846bu, 0xf4080300u, 0xd4000000u, 0xf4080400u, 0xfa000130u, 0xbf8cc07fu, 0xe0002000u, 0x80030200u,
    0xbf8c0070u, 0xe0102000u, 0x80040200u, EndProgram};

constexpr BufferDescriptor copySource(std::uint32_t slot, std::uint32_t stride = 4u) {
    return {0x60000u + slot * 0x100u, stride << 16u, 16u, CopyDescriptorWord3};
}

constexpr BufferDescriptor copyDestination(std::uint32_t slot) {
    return {0x70000u + slot * 0x100u, 4u << 16u, 16u, CopyDescriptorWord3};
}

CopyTable copyTable(std::uint32_t count, std::uint32_t filledSlots, const BufferDescriptor& leftover) {
    CopyTable table{};
    for (std::uint32_t slot = 0; slot < CopySlots; ++slot) {
        const auto source = slot < filledSlots ? copySource(slot) : leftover;
        const auto destination = slot < filledSlots ? copyDestination(slot) : leftover;
        std::copy(source.begin(), source.end(), table.begin() + slot * 4u);
        std::copy(destination.begin(), destination.end(), table.begin() + CopyDestinationDword + slot * 4u);
        table[CopyElementCountDword + slot] = 16u;
    }
    table[CopyCountDword] = count;
    const auto result = copyDestination(CopySlots);
    std::copy(result.begin(), result.end(), table.begin() + CopyResultDword);
    return table;
}

struct CopyRequest {
    CopyRequest(std::span<const std::uint32_t> code, const CopyTable& contents) : table(contents) {
        memory[0] = MemoryRegion{CopyTableAddress, std::as_bytes(std::span(table))};
        request.shader = {ShaderStage::Compute, CodeAddress, code, 0u, {}};
        request.context.waveSize = 64u;
        request.context.userDataBaseRegister = 0u;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {true, false, false}, false, 1u};
        request.context.memory = memory;
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32u;
        request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
        request.target.maxWorkgroupInvocations = 1024u;
        request.target.maxWorkgroupSharedMemoryBytes = 65536u;
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.target.supportedCapabilities = BdaCapabilities;
        request.target.supportedExtensions = BdaExtensions;
        request.layout.pushConstantSizeBytes = 128u;
    }
    CopyRequest(const CopyRequest&) = delete;
    CopyRequest& operator=(const CopyRequest&) = delete;

    CopyTable table;
    std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(CopyTableAddress), static_cast<std::uint32_t>(CopyTableAddress >> 32u)};
    std::array<MemoryRegion, 1> memory{};
    RecompileRequest request{};
};

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

std::vector<BufferDescriptor> boundBuffers(const ResourceSnapshot& snapshot) {
    std::vector<BufferDescriptor> result;
    for (const auto& value : snapshot.buffers) {
        const BufferDescriptor descriptor{value.dwords[0], value.dwords[1], value.dwords[2], value.dwords[3]};
        if (descriptor != BufferDescriptor{}) {
            result.push_back(descriptor);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<BufferDescriptor> tableBuffers(const CopyTable& table, std::uint32_t slots) {
    std::vector<BufferDescriptor> result;
    for (std::uint32_t slot = 0; slot < slots; ++slot) {
        for (const auto first : {slot * 4u, CopyDestinationDword + slot * 4u}) {
            result.push_back({table[first], table[first + 1u], table[first + 2u], table[first + 3u]});
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void requireBoundSlots(std::span<const std::uint32_t> code, std::uint32_t count, std::uint32_t filledSlots, const BufferDescriptor& leftover, std::uint32_t boundSlots, const std::string& message) {
    const CopyRequest copy(code, copyTable(count, filledSlots, leftover));
    const auto materialized = materialize(copy.request);
    require(materialized.snapshot.buffers.size() == 2u * CopySlots, message + ": the loop does not read one source and one destination buffer per table slot");
    require(boundBuffers(materialized.snapshot) == tableBuffers(copy.table, boundSlots), message + ": the materialized buffers are not the table slots below " + std::to_string(boundSlots));
}

void leftoverSlotsPastTheLoopCountShareTheVariant() {
    const CopyRequest first(UnsignedCopyLoop, copyTable(2u, 2u, FirstLeftover));
    const CopyRequest second(UnsignedCopyLoop, copyTable(2u, 2u, SecondLeftover));
    const auto firstMaterialized = materialize(first.request);
    const auto secondMaterialized = materialize(second.request);
    require(boundBuffers(firstMaterialized.snapshot) == tableBuffers(first.table, 2u), "the slots past a loop count of 2 were bound");
    require(firstMaterialized.normalized == secondMaterialized.normalized, "leftover descriptors past the loop count are still part of the specialization");
    const auto firstResult = Recompile(first.request);
    const auto secondResult = Recompile(second.request);
    require(secondResult.cacheHit, "leftover descriptors past the loop count compiled a second variant");
    requireSameProgram(firstResult, secondResult, false, "leftover descriptors past the loop count");
    require(firstResult.pushConstants == secondResult.pushConstants, "leftover descriptors past the loop count changed the push constants");
    for (std::size_t index = 0; index < firstResult.bindings.size(); ++index) {
        if (firstResult.bindings[index].role == DescriptorRole::GuestBuffers) {
            require(firstResult.bindings[index].guestDescriptor == secondResult.bindings[index].guestDescriptor, "leftover descriptors past the loop count were bound");
        }
    }
}

void reachableSlotKeepsItsOwnVariant() {
    auto changed = copyTable(2u, 2u, FirstLeftover);
    const auto source = copySource(1u, 8u);
    std::copy(source.begin(), source.end(), changed.begin() + 4u);
    const CopyRequest first(UnsignedCopyLoop, copyTable(2u, 2u, FirstLeftover));
    const CopyRequest second(UnsignedCopyLoop, changed);
    require(!(materialize(first.request).normalized == materialize(second.request).normalized), "the stride of a slot below the loop count is not part of the specialization");
    const auto firstResult = Recompile(first.request);
    const auto secondResult = Recompile(second.request);
    require(!secondResult.cacheHit, "a new stride in a slot below the loop count reused a variant");
    require(!(firstResult.spirv == secondResult.spirv), "a new stride in a slot below the loop count did not change the compiled program");
}

void zeroLoopCountBindsNoSlot() {
    requireBoundSlots(UnsignedCopyLoop, 0u, CopySlots, FirstLeftover, 0u, "a loop count of 0");
}

void loopCountPastTheSlotsBindsEverySlot() {
    requireBoundSlots(UnsignedCopyLoop, CopySlots + 1u, CopySlots, FirstLeftover, CopySlots, "a loop count past the table slots");
    requireBoundSlots(UnsignedCopyLoop, 0xffffffffu, CopySlots, FirstLeftover, CopySlots, "the largest unsigned loop count");
    const CopyRequest copy(UnsignedCopyLoop, copyTable(CopySlots + 1u, 2u, FirstLeftover));
    const auto materialized = materialize(copy.request);
    require(static_cast<std::size_t>(std::count(materialized.snapshot.buffers.begin(), materialized.snapshot.buffers.end(), DescriptorValue{{FirstLeftover[0], FirstLeftover[1], FirstLeftover[2], FirstLeftover[3]}, 4u})) == 2u * (CopySlots - 2u), "a loop count past the table slots dropped the leftover descriptors it may read");
}

void signedLoopCountBoundsTheSlotsAsSigned() {
    requireBoundSlots(SignedCopyLoop, 3u, 3u, FirstLeftover, 3u, "a signed loop count of 3");
    requireBoundSlots(SignedCopyLoop, 0x80000000u, CopySlots, FirstLeftover, 0u, "a negative signed loop count");
    requireBoundSlots(UnsignedCopyLoop, 0x80000000u, CopySlots, FirstLeftover, CopySlots, "the unsigned loop count 0x80000000");
}

void countComparedFromTheLeftBoundsTheSlots() {
    requireBoundSlots(SwappedCopyLoop, 2u, 2u, FirstLeftover, 2u, "a loop count compared as count > slot");
    requireBoundSlots(SwappedCopyLoop, 0u, CopySlots, FirstLeftover, 0u, "a loop count of 0 compared as count > slot");
}

bool bindsBuffer(const ResourceSnapshot& snapshot, const BufferDescriptor& descriptor) {
    const auto bound = boundBuffers(snapshot);
    return std::find(bound.begin(), bound.end(), descriptor) != bound.end();
}

void requireSlotsBound(std::span<const std::uint32_t> code, std::uint32_t count, std::uint32_t filledSlots, std::uint32_t boundSources, std::uint32_t boundDestinations, const std::string& message) {
    const CopyRequest copy(code, copyTable(count, filledSlots, FirstLeftover));
    const auto materialized = materialize(copy.request);
    for (std::uint32_t slot = 0; slot < filledSlots; ++slot) {
        require(bindsBuffer(materialized.snapshot, copySource(slot)) == (slot < boundSources), message + ": the source of table slot " + std::to_string(slot) + (slot < boundSources ? " was dropped although the program may read it" : " was bound although the program never reads it"));
        require(bindsBuffer(materialized.snapshot, copyDestination(slot)) == (slot < boundDestinations), message + ": the destination of table slot " + std::to_string(slot) + (slot < boundDestinations ? " was dropped although the program may write it" : " was bound although the program never writes it"));
    }
}

void loopTestedAfterItsBodyKeepsTheSlotsItReads() {
    requireSlotsBound(BottomTestedCopyLoop, 0u, 1u, 1u, 1u, "a loop count of 0 in a loop that tests it after the body");
    requireSlotsBound(BottomTestedCopyLoop, 2u, 2u, 2u, 2u, "a loop count of 2 in a loop that tests it after the body");
}

void loadBeforeTheLoopTestKeepsTheSlotAtTheCount() {
    requireSlotsBound(LoadBeforeTestCopyLoop, 2u, 3u, 3u, 2u, "a load before the test of a loop count of 2 and a store after it");
}

void branchOnTheCountThatMergesAgainBindsEverySlot() {
    requireSlotsBound(BranchOnCountThenCopyEightSlots, 2u, CopySlots, CopySlots, CopySlots, "a loop over eight slots that branches on a count of 2 and merges before the copy");
}

void slotReadAfterTheLoopStaysBound() {
    requireSlotsBound(CopyLoopThenReadTheSlotAtTheCount, 0u, 1u, 1u, 0u, "a loop count of 0 followed by a read of the slot at the count");
}

}

int main() {
    const std::array<std::pair<const char*, void (*)()>, 15> tests{{
        {"buffer_load_format_x compiles the selectors it does not read to the same program", &formattedLoadIgnoresSelectorsOfComponentsItDoesNotRead},
        {"buffer_load_format_xy compiles the Z and W selectors to the same program", &formattedPairLoadIgnoresTheZAndWSelectors},
        {"buffer_store_format_x compiles every selector to the same program", &formattedStoreIgnoresTheSelectors},
        {"buffer_load_format_x of a buffer without a format compiles every selector to the same program", &formattedLoadWithoutFormatIgnoresTheSelectors},
        {"tbuffer_load_format_x compiles every descriptor format and selector to the same program", &typedLoadIgnoresTheDescriptorFormatAndSelectors},
        {"leftover table slots past a loop count share the variant of the slots the loop reads", &leftoverSlotsPastTheLoopCountShareTheVariant},
        {"a new descriptor in a table slot below the loop count compiles its own variant", &reachableSlotKeepsItsOwnVariant},
        {"a loop count of 0 binds no table slot", &zeroLoopCountBindsNoSlot},
        {"a loop count past the table slots binds every slot", &loopCountPastTheSlotsBindsEverySlot},
        {"a signed loop compare bounds the table slots with signed semantics", &signedLoopCountBoundsTheSlotsAsSigned},
        {"a loop count compared as count > slot bounds the table slots", &countComparedFromTheLeftBoundsTheSlots},
        {"a loop that tests its count after the body keeps the slots it reads, including the first one at a count of 0", &loopTestedAfterItsBodyKeepsTheSlotsItReads},
        {"a load before the loop test keeps the slot at the count that the store after the test drops", &loadBeforeTheLoopTestKeepsTheSlotAtTheCount},
        {"a branch on the loop count that merges again before the copy binds every slot", &branchOnTheCountThatMergesAgainBindsEverySlot},
        {"a table slot the loop skips stays bound for a read after the loop", &slotReadAfterTheLoopStaysBound},
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

#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProgramFailures.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceProgram.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include <array>
#include <iostream>
#include <future>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void expectFailure(TAction action, const char* expected, const char* message) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find(expected) != std::string::npos, "unexpected failure reason");
        return;
    }
    throw std::runtime_error(message);
}

void verifyResult(const ShaderRecompiler::RecompileResult& first, const ShaderRecompiler::RecompileResult& second) {
    require(first.spirv == second.spirv, "replayed SPIR-V differs");
    require(first.pushConstants == second.pushConstants, "replayed push constants differ");
    require(first.bdaAbiVersion == second.bdaAbiVersion && first.bindings.size() == second.bindings.size(), "replayed layout differs");
    for (std::size_t index = 0; index < first.bindings.size(); ++index) {
        const auto& left = first.bindings[index];
        const auto& right = second.bindings[index];
        require(left.kind == right.kind && left.role == right.role && left.descriptorSet == right.descriptorSet && left.binding == right.binding && left.count == right.count && left.guestDescriptor == right.guestDescriptor && left.readOnly == right.readOnly, "replayed binding differs");
    }
}

void verifyRegisterSources() {
    using namespace ShaderRecompiler;
    IrResourcePlan plan;
    IrValue samplerRegister(IrOpcode::Void, IrType::ScalarReg, 0);
    IrValue bufferRegister(IrOpcode::Void, IrType::ScalarReg, 1);
    IrValue sameSamplerRegister(IrOpcode::Void, IrType::ScalarReg, 2);
    samplerRegister.SetRegister({RegisterBank::Scalar, 8});
    bufferRegister.SetRegister({RegisterBank::Scalar, 12});
    sameSamplerRegister.SetRegister({RegisterBank::Scalar, 8});
    IrValue samplerRead(IrOpcode::GetUserData, IrType::U32, 3);
    IrValue bufferRead(IrOpcode::GetUserData, IrType::U32, 4);
    IrValue sameSamplerRead(IrOpcode::GetUserData, IrType::U32, 5);
    samplerRead.AddArgument(&samplerRegister);
    bufferRead.AddArgument(&bufferRegister);
    sameSamplerRead.AddArgument(&sameSamplerRegister);
    require(!EquivalentValue(plan, &samplerRead, &bufferRead), "sampler SGPRs were merged with buffer SGPRs");
    require(EquivalentValue(plan, &samplerRead, &sameSamplerRead), "identical user data reads were not recognized");
    sameSamplerRegister.SetRegister({RegisterBank::UserData, 8});
    require(!EquivalentValue(plan, &samplerRead, &sameSamplerRead), "different register banks were merged");
    IrValue firstVector(IrOpcode::Void, IrType::VectorReg, 6);
    IrValue secondVector(IrOpcode::Void, IrType::VectorReg, 7);
    firstVector.SetRegister({RegisterBank::Vector, 0});
    secondVector.SetRegister({RegisterBank::Vector, 1});
    require(!EquivalentValue(plan, &firstVector, &secondVector), "different vector registers were merged");
    require(!EquivalentValue(plan, &samplerRegister, &firstVector), "different register types were merged");
}

void verifyProgramFailures() {
    using namespace ShaderRecompiler;
    using Failures = AgcDriver::ProgramFailures;
    const std::array<std::uint32_t, 8> code{0xf4040004u, 0xfa000000u, 0xf4000080u, 0xfa000000u, 0x7e000202u, 0xf80008cfu, 0u, 0xbf810000u};
    std::uint32_t payload = 0x3f800000u;
    std::uint64_t table = reinterpret_cast<std::uintptr_t>(&payload);
    const auto address = reinterpret_cast<std::uintptr_t>(&table);
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
    RecompileRequest request{};
    request.shader = {ShaderStage::Vertex, 0x20000u, code, 0, {}, 0x5eed0001u};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 8;
    request.context.userData = userData;
    request.context.vertex = ShaderVertexStageInfo{};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;

    Failures failures;
    require(!failures.Skips(request), "an empty failure cache skipped a program");
    AgcDriver::ShaderMemory memory({});
    memory.Capture(request);
    require(memory.Source() != nullptr, "capture did not keep the source entry");
    const auto regions = memory.Regions();
    request.context.memory = regions;
    const auto specialization = memory.Specialization();
    require(!failures.Skips(request, specialization), "an empty failure cache skipped a variant");
    require(!Recompile(request).cacheHit, "the failure test program was already compiled");

    const auto key = Failures::CompileKey(request, specialization);
    require(key.stage == Failures::Stage::Compile && Failures::Skipping(key), "a compilation failure is not skipped");
    require(failures.Record(key), "the first compilation failure was not recorded");
    require(!failures.Record(key), "a repeated compilation failure was recorded again");
    require(failures.Skips(request, specialization), "a known compilation failure was not skipped");
    require(!failures.Skips(request), "a compilation failure skipped every variant of the program");

    auto relocated = request;
    relocated.shader.codeAddress += 0x1000;
    require(Recompile(relocated).cacheHit, "relocation missed the success cache");
    require(failures.Skips(relocated, specialization), "relocation missed the failure cache");
    auto changedLayout = request;
    changedLayout.layout.pushConstantSizeBytes = 64;
    require(!Recompile(changedLayout).cacheHit, "a layout change hit the success cache");
    require(!failures.Skips(changedLayout, specialization), "a layout change hit the failure cache");
    auto changedTarget = request;
    changedTarget.target.subgroupSize = 32;
    require(!failures.Skips(changedTarget, specialization), "a target change hit the failure cache");
    auto changedCode = request;
    changedCode.shader.codeHash = 0x5eed0002u;
    require(!failures.Skips(changedCode, specialization), "different code hit the failure cache");
    auto changedSpecialization = specialization;
    changedSpecialization.buffers.emplace_back();
    require(!failures.Skips(request, changedSpecialization), "a resource change hit the failure cache");

    payload = 0x40000000u;
    AgcDriver::ShaderMemory updated({});
    updated.Capture(request);
    require(updated.Specialization() == specialization, "dynamic shader data changed the specialization");
    const auto updatedRegions = updated.Regions();
    auto updatedRequest = request;
    updatedRequest.context.memory = updatedRegions;
    require(Recompile(updatedRequest).cacheHit, "dynamic shader data missed the success cache");
    require(failures.Skips(updatedRequest, updated.Specialization()), "dynamic shader data missed the failure cache");

    auto invalidPlan = request;
    invalidPlan.context.userDataBaseRegister = 105;
    invalidPlan.context.memory = {};
    expectFailure([&] { memory.Capture(invalidPlan); }, "shader user data exceeds the scalar register bank", "an invalid user data base was prepared");
    require(memory.Source() == nullptr, "a failed preparation kept the previous source entry");
    const auto planKey = Failures::PlanKey(invalidPlan);
    require(planKey.stage == Failures::Stage::Plan && Failures::Skipping(planKey), "a preparation failure is not skipped");
    require(!failures.Skips(invalidPlan), "an unrecorded preparation failure was skipped");
    require(failures.Record(planKey), "the preparation failure was not recorded");
    require(failures.Skips(invalidPlan), "a known preparation failure was not skipped");
    require(!failures.Skips(request), "a preparation failure skipped another context");

    auto agreeing = request;
    agreeing.shader.codeHash = 0x5eed0003u;
    agreeing.context.memory = {};
    auto& vertex = agreeing.context.vertex.emplace();
    vertex.resourcesNum = 2;
    vertex.resources[0].fields = {0x10000u, 16u << 16u, 4u, 0u};
    vertex.resources[1].fields = {0x10004u, 16u << 16u, 4u, 0u};
    auto disagreeing = agreeing;
    disagreeing.context.vertex->resources[1].fields[2] = 8u;
    std::vector<std::uint64_t> agreeingSource;
    std::vector<std::uint64_t> disagreeingSource;
    RecompileCacheKey::Build(agreeing, agreeingSource);
    RecompileCacheKey::Build(disagreeing, disagreeingSource);
    require(agreeingSource == disagreeingSource, "vertex buffer record counts changed the recompiler's source key");
    AgcDriver::ShaderMemory vertexMemory({});
    expectFailure([&] { vertexMemory.Capture(disagreeing); }, "merged vertex buffers disagree on record count", "vertex buffers that disagree on their record count were prepared");
    require(vertexMemory.Source() == nullptr, "rejected vertex buffers kept a source entry");
    const auto inputKey = Failures::PlanKey(disagreeing);
    require(!Failures::Skipping(inputKey) && !Failures::Skipping(Failures::CompileKey(disagreeing, specialization)), "rejected vertex buffers were skipped by the source key");
    require(Failures::Skipping(Failures::PlanKey(agreeing)) && Failures::Skipping(Failures::CompileKey(agreeing, specialization)), "a failure with accepted vertex buffers was not keyed by the source");
    require(failures.Record(inputKey) && !failures.Skips(agreeing), "rejected vertex buffers skipped work whose vertex buffers agree");
    vertexMemory.Capture(agreeing);
    require(vertexMemory.Source() != nullptr, "accepted vertex buffers were not prepared");

    table = 0;
    auto nullTable = request;
    nullTable.context.memory = {};
    AgcDriver::ShaderMemory invalidMemory({});
    expectFailure([&] { invalidMemory.Capture(nullTable); }, "null or misaligned address", "a null nested pointer was captured");
    require(invalidMemory.Source() != nullptr, "a resource capture failure lost the prepared source entry");
    const auto captureKey = Failures::ProgramKey(Failures::Stage::Capture, nullTable.shader.codeHash);
    require(!Failures::Skipping(captureKey), "a resource capture failure is skipped");
    require(failures.Record(captureKey) && !failures.Record(captureKey), "a resource capture failure was not recorded once");
    require(!failures.Skips(nullTable), "a resource capture failure skipped the program");
    require(failures.Contains(captureKey) && failures.Contains(key) && failures.Contains(planKey), "a recorded failure was forgotten");
}

}

int main() {
    try {
        using namespace ShaderRecompiler;
        verifyRegisterSources();
#if ANYPS5_ENABLE_SPIRV_TOOLS
        const std::vector<std::uint32_t> minimalSpirv{
            0x07230203u, 0x00010000u, 0u, 5u, 0u,
            0x00020011u, 1u,
            0x0003000eu, 0u, 1u,
            0x0005000fu, 5u, 3u, 0x6e69616du, 0u,
            0x00060010u, 3u, 17u, 1u, 1u, 1u,
            0x00020013u, 1u,
            0x00030021u, 2u, 1u,
            0x00050036u, 1u, 3u, 0u, 2u,
            0x000200f8u, 4u,
            0x00010000u,
            0x000100fdu,
            0x00010038u
        };
        const auto optimizedSpirv = ValidateAndOptimizeSpirv(minimalSpirv, 0x00401001u, 0x00010000u);
        require(optimizedSpirv.size() < minimalSpirv.size(), "SPIR-V optimization did not remove the no-op");
        require(optimizedSpirv == ValidateAndOptimizeSpirv(minimalSpirv, 0x00401001u, 0x00010000u), "SPIR-V optimization is not deterministic");
#endif
        const std::array<std::uint32_t, 8> code{0xf4040004u, 0xfa000000u, 0xf4000080u, 0xfa000000u, 0x7e000202u, 0xf80008cfu, 0u, 0xbf810000u};
        std::uint32_t payload = 0x3f800000u;
        std::uint64_t table = reinterpret_cast<std::uintptr_t>(&payload);
        const auto address = reinterpret_cast<std::uintptr_t>(&table);
        const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
        RecompileRequest request{};
        request.shader = {ShaderStage::Vertex, 0x10000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 8;
        request.context.userData = userData;
        request.context.vertex = ShaderVertexStageInfo{};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 64;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;

        expectFailure([&] { static_cast<void>(Recompile(request)); }, "SrtWalker::EvaluateRuntimeSources", "missing snapshot unexpectedly read live memory");
        AgcDriver::ShaderMemory memory({});
        memory.Capture(request);
        auto regions = memory.Regions();
        require(regions.size() == 3, "nested pointer reads were not captured");
        request.context.memory = regions;
        const auto first = Recompile(request);
        require(!first.spirv.empty(), "empty compiled shader");
        require(!first.cacheHit, "first shader compilation unexpectedly hit the cache");
        const auto plan = GetResourcePlan(request);
        require(plan == GetResourcePlan(request), "resource plan was rebuilt");
        const auto cached = Recompile(request);
        require(cached.cacheHit, "unchanged shader did not hit the cache");
        verifyResult(first, cached);
        auto relocated = request;
        relocated.shader.codeAddress += 0x1000;
        require(Recompile(relocated).cacheHit, "shader relocation caused recompilation");
        auto changedTarget = request;
        changedTarget.target.subgroupSize = 32;
        require(GetResourcePlan(changedTarget) != plan, "different target reused the source entry");
        std::vector<std::uint32_t> changedCode(code.begin(), code.end());
        changedCode.insert(changedCode.begin(), 0xbf800000u);
        auto changedSource = request;
        changedSource.shader.code = changedCode;
        require(GetResourcePlan(changedSource) != plan, "changed code reused the source entry");
        auto uncached = request;
        uncached.useCache = false;
        require(GetResourcePlan(uncached) != plan, "disabled cache reused the resource plan");
        const auto fresh = Recompile(uncached);
        require(!fresh.cacheHit, "disabled cache reused the compiled variant");
        verifyResult(first, fresh);
        require(!RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(uncached)).request.useCache, "cache policy was lost in serialization");
        auto changedLayout = request;
        changedLayout.layout.pushConstantSizeBytes = 64;
        require(!Recompile(changedLayout).cacheHit, "binding layout change reused an incompatible variant");
        require(Recompile(changedLayout).cacheHit, "new binding layout variant was not cached");
        require(Recompile(request).cacheHit, "compiling a new variant evicted the original");
        auto missingMemory = request;
        missingMemory.context.memory = {};
        expectFailure([&] { static_cast<void>(Recompile(missingMemory)); }, "SrtWalker::EvaluateRuntimeSources", "cache hit bypassed resource validation");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        auto invalidSpirv = first.spirv.Words();
        invalidSpirv[0] = 0;
        expectFailure([&] { static_cast<void>(ValidateAndOptimizeSpirv(invalidSpirv, request.target.vulkanVersion, request.target.spirvVersion)); }, "SPIR-V validation before optimization failed", "invalid SPIR-V passed validation");
        expectFailure([&] { static_cast<void>(ValidateAndOptimizeSpirv(first.spirv, 0x00400000u, 0x00010600u)); }, "unsupported Vulkan/SPIR-V target", "incompatible target accepted");
        expectFailure([&] { static_cast<void>(ValidateAndOptimizeSpirv(first.spirv, 0x00405000u, 0x00010600u)); }, "unsupported Vulkan target", "unknown Vulkan target accepted");
#endif
        payload = 0x40000000u;
        AgcDriver::ShaderMemory updatedMemory({});
        updatedMemory.Capture(request);
        const auto updatedRegions = updatedMemory.Regions();
        auto updated = request;
        updated.context.memory = updatedRegions;
        const auto updatedCached = Recompile(updated);
        require(updatedCached.cacheHit, "dynamic shader data caused recompilation");
        updated.useCache = false;
        verifyResult(updatedCached, Recompile(updated));
        bool changedData = updatedCached.pushConstants != first.pushConstants;
        for (std::size_t i = 0; i < first.bindings.size(); ++i) changedData = changedData || updatedCached.bindings.at(i).guestDescriptor != first.bindings[i].guestDescriptor;
        require(changedData, "cache hit retained stale shader data");
        auto concurrent = request;
        concurrent.layout.pushConstantSizeBytes = 60;
        std::array<std::future<RecompileResult>, 4> concurrentResults;
        for (auto& future : concurrentResults) future = std::async(std::launch::async, [concurrent] { return Recompile(concurrent); });
        std::uint32_t compilations = 0;
        for (auto& future : concurrentResults) {
            const auto result = future.get();
            if (!result.cacheHit) ++compilations;
        }
        require(compilations == 1, "concurrent requests compiled the same variant repeatedly");
        const auto serialized = RequestSerializer{}.Serialize(request);
        table = 0;
        payload = 0xdeadbeefu;
        verifyResult(first, Recompile(request));
        auto replay = RequestSerializer{}.Deserialize(serialized);
        verifyResult(first, Recompile(replay.request));
        RequestMemoryView view(replay.request.context.memory);
        const auto runtime = view.MakeRuntime(userData, request.shader.codeAddress);
        std::uint32_t captured = 0;
        require(runtime.readMemory(runtime.userContext, reinterpret_cast<std::uintptr_t>(&payload), &captured) && captured == 0x3f800000u, "snapshot changed with live memory");

        request.context.userDataBaseRegister = 0x8c;
        expectFailure([&] { static_cast<void>(PrepareResourceProgram(request)); }, "shader user data exceeds the scalar register bank", "PM4 register address accepted as SGPR base");
        request.context.userDataBaseRegister = 105;
        expectFailure([&] { static_cast<void>(PrepareResourceProgram(request)); }, "shader user data exceeds the scalar register bank", "user data overran scalar register bank");
        request.context.userDataBaseRegister = 8;
        request.context.memory = {};
        AgcDriver::ShaderMemory invalid({});
        expectFailure([&] { invalid.Capture(request); }, "null or misaligned address", "null nested pointer was accepted");
        verifyProgramFailures();
        std::cout << "Shader memory capture, strict validation, deterministic replay and program failure keys passed\n";
        return 0;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        std::cerr << message.substr(0, message.find("RecompileRequest:")) << '\n';
        return 1;
    }
}

#ifndef CORE_SHADER_RECOMPILER_SPIRVBACKEND_SPIRVBDA_HPP
#define CORE_SHADER_RECOMPILER_SPIRVBACKEND_SPIRVBDA_HPP

#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include "SpirvBackend/SpirvEmitter.hpp"
#include "BdaAbi.hpp"
#include <span>
#include <vector>

namespace ShaderRecompiler {

inline constexpr std::uint32_t MaxBdaCacheSlots = 8;
inline constexpr std::uint32_t MaxBdaGroupReads = 16;

struct BdaLaneAddress {
    std::uint32_t base = 0;
    std::uint32_t offset = 0;
    std::uint32_t active = 0;
};

std::uint32_t BdaConstant(SpirvEmitterState& state, std::uint64_t value);
std::uint32_t BdaWord(SpirvEmitterState& state, std::uint32_t variable, std::uint32_t index);
std::uint32_t BdaLoadWord(SpirvEmitterState& state, std::uint32_t index);
std::uint32_t BdaLoadAddress(SpirvEmitterState& state, std::uint32_t index);
void RecordBdaFault(SpirvEmitterState& state, std::uint32_t address, std::uint32_t bytes, std::uint32_t instruction, BdaAbi::FaultReason reason);
void ReturnBdaFailureIf(SpirvEmitterState& state, std::uint32_t condition, std::uint32_t address, std::uint32_t bytes, std::uint32_t instruction, BdaAbi::FaultReason reason);
void ValidateBdaTarget(const IrProgram& program, const SpirvTargetOptions& target);
void StopBdaInvocationIf(SpirvEmitterState& state, std::uint32_t condition);
std::uint32_t EmitBdaRead(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t bits);
std::uint32_t AddBdaAddress(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t address, std::uint32_t offset, bool subtract);
void EmitBdaMissCount(SpirvEmitterState& state);
void DefineBdaFaultFunction(SpirvEmitterState& state);
void DefineBdaFunctions(SpirvEmitterState& state);
void PlanBdaReads(SpirvEmitterState& state);
std::vector<std::vector<std::uint32_t>> EmitBdaGroupRead(SpirvEmitterState& state, const BdaReadGroup& group, std::span<const BdaLaneAddress> lanes);

}

#endif

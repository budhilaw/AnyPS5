#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PM4_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PM4_HPP

#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4Opcodes.hpp"
#include <array>
#include <span>
#include <functional>
#include <string>

namespace AgcDriver::Pm4 {

struct DrawParameters {
    std::uint64_t indexAddress;
    std::uint32_t indexCount;
    std::uint32_t indexSize;
    std::uint32_t instanceCount;
    std::uint32_t flags;
    bool indexed = true;
    std::uint32_t firstVertex = 0;
    std::uint32_t firstInstance = 0;
};

std::string Name(std::uint32_t header);
std::string_view UnsupportedReason(std::uint32_t header);
void Wait(std::span<const std::uint32_t> packet);
bool TryWait(std::span<const std::uint32_t> packet);
bool TryWait(std::span<const std::uint32_t> packet, const std::function<bool(std::uint64_t, std::uint32_t, std::uint64_t&)>& lookup);
bool DmaGdsDestination(std::span<const std::uint32_t> packet);
bool DmaGdsSource(std::span<const std::uint32_t> packet);
bool DmaImmediateSource(std::span<const std::uint32_t> packet);
bool EventWritesMemory(std::span<const std::uint32_t> packet);
void TransferRanges(std::span<const std::uint32_t> packet, std::uint64_t& destination, std::size_t& destinationBytes, std::uint64_t& source, std::size_t& sourceBytes);
bool DeferrableWrite(std::span<const std::uint32_t> packet, std::uint64_t& address, std::uint32_t& bytes, std::uint64_t& value, bool& known);
void Validate(std::span<const std::uint32_t> packet, std::uint32_t queue);
void Execute(std::span<const std::uint32_t> packet, QueueState& queue);
bool AccessesMemory(std::uint32_t header);
bool UsesGpuCacheBarrier(std::span<const std::uint32_t> packet);
std::uint64_t DispatchIndirectAddress(std::span<const std::uint32_t> packet, const QueueState& queue);
std::array<std::uint32_t, 5> ResolveDispatch(std::span<const std::uint32_t> packet, const QueueState& queue);
DrawParameters ResolveDraw(std::span<const std::uint32_t> packet, const QueueState& queue);

}

#endif

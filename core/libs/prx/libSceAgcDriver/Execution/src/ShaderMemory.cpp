#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace AgcDriver {

ShaderMemory::ShaderMemory(std::span<const ShaderRecompiler::MemoryRegion> regions, std::vector<std::shared_ptr<const void>> keep) : owners(std::move(keep)) {
    const ShaderRecompiler::RequestMemoryView validated(regions);
    for (const auto& region : regions) {
        const auto same = std::find_if(initial.begin(), initial.end(), [&](const auto& other) { return other.guestAddress == region.guestAddress; });
        if (same == initial.end()) initial.push_back(region);
    }
    std::sort(initial.begin(), initial.end(), [](const auto& left, const auto& right) { return left.guestAddress < right.guestAddress; });
}

bool ShaderMemory::read(void* context, std::uint64_t address, std::uint32_t* value) {
    auto& self = *static_cast<ShaderMemory*>(context);
    if (address % sizeof(*value) != 0 || address > std::numeric_limits<std::uint64_t>::max() - sizeof(*value)) {
        throw std::runtime_error("AGC driver: invalid shader memory read address");
    }
    const auto next = std::upper_bound(self.initial.begin(), self.initial.end(), address, [](std::uint64_t value, const auto& region) { return value < region.guestAddress; });
    if (next != self.initial.begin()) {
        const auto& previous = *std::prev(next);
        const auto offset = address - previous.guestAddress;
        if (offset < previous.bytes.size()) {
            if (previous.bytes.size() - offset < sizeof(*value)) {
                throw std::runtime_error("AGC driver: shader memory read crosses a snapshot boundary");
            }
            std::memcpy(value, previous.bytes.data() + offset, sizeof(*value));
            return true;
        }
    }
    if (next != self.initial.end() && next->guestAddress - address < sizeof(*value)) {
        throw std::runtime_error("AGC driver: shader memory read overlaps a snapshot boundary");
    }
    if (const auto found = self.dwords.find(address); found != self.dwords.end()) {
        *value = found->second;
        return true;
    }
    // A page checked earlier in this capture needs no range check: tracking that protects it
    // since faults and resolves. Queued GPU writes are still resolved per read.
    constexpr std::uint64_t pageBytes = 4096;
    if (address / pageBytes == self.checkedPage) {
        GuestMemory::MemoryAccessScope::Resolve(address, sizeof(*value), false);
        std::memcpy(value, reinterpret_cast<const void*>(address), sizeof(*value));
    } else {
        GuestMemory::Read(address, std::as_writable_bytes(std::span(value, 1)), alignof(std::uint32_t));
        self.checkedPage = address / pageBytes;
    }
    self.dwords.emplace(address, *value);
    return true;
}

void ShaderMemory::Capture(const ShaderRecompiler::RecompileRequest& request) {
    const auto plan = ShaderRecompiler::GetResourcePlan(request);
    constexpr ShaderRecompiler::ResourceMaterializer materializer;
    ShaderRecompiler::SrtRuntime runtime;
    runtime.userData = request.context.userData;
    runtime.shaderBase = request.shader.codeAddress;
    runtime.userContext = this;
    runtime.readMemory = &read;
    runtime.readSpecializationMemory = &read;
    snapshot = {};
    specialization = {};
    checkedPage = ~0ull;
    materializer.Materialize(*plan, runtime, snapshot, specialization);
}

std::vector<ShaderRecompiler::MemoryRegion> ShaderMemory::Regions() const {
    std::vector<ShaderRecompiler::MemoryRegion> result;
    result.reserve(initial.size() + dwords.size());
    auto region = initial.begin();
    for (const auto& [address, value] : dwords) {
        for (; region != initial.end() && region->guestAddress < address; ++region) result.push_back(*region);
        result.push_back({address, std::as_bytes(std::span(&value, 1))});
    }
    result.insert(result.end(), region, initial.end());
    return result;
}

}

#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>

namespace AgcDriver::Graphics {

void GuestBufferMemory::AcquireRegistered(std::span<const GuestMemorySnapshot> snapshots) {
    Require(!uploaded && regions.empty() && lease.empty(), "guest allocation lease must precede resource registration");
    lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    std::vector<std::pair<std::uint64_t, std::uint64_t>> required;
    required.reserve(snapshots.size());
    for (const auto& snapshot : snapshots) required.emplace_back(snapshot.address, snapshot.address + snapshot.bytes.size());
    regions.reserve(lease.size());
    for (const auto& range : lease) {
        validate(range->address, range->bytes);
        if (!range->writable && !range->releasable && context.guestBufferCache != nullptr) {
            regions.push_back({range->address, range->address + range->bytes, false, false, {}, nullptr});
            regions.back().image = range;
            continue;
        }
        if (range->writable && context.guestBufferCache != nullptr) {
            for (const auto& [first, last] : context.guestBufferCache->AddressWindows(range->address, range->address + range->bytes, required)) regions.push_back({first, last, true, false, {}, nullptr});
            continue;
        }
        std::vector<std::byte> snapshot;
        if (!range->writable) {
            snapshot.resize(range->bytes);
            GuestMemory::Read(range->address, snapshot);
        }
        regions.push_back({range->address, range->address + range->bytes, range->writable, false, std::move(snapshot), nullptr});
    }
}

bool GuestBufferMemory::LearnAddress(std::uint64_t address) const {
    if (context.guestBufferCache == nullptr) return false;
    const bool registered = std::any_of(lease.begin(), lease.end(), [&](const auto& range) { return range->writable && range->address <= address && address < range->address + range->bytes; });
    if (!registered) return false;
    context.guestBufferCache->LearnAddress(address);
    return true;
}

}

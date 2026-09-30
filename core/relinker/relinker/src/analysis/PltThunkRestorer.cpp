#include <relinker/analysis/PltThunkRestorer.hpp>
#include <cstring>
#include <map>
#include <optional>

namespace Relinker {

namespace {

constexpr std::uint32_t JumpSlot = 7;
constexpr std::uint64_t ThunkSize = 16;

struct Slot {
    VirtualAddress Address;
    const std::string* Nid;
};

template<typename T>
T Read(const std::vector<std::uint8_t>& bytes, std::uint64_t offset) {
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

template<typename T>
void Write(std::vector<std::uint8_t>& bytes, std::size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::vector<std::uint8_t> Thunk(VirtualAddress thunk, VirtualAddress slot, std::uint32_t index, VirtualAddress plt0) {
    std::vector<std::uint8_t> bytes = {0xFF, 0x25, 0, 0, 0, 0, 0x68, 0, 0, 0, 0, 0xE9, 0, 0, 0, 0};
    Write(bytes, 2, static_cast<std::int32_t>(static_cast<std::int64_t>(slot - (thunk + 6))));
    Write(bytes, 7, index);
    Write(bytes, 12, static_cast<std::int32_t>(static_cast<std::int64_t>(plt0 - (thunk + ThunkSize))));
    return bytes;
}

}

RestoredPltThunks RestorePatchedPltThunks(const std::vector<NidReference>& references, const std::vector<std::uint8_t>& text, VirtualAddress textVaddr, FileByteOffset textOffset, FileByteOffset tableOffset) {
    RestoredPltThunks result;
    std::map<std::uint32_t, Slot> slots;
    std::map<VirtualAddress, std::uint32_t> indices;
    for (const auto& reference : references) {
        if (reference.RelocationTypeValue != JumpSlot || reference.RelocationTableOffset < tableOffset || (reference.RelocationTableOffset - tableOffset) % 24 != 0) continue;
        const auto index = static_cast<std::uint32_t>((reference.RelocationTableOffset - tableOffset) / 24);
        slots.emplace(index, Slot{reference.RelocationAddress, &reference.Nid});
        indices.emplace(reference.RelocationAddress, index);
    }
    if (slots.empty() || text.size() < ThunkSize) return result;
    std::optional<VirtualAddress> plt0;
    std::map<std::uint32_t, bool> canonical;
    for (std::size_t offset = 0; offset + ThunkSize <= text.size(); ++offset) {
        if (text[offset] != 0xFF || text[offset + 1] != 0x25 || text[offset + 6] != 0x68 || text[offset + 11] != 0xE9) continue;
        const auto thunk = textVaddr + offset;
        const auto slot = thunk + 6 + static_cast<std::uint64_t>(static_cast<std::int64_t>(Read<std::int32_t>(text, offset + 2)));
        const auto found = indices.find(slot);
        if (found == indices.end() || Read<std::uint32_t>(text, offset + 7) != found->second) continue;
        const auto target = thunk + ThunkSize + static_cast<std::uint64_t>(static_cast<std::int64_t>(Read<std::int32_t>(text, offset + 12)));
        if (thunk != target + ThunkSize * (static_cast<std::uint64_t>(found->second) + 1)) return result;
        if (plt0.has_value() && *plt0 != target) return result;
        plt0 = target;
        canonical[found->second] = true;
        offset += ThunkSize - 1;
    }
    if (!plt0.has_value()) return result;
    for (const auto& [index, slot] : slots) {
        if (canonical.contains(index)) continue;
        const auto thunk = *plt0 + ThunkSize * (static_cast<std::uint64_t>(index) + 1);
        if (thunk < textVaddr || thunk + ThunkSize > textVaddr + text.size()) continue;
        result.Patches.push_back({textOffset + (thunk - textVaddr), Thunk(thunk, slot.Address, index, *plt0)});
        result.Nids.push_back(*slot.Nid);
    }
    return result;
}

}

#include <relinker/analysis/PltThunkRestorer.hpp>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {

constexpr Relinker::VirtualAddress TextVaddr = 0x1000;
constexpr Relinker::FileByteOffset TextOffset = 0x4000;
constexpr Relinker::FileByteOffset TableOffset = 0x9000;
constexpr Relinker::VirtualAddress Plt0 = 0x1100;
constexpr Relinker::VirtualAddress GotBase = 0x3000;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TValue>
void write(std::vector<std::uint8_t>& bytes, std::size_t offset, TValue value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::vector<std::uint8_t> thunk(std::uint32_t index) {
    const auto address = Plt0 + 16 * (index + 1);
    std::vector<std::uint8_t> bytes = {0xFF, 0x25, 0, 0, 0, 0, 0x68, 0, 0, 0, 0, 0xE9, 0, 0, 0, 0};
    write(bytes, 2, static_cast<std::int32_t>(GotBase + 8 * index - (address + 6)));
    write(bytes, 7, index);
    write(bytes, 12, static_cast<std::int32_t>(Plt0 - (address + 16)));
    return bytes;
}

std::vector<Relinker::NidReference> references(std::uint32_t count) {
    std::vector<Relinker::NidReference> result;
    for (std::uint32_t index = 0; index < count; ++index)
        result.push_back({"nid" + std::to_string(index), {}, 7, TableOffset + 24 * index, GotBase + 8 * index, 0});
    result.push_back({"data", {}, 6, 0x8000, 0x3800, 0});
    return result;
}

std::vector<std::uint8_t> text(std::uint32_t count) {
    std::vector<std::uint8_t> bytes(0x400, 0xCC);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto stub = thunk(index);
        std::copy(stub.begin(), stub.end(), bytes.begin() + static_cast<std::ptrdiff_t>(Plt0 - TextVaddr + 16 * (index + 1)));
    }
    return bytes;
}

void restoresOverwrittenThunks() {
    auto bytes = text(4);
    const std::vector<std::uint8_t> patched = {0x31, 0xC0, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3};
    std::copy(patched.begin(), patched.end(), bytes.begin() + static_cast<std::ptrdiff_t>(Plt0 - TextVaddr + 16 * 3));
    const auto restored = Relinker::RestorePatchedPltThunks(references(4), bytes, TextVaddr, TextOffset, TableOffset);
    require(restored.Patches.size() == 1 && restored.Nids.size() == 1 && restored.Nids[0] == "nid2", "exactly the overwritten thunk is restored");
    require(restored.Patches[0].Offset == TextOffset + (Plt0 - TextVaddr + 16 * 3), "the restored thunk is written at its PLT position");
    require(restored.Patches[0].Bytes == thunk(2), "the restored thunk jumps through its slot, pushes its index and falls back to PLT0");
}

void leavesIntactPltAlone() {
    const auto restored = Relinker::RestorePatchedPltThunks(references(4), text(4), TextVaddr, TextOffset, TableOffset);
    require(restored.Patches.empty() && restored.Nids.empty(), "an intact PLT needs no restoration");
}

void ignoresInconsistentLayouts() {
    auto bytes = text(4);
    auto moved = thunk(1);
    write(moved, 12, static_cast<std::int32_t>(Plt0 + 0x40 - (Plt0 + 32 + 16)));
    std::copy(moved.begin(), moved.end(), bytes.begin() + static_cast<std::ptrdiff_t>(Plt0 - TextVaddr + 32));
    std::fill_n(bytes.begin() + static_cast<std::ptrdiff_t>(Plt0 - TextVaddr + 48), 16, 0xC3);
    const auto restored = Relinker::RestorePatchedPltThunks(references(4), bytes, TextVaddr, TextOffset, TableOffset);
    require(restored.Patches.empty(), "thunks that disagree about PLT0 disable restoration");
}

}

int main() {
    try {
        restoresOverwrittenThunks();
        leavesIntactPltAlone();
        ignoresInconsistentLayouts();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "PLT thunk restorer tests passed\n";
    return 0;
}

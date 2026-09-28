#include <cstdlib>
#include <string>
#include <mutex>
#include <array>
#include <chrono>
#include <functional>
#include <thread>
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace AgcDriver::Pm4 {
namespace {

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint64_t address(std::uint32_t low, std::uint32_t high) {
    return low | (static_cast<std::uint64_t>(high) << 32u);
}

std::uint32_t registerOffset(std::uint32_t value) {
    require(value != 0xffffffffu, "indirect register sentinel semantics are not implemented");
    // Bits 28-31 carry the index/bank selection of the SET_*_REG_INDEX forms (no host meaning).
    const auto offset = value & ~0xf0000000u;
    if (offset > 0xffffu) {
        char message[96];
        std::snprintf(message, sizeof(message), "extended register semantics are not implemented (register word 0x%08x)", value);
        throw std::runtime_error(message);
    }
    if ((value & 0x80000000u) != 0) {
        static std::once_flag once;
        std::call_once(once, [&] { std::fprintf(stderr, "AGC driver: register word 0x%08x uses bit 31 (ignored)\n", value); });
    }
    return offset;
}

Registers& registersFor(QueueState& queue, std::uint32_t opcode) {
    if (opcode == 0x69 || opcode == 0x9f) return queue.context;
    if (opcode == 0x76 || opcode == 0x63) return queue.shader;
    return queue.userConfig;
}

void writeRegister(QueueState& queue, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    if (offset == 0x80 && value != 0) {
        static std::once_flag once;
        std::call_once(once, [&] { std::fprintf(stderr, "AGC driver: PA_SC_WINDOW_OFFSET written with 0x%08x by packet opcode 0x%02x\n", value, opcode); });
    }
    registersFor(queue, opcode).insert_or_assign(offset, value);
    if ((opcode == 0x64 || opcode == 0x79 || opcode == 0x7a) && offset == 0x243) queue.indexType = value & 3u;
}

bool memorySelector(std::uint32_t selector) {
    return selector == 0 || selector == 3;
}

std::uint32_t dmaSource(std::span<const std::uint32_t> packet) {
    return ((packet[1] >> 29u) & 3u) | ((packet[6] >> 24u) & 4u) | ((packet[6] >> 25u) & 8u);
}

std::uint32_t dmaDestination(std::span<const std::uint32_t> packet) {
    return ((packet[1] >> 20u) & 3u) | ((packet[6] >> 25u) & 4u) | ((packet[6] >> 26u) & 8u);
}

void copyMemory(std::uint64_t source, std::uint64_t destination, std::size_t bytes, bool immediate) {
    if (bytes == 0) return;
    GuestMemory::CheckRange(reinterpret_cast<void*>(destination), bytes, 1, true);
    std::vector<std::byte> data(bytes);
    if (immediate) {
        const auto value = static_cast<std::uint32_t>(source);
        for (std::size_t i = 0; i < bytes; ++i) data[i] = static_cast<std::byte>(value >> ((i % 4) * 8));
    } else {
        GuestMemory::Read(source, data);
    }
    GuestMemory::Write(destination, data);
}

}

std::string Name(std::uint32_t header) {
    const auto opcode = (header >> 8u) & 0xffu;
    if (opcode == 0x10 && (header & 0xfcu) != 0) {
        switch ((header >> 2u) & 0x3fu) {
            case 0x05: return "DRAW_RESET";
            case 0x06: return "WAIT_FLIP_DONE";
            case 0x09: return "DISPATCH_RESET";
            case 0x0b: return "PUSH_MARKER";
            case 0x0c: return "POP_MARKER";
            case 0x14: return "ACQUIRE_MEM_CUSTOM";
            case 0x15: return "WRITE_DATA_CUSTOM";
            case 0x17: return "FLIP";
            case 0x18: return "RELEASE_MEM_CUSTOM";
            case 0x19: return "DMA_DATA_CUSTOM";
            case 0x1a: return "CONTEXT_STATE";
            default: return "UNKNOWN_CUSTOM";
        }
    }
    for (const auto& entry : Opcodes) if (entry.value == opcode) return std::string(entry.name);
    char text[24]{};
    std::snprintf(text, sizeof(text), "UNKNOWN_0x%02x", opcode);
    return text;
}

std::string_view UnsupportedReason(std::uint32_t header) {
    const auto opcode = (header >> 8u) & 0xffu;
    if (opcode == 0x10) {
        switch ((header >> 2u) & 0x3fu) {
            case 0: case 0x06: case 0x09: case 0x0b: case 0x0c: case 0x17: case 0x1a: return {};
            case 0x14: case 0x18: return "guest cache actions and GPU release events are not implemented";
            default: return "custom packet has no implemented contract in the reference dispatch table";
        }
    }
    switch (opcode) {
        case 0x11: case 0x12: case 0x13: case 0x15: case 0x16: case 0x26:
        case 0x2a: case 0x2d: case 0x2f: case 0x35: case 0x37: case 0x40: case 0x42: case 0x46: case 0x50:
        case 0x58: case 0x63: case 0x64: case 0x69: case 0x76: case 0x79: case 0x7a:
        case 0x81: case 0x83: case 0x9f: return {};
        case 0x24: case 0x25: case 0x27: case 0x3c: case 0x49: case 0x93: return {};
        case 0x2c: case 0x38: case 0x3a: case 0x8d:
            return "graphics draw, shader stages and guest render-target materialization are not implemented";
        case 0x20: return "GPU query predication is not implemented";
        case 0x22: return "conditional command execution and conditional flip reservation are not implemented";
        case 0x33: case 0x3f: return "nested command buffers, branching and nested flip reservation are not implemented";
        case 0x39: case 0x59:
            return "cooperative command-queue waits are not implemented";
        case 0x84: case 0x85: case 0x86: case 0x88:
            return "separate CE/DE execution and counter synchronization are not implemented";
        case 0x43: case 0x47: case 0x48:
            return "guest cache actions and end-of-pipe event writes other than RELEASE_MEM are not implemented";
        case 0x8e: return "GPU LOD statistics are not implemented; synthetic results are forbidden";
        case 0x28: case 0x41: case 0x68: case 0x78:
            return "opcode is named but has no handler in the reference dispatch table";
        default: return "opcode is not known in the reference";
    }
}

void Validate(std::span<const std::uint32_t> packet, std::uint32_t queue) {
    require(packet.size() >= 2, "truncated PM4 header or payload");
    const auto header = packet[0];
    require((header & 0xc0000000u) == 0xc0000000u, "unsupported PM4 packet type");
    require(packet.size() == ((header >> 16u) & 0x3fffu) + 2u, "invalid PM4 packet size");
    const auto opcode = (header >> 8u) & 0xffu;
    const auto size = [&](std::size_t count) { require(packet.size() == count, "invalid packet size"); };
    const auto graphics = [&] { require(queue == 0, "graphics packet in compute queue"); };
    const auto reason = UnsupportedReason(header);
    if (!reason.empty()) throw std::runtime_error(std::string(reason));
    if (opcode == 0x10) {
        require((header & 3u) == 0, "unsupported NOP header flags");
        switch ((header >> 2u) & 0x3fu) {
            case 0:
                require((packet[1] & 0xffff0000u) != 0x68750000u, "typed user-data and legacy flip markers are not implemented");
                break;
            case 0x09: size(2); break;
            case 0x06: graphics(); size(4); require(packet[3] == 0, "unsupported rendering wait mode"); break;
            case 0x0b: {
                const auto data = std::as_bytes(packet.subspan(1));
                require(std::find(data.begin(), data.end(), std::byte{}) != data.end(), "unterminated marker text");
                break;
            }
            case 0x0c: break;
            case 0x17: graphics(); size(6); break;
            case 0x1a:
                graphics();
                require(packet.size() == 3 || packet.size() == 5, "invalid context-state packet size");
                require(packet[1] <= 3, "unknown context-state operation");
                require(std::all_of(packet.begin() + 2, packet.end(), [](auto value) { return value == 0; }), "context-state trailing fields are not implemented");
                break;
        }
        return;
    }
    // SET_UCONFIG_REG with header flag 1 on register 0x342 is the AGC library's internal tag packet
    // that brackets waits and indirect draw groups; it carries no register state.
    const bool tagPacket = opcode == 0x79 && (header & 0xffu) == 1 && (packet.size() == 3 || packet.size() == 4) && packet[1] == 0x342u;
    require((header & 0xffu) == 0 || (opcode == 0x11 && (header & 0xffu) == 2) || tagPacket, "PM4 header flags are not implemented");
    if (tagPacket) return;
    switch (opcode) {
        case 0x11:
            size(4);
            require(packet[1] == 1 && (packet[2] & 7u) == 0 && packet[3] <= 0xffffu, "unsupported indirect base index, alignment or address bits");
            if ((header & 2u) == 0) graphics();
            break;
        case 0x12: graphics(); size(2); require((packet[1] & ~0xfu) == 0, "unsupported CLEAR_STATE payload bits"); break;
        case 0x13: case 0x2f: graphics(); size(2); break;
        case 0x26: graphics(); size(3); break;
        case 0x2a: graphics(); size(2); require(packet[1] <= 3, "unsupported index-type modifiers"); break;
        case 0x2d:
            graphics();
            size(3);
            require((packet[2] & ~0x20u) == 2u, "unsupported auto draw flags");
            break;
        case 0x35:
            graphics();
            size(5);
            require(packet[3] <= packet[1], "index count exceeds maximum index size");
            require((packet[4] & ~0x20u) == 0, "unsupported indexed draw flags");
            break;
        case 0x27:
            graphics();
            size(6);
            require(packet[4] <= packet[1], "index count exceeds maximum index size");
            require((packet[5] & ~0x20u) == 0, "unsupported indexed draw flags");
            break;
        case 0x24: case 0x25:
            graphics();
            size(5);
            require((packet[1] & 3u) == 0, "misaligned indirect draw arguments");
            require((packet[4] & ~0x22u) == (opcode == 0x24 ? 2u : 0u), "unsupported indirect draw flags");
            break;
        case 0x49:
            size(8);
            require(((packet[2] >> 29u) & 7u) != 5, "GDS release data is not implemented");
            require(((packet[2] >> 29u) & 7u) <= 3, "unsupported release data selector");
            break;
        case 0x15: size(5); require((packet[4] & ~0x8000u) == 0x41u, "dispatch modifiers are not implemented"); break;
        case 0x16:
            require(packet.size() == 3 || packet.size() == 4, "invalid indirect dispatch size");
            require((packet.back() & ~0x8000u) == 0x41u, "indirect dispatch modifiers are not implemented");
            break;
        case 0x42: size(2); require(packet[1] == 0, "unsupported PFP_SYNC_ME payload"); break;
        case 0x3c: case 0x93:
            size(opcode == 0x3c ? 7 : 9);
            require((packet[1] & 7u) <= 6, "invalid WAIT_REG_MEM comparison");
            require((packet[1] & 0x10u) != 0, "WAIT_REG_MEM on registers is not implemented");
            break;
        case 0x46: {
            require((packet[1] & ~0x73fu) == 0, "unsupported EVENT_WRITE flags or reserved bits");
            const auto eventType = packet[1] & 0x3fu;
            const auto eventIndex = (packet[1] >> 8u) & 7u;
            switch (eventType) {
                case 0x07: case 0x0f: case 0x10:
                    size(2);
                    require(eventIndex == 4, "invalid partial-flush event index");
                    if (eventType != 0x07) graphics();
                    break;
                case 0x16: case 0x31: case 0x2a: case 0x2c: case 0x2e:
                    graphics();
                    size(2);
                    require(eventIndex == 0 || eventIndex == 7, "invalid cache-flush event index");
                    break;
                default: throw std::runtime_error("EVENT_WRITE event type " + std::to_string(eventType) + " is not implemented");
            }
            break;
        }
        case 0x58: {
            require(packet.size() == 7 || packet.size() == 8, "invalid ACQUIRE_MEM packet size");
            const auto controlMask = packet.size() == 8 ? 0x86287fc3u : 0xfeecfffbu;
            require((packet[1] & ~controlMask) == 0, "unsupported ACQUIRE_MEM control flags");
            require(queue == 0 || (packet[1] & 0x06287fc3u) == 0, "graphics cache operation in compute queue");
            require(packet[3] == 0 && packet[5] == 0, "ACQUIRE_MEM ranges above 40 bits are not implemented");
            require(packet[6] <= 0xffffu, "invalid ACQUIRE_MEM poll interval");
            const auto base = static_cast<std::uint64_t>(packet[4]) << 8u;
            const auto bytes = static_cast<std::uint64_t>(packet[2]) << 8u;
            require(bytes <= (1ull << 40u) - base, "ACQUIRE_MEM range exceeds 40-bit address space");
            if (packet.size() == 8) {
                require((packet[7] & ~0x3ffffu) == 0, "unsupported ACQUIRE_MEM GCR flags");
                require((packet[7] & 0x2000u) == 0, "ACQUIRE_MEM cache discard is not implemented");
            }
            break;
        }
        case 0x63: case 0x64: case 0x9f:
            if (opcode != 0x63) graphics();
            size(5);
            require((packet[1] & 3u) == 0 && packet[3] == 0x80000000u && packet[4] <= 0x3fffu, "unsupported indirect-register address or control fields");
            break;
        case 0x69: case 0x76: case 0x79: case 0x7a: {
            if (opcode != 0x76) graphics();
            require(packet.size() >= 3, "register packet has no values");
            if (opcode == 0x7a) require((packet[1] & 0xf0000000u) == 0 || (packet.size() == 3 && packet[1] == 0x20000243u), "indexed register bank selection is not implemented");
            const auto offset = registerOffset(packet[1]);
            require(packet.size() - 2 <= 0x10000u - offset, "register range overflow");
            break;
        }
        case 0x81:
            graphics();
            require(packet[1] <= 0xbffcu && (packet[1] & 3u) == 0 && packet.size() - 2 <= 0x3000u - packet[1] / 4u, "constant RAM write range overflow or misalignment");
            break;
        case 0x83:
            graphics(); size(5);
            require(packet[1] <= 0xbffcu && (packet[1] & 3u) == 0 && packet[2] <= 0x3000u - packet[1] / 4u, "constant RAM dump range overflow or misalignment");
            break;
        case 0x37: {
            require(packet.size() >= 5, "WRITE_DATA has no data");
            require((packet[1] & ~0x06110f00u) == 0, "WRITE_DATA engine or reserved fields are not implemented"); // bits 25-26: cache policy, no host meaning
            const auto destination = (packet[1] >> 8u) & 0xfu;
            require(destination == 1 || destination == 2 || (queue != 0 && destination == 5), "WRITE_DATA register or GDS destination is not implemented");
            require((packet[2] & 3u) == 0, "misaligned WRITE_DATA destination");
            break;
        }
        case 0x40: {
            size(6);
            require((packet[1] & ~0x46116f0fu) == 0, "COPY_DATA engine or reserved fields are not implemented"); // bits 13-14 and 25-26: cache policies
            const auto source = ((packet[1] & 0xfu) << 1u) | ((packet[1] >> 30u) & 1u);
            const auto destination = ((packet[1] >> 8u) & 0xfu) << 1u;
            require(destination == 2 || destination == 4, "COPY_DATA register or GDS destination is not implemented");
            require(source == 2 || source == 4 || source == 5 || source == 10 || source == 11, "COPY_DATA register, GDS or reference-clock source is not implemented");
            require(source < 10 || ((packet[1] & 0x10000u) == 0 && packet[3] == 0), "64-bit immediate COPY_DATA is not implemented");
            break;
        }
        case 0x50:
            size(7);
            require((packet[1] & ~0xe6306001u) == 0, "DMA_DATA reserved fields are not implemented"); // bits 13-14 and 25-26: cache policies, no host meaning
            if (dmaDestination(packet) == 1 || dmaSource(packet) == 1) {
                // GDS: offsets address the 64 KiB share, sizes stay inside it.
                const auto bytes = packet[6] & 0x3ffffffu;
                if (dmaDestination(packet) == 1) require(packet[4] <= 0x10000u && bytes <= 0x10000u - packet[4], "DMA_DATA GDS destination exceeds the GDS size");
                if (dmaSource(packet) == 1) require(packet[2] <= 0x10000u && bytes <= 0x10000u - packet[2], "DMA_DATA GDS source exceeds the GDS size");
                require(dmaDestination(packet) == 1 ? memorySelector(dmaSource(packet)) || dmaSource(packet) == 2 : memorySelector(dmaDestination(packet)), "DMA_DATA GDS transfer with a register or GDS peer is not implemented");
                break;
            }
            if (!memorySelector(dmaDestination(packet))) {
                char text[200];
                std::snprintf(text, sizeof(text), "DMA_DATA destination selector %u (source %u) is not implemented: packet %08x %08x %08x %08x %08x %08x %08x", dmaDestination(packet), dmaSource(packet), packet[0], packet[1], packet[2], packet[3], packet[4], packet[5], packet[6]);
                throw std::runtime_error(text);
            }
            require(memorySelector(dmaSource(packet)) || dmaSource(packet) == 2, "DMA_DATA register or GDS source is not implemented");
            require(dmaSource(packet) != 2 || packet[3] == 0, "DMA_DATA immediate exceeds 32 bits");
            break;
        default: throw std::runtime_error("known packet has no validator");
    }
}

// Polls a guest label until the packet's comparison holds. The stream executes in order, so a
// label released earlier in the same queue already holds its value; other writers are the title's
// threads. A wait that never completes is a title-side deadlock and is reported instead of hung.
// One evaluation of a WAIT_REG_MEM condition against guest memory.
bool TryWait(std::span<const std::uint32_t> packet) {
    return TryWait(packet, [](std::uint64_t, std::uint32_t, std::uint64_t&) { return false; });
}

bool TryWait(std::span<const std::uint32_t> packet, const std::function<bool(std::uint64_t, std::uint32_t, std::uint64_t&)>& lookup) {
    Validate(packet, 0);
    const bool wide = ((packet[0] >> 8u) & 0xffu) == 0x93;
    const auto function = packet[1] & 7u;
    const auto target = address(packet[2], packet[3]);
    require(target != 0, "WAIT_REG_MEM address was never patched");
    const std::uint64_t reference = wide ? address(packet[4], packet[5]) : packet[4];
    const std::uint64_t mask = wide ? address(packet[6], packet[7]) : packet[5];
    std::uint64_t value = 0;
    if (!lookup(target, wide ? 8u : 4u, value)) GuestMemory::Read(target, std::as_writable_bytes(std::span(&value, 1)).first(wide ? 8 : 4), wide ? 8 : 4);
    if (!wide) value &= 0xffffffffu;
    value &= mask;
    return function == 0 || (function == 1 && value < reference) || (function == 2 && value <= reference) || (function == 3 && value == reference) || (function == 4 && value != reference) || (function == 5 && value >= reference) || (function == 6 && value > reference);
}

bool DmaGdsDestination(std::span<const std::uint32_t> packet) { return dmaDestination(packet) == 1; }
bool DmaGdsSource(std::span<const std::uint32_t> packet) { return dmaSource(packet) == 1; }
bool DmaImmediateSource(std::span<const std::uint32_t> packet) { return dmaSource(packet) == 2; }

void TransferRanges(std::span<const std::uint32_t> packet, std::uint64_t& destination, std::size_t& destinationBytes, std::uint64_t& source, std::size_t& sourceBytes) {
    destination = source = 0;
    destinationBytes = sourceBytes = 0;
    switch ((packet[0] >> 8u) & 0xffu) {
        case 0x37:
            destination = address(packet[2], packet[3]);
            destinationBytes = packet.size() > 4 ? (packet.size() - 4) * 4 : 0;
            break;
        case 0x40: {
            const auto src = ((packet[1] & 0xfu) << 1u) | ((packet[1] >> 30u) & 1u);
            const std::size_t bytes = (packet[1] & 0x10000u) != 0 ? 8 : 4;
            destination = address(packet[4], packet[5]);
            destinationBytes = bytes;
            if (src < 10) { source = address(packet[2], packet[3]); sourceBytes = bytes; }
            break;
        }
        case 0x50: {
            const auto bytes = static_cast<std::size_t>(packet[6] & 0x3ffffffu);
            if (dmaDestination(packet) != 1) { destination = address(packet[4], packet[5]); destinationBytes = bytes; }
            if (dmaSource(packet) != 2 && dmaSource(packet) != 1) { source = address(packet[2], packet[3]); sourceBytes = bytes; }
            break;
        }
        default: break;
    }
}

bool DeferrableWrite(std::span<const std::uint32_t> packet, std::uint64_t& address_, std::uint32_t& bytes, std::uint64_t& value, bool& known) {
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    known = true;
    switch (opcode) {
        case 0x37: // WRITE_DATA: immediate dwords to memory
            if (packet.size() < 5 || packet.size() > 6 || (packet[1] & 0x10000u) != 0) return false;
            address_ = address(packet[2], packet[3]);
            bytes = static_cast<std::uint32_t>(std::min<std::size_t>(packet.size() - 4, 2) * 4);
            value = packet.size() > 5 ? address(packet[4], packet[5]) : packet[4];
            return true;
        case 0x40: { // COPY_DATA with an immediate source
            const auto source = ((packet[1] & 0xfu) << 1u) | ((packet[1] >> 30u) & 1u);
            if (source < 10) return false;
            address_ = address(packet[4], packet[5]);
            bytes = (packet[1] & 0x10000u) != 0 ? 8u : 4u;
            value = bytes == 8 ? address(packet[2], packet[3]) : packet[2];
            return true;
        }
        case 0x50: { // DMA_DATA with an immediate source, label sized
            const auto size = packet[6] & 0x3ffffffu;
            if (dmaSource(packet) != 2 || dmaDestination(packet) == 1 || (size != 4 && size != 8)) return false;
            address_ = address(packet[4], packet[5]);
            bytes = size;
            value = size == 8 ? address(packet[2], packet[3]) : packet[2];
            return true;
        }
        case 0x49: { // RELEASE_MEM with data
            const auto dataSelect = (packet[2] >> 29u) & 7u;
            const auto interrupt = (packet[2] >> 24u) & 7u;
            if (dataSelect == 0 || interrupt == 4) return false;
            address_ = address(packet[3], packet[4]);
            bytes = dataSelect == 1 ? 4u : 8u;
            value = dataSelect == 1 ? packet[5] : address(packet[5], packet[6]);
            known = dataSelect != 3;
            return true;
        }
        default: return false;
    }
}

void Wait(std::span<const std::uint32_t> packet) {
    static const bool traceLabels = std::getenv("ANYPS5_TRACE_LABELS") != nullptr;
    if (traceLabels) std::fprintf(stderr, "[label] wait 0x%llx function %u reference 0x%llx\n", static_cast<unsigned long long>(address(packet[2], packet[3])), packet[1] & 7u, static_cast<unsigned long long>(((packet[0] >> 8u) & 0xffu) == 0x93 ? address(packet[4], packet[5]) : packet[4]));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!TryWait(packet)) {
        require(std::chrono::steady_clock::now() < deadline, "WAIT_REG_MEM did not complete within 30 seconds");
        std::this_thread::sleep_for(std::chrono::microseconds(20));
    }
}

bool UsesGpuCacheBarrier(std::span<const std::uint32_t> packet) {
    require(!packet.empty() && ((packet[0] >> 8u) & 0xffu) == 0x58, "cache barrier requires ACQUIRE_MEM");
    Validate(packet, 0);
    return packet.size() == 8 && (packet[7] & 0xfc00u) == 0;
}

bool AccessesMemory(std::uint32_t header) {
    switch ((header >> 8u) & 0xffu) {
        case 0x16: case 0x24: case 0x25: case 0x27: case 0x2d: case 0x35: case 0x37: case 0x40: case 0x49: case 0x50: case 0x63: case 0x64: case 0x83: case 0x9f: return true;
        default: return false;
    }
}

std::array<std::uint32_t, 5> ResolveDispatch(std::span<const std::uint32_t> packet, const QueueState& queue) {
    std::uint64_t source = 0;
    if (packet.size() == 4) source = address(packet[1], packet[2]);
    else {
        require(queue.dispatchIndirectBase != 0, "indirect dispatch base has not been set");
        require(packet[1] <= std::numeric_limits<std::uint64_t>::max() - queue.dispatchIndirectBase, "indirect dispatch address overflow");
        source = queue.dispatchIndirectBase + packet[1];
    }
    std::array<std::uint32_t, 5> result{0xc0031500u, 0, 0, 0, packet.back()};
    GuestMemory::Read(source, std::as_writable_bytes(std::span(result).subspan(1, 3)), 4);
    static const bool traceIndirect = std::getenv("ANYPS5_TRACE_INDIRECT") != nullptr;
    if (traceIndirect) std::fprintf(stderr, "[indirect] dispatch args at 0x%llx: %u %u %u\n", static_cast<unsigned long long>(source), result[1], result[2], result[3]);
    return result;
}

DrawParameters ResolveDraw(std::span<const std::uint32_t> packet, const QueueState& queue) {
    Validate(packet, 0);
    if (((packet[0] >> 8u) & 0xffu) == 0x2d) {
        const auto offset = queue.userConfig.find(0x24a);
        require(offset != queue.userConfig.end(), "missing GE_INDX_OFFSET register");
        const auto firstVertex = offset->second;
        require(packet[1] == 0 || firstVertex <= std::numeric_limits<std::uint32_t>::max() - (packet[1] - 1u), "auto draw vertex range overflow");
        return {0, packet[1], 0, queue.instanceCount, packet[2] & 0x20u, false, firstVertex, 0};
    }
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    require(queue.indexType <= 2, "unsupported index type");
    const std::uint32_t indexSize = queue.indexType == 0 ? 2 : queue.indexType == 1 ? 4 : 1;
    const auto indexRange = [&](std::uint64_t base, std::uint32_t firstIndex, std::uint32_t indexCount) {
        require(base != 0 && base % indexSize == 0, "null or misaligned index base");
        const auto offset = static_cast<std::uint64_t>(firstIndex) * indexSize;
        require(offset <= std::numeric_limits<std::uint64_t>::max() - base, "index address overflow");
        const auto address = base + offset;
        const auto bytes = static_cast<std::uint64_t>(indexCount) * indexSize;
        require(bytes <= std::numeric_limits<std::size_t>::max(), "index range size overflow");
        GuestMemory::CheckRange(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes), indexSize);
        return address;
    };
    if (opcode == 0x24 || opcode == 0x25) {
        // Indirect draws read their arguments from the draw indirect base; the vertex and instance
        // offsets the CP would patch into user SGPRs are applied by the draw path instead.
        require(queue.drawIndirectBase != 0, "indirect draw base has not been set");
        require(packet[1] <= std::numeric_limits<std::uint64_t>::max() - queue.drawIndirectBase, "indirect draw address overflow");
        const auto source = queue.drawIndirectBase + packet[1];
        if (opcode == 0x25) {
            std::array<std::uint32_t, 5> arguments{}; // indexCount, instanceCount, firstIndex, vertexOffset, firstInstance
            GuestMemory::Read(source, std::as_writable_bytes(std::span(arguments)), 4);
            const auto indirectAddress = indexRange(queue.indexBase, arguments[2], arguments[0]);
            return {indirectAddress, arguments[0], indexSize, arguments[1], packet[4], true, arguments[3], arguments[4]};
        }
        std::array<std::uint32_t, 4> arguments{}; // vertexCount, instanceCount, firstVertex, firstInstance
        GuestMemory::Read(source, std::as_writable_bytes(std::span(arguments)), 4);
        return {0, arguments[0], 0, arguments[1], packet[4] & 0x20u, false, arguments[2], arguments[3]};
    }
    if (opcode == 0x27) {
        const auto inlineAddress = indexRange(address(packet[2], packet[3]), 0, packet[4]);
        return {inlineAddress, packet[4], indexSize, queue.instanceCount, packet[5]};
    }
    require(opcode == 0x35, "expected DRAW_INDEX_OFFSET_2 packet");
    const auto offsetAddress = indexRange(queue.indexBase, packet[2], packet[3]);
    return {offsetAddress, packet[3], indexSize, queue.instanceCount, packet[4]};
}

void Execute(std::span<const std::uint32_t> packet, QueueState& queue) {
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    if (opcode == 0x79 && (packet[0] & 0xffu) == 1) return; // AGC internal tag packet
    switch (opcode) {
        case 0x10:
            switch ((packet[0] >> 2u) & 0x3fu) {
                case 0: return;
                // A NOP changes nothing on the GPU: a reset command buffer keeps the registers
                // its earlier submissions wrote (titles rely on that across frames).
                case 0x09: queue.markers.clear(); return;
                case 0x0b: queue.markers.emplace_back(reinterpret_cast<const char*>(packet.data() + 1)); return;
                case 0x0c:
                    // Marker stacks are per command buffer on the console; the shared register state pops
                    // markers pushed on another queue, so an empty stack is not an error.
                    if (!queue.markers.empty()) queue.markers.pop_back();
                    return;
                case 0x1a:
                    switch (packet[1]) {
                        // Operation 0 is a NOP hint: the GPU keeps its context registers
                        // (titles draw afterwards relying on earlier register writes).
                        case 0: break;
                        case 1: case 3:
                            require(!queue.savedContext.has_value(), "context state is already pushed");
                            queue.savedContext = queue.context;
                            if (packet[1] == 3) queue.ClearContext();
                            break;
                        case 2:
                            require(queue.savedContext.has_value(), "context state has not been pushed");
                            queue.context = std::move(*queue.savedContext);
                            queue.savedContext.reset();
                            break;
                    }
                    return;
                default: throw std::runtime_error("custom packet requires driver execution");
            }
        case 0x11:
            ((packet[0] & 2u) == 0 ? queue.drawIndirectBase : queue.dispatchIndirectBase) = address(packet[2], packet[3]);
            return;
        case 0x12:
            queue.ClearContext(); return;
        case 0x13: queue.indexBufferSize = packet[1]; return;
        case 0x26: queue.indexBase = address(packet[1], packet[2]); return;
        case 0x2a: queue.indexType = packet[1]; return;
        case 0x2f: queue.instanceCount = packet[1]; return;
        case 0x63: case 0x64: case 0x9f: {
            std::vector<std::uint32_t> pairs(static_cast<std::size_t>(packet[4]) * 2);
            GuestMemory::Read(address(packet[1], packet[2]), std::as_writable_bytes(std::span(pairs)), 4);
            // A pair whose offset word is not a register offset (titles pad their lists with
            // unrelated data) addresses no register on the hardware either: it is skipped.
            const auto malformed = [](std::uint32_t word) { return word != 0xffffffffu && (word & 0x0fff0000u) != 0; };
            for (std::size_t i = 0; i < pairs.size(); i += 2) {
                if (malformed(pairs[i])) {
                    static std::once_flag once;
                    std::call_once(once, [&] { std::fprintf(stderr, "AGC driver: indirect register list at 0x%llx skips pair %zu {0x%08x, 0x%08x} (not a register offset)\n", static_cast<unsigned long long>(address(packet[1], packet[2])), i / 2, pairs[i], pairs[i + 1]); });
                    continue;
                }
                try {
                    registerOffset(pairs[i]);
                } catch (const std::exception& error) {
                    std::string message(error.what());
                    char text[200];
                    std::snprintf(text, sizeof(text), ": indirect register packet {0x%08x 0x%08x 0x%08x 0x%08x 0x%08x} at 0x%llx, %u pairs, pair %zu; memory before/after:", packet[0], packet[1], packet[2], packet[3], packet[4], static_cast<unsigned long long>(address(packet[1], packet[2])), packet[4], i / 2);
                    message += text;
                    std::array<std::uint32_t, 48> window{};
                    const auto start = address(packet[1], packet[2]) - 16 * 4;
                    GuestMemory::Read(start, std::as_writable_bytes(std::span(window)), 4);
                    for (std::size_t w = 0; w < window.size(); ++w) { std::snprintf(text, sizeof(text), "%s%s0x%08x", w % 8 == 0 ? "\n    " : " ", w == 16 ? "[" : "", window[w]); message += text; if (w == 16) message += "]"; }
                    throw std::runtime_error(message);
                }
            }
            for (std::size_t i = 0; i < pairs.size(); i += 2) if (!malformed(pairs[i])) writeRegister(queue, opcode, registerOffset(pairs[i]), pairs[i + 1]);
            return;
        }
        case 0x69: case 0x76: case 0x79: case 0x7a: {
            const auto offset = registerOffset(packet[1]);
            for (std::size_t i = 2; i < packet.size(); ++i) writeRegister(queue, opcode, offset + static_cast<std::uint32_t>(i - 2), packet[i]);
            return;
        }
        case 0x81:
            std::copy(packet.begin() + 2, packet.end(), queue.constantRam.begin() + packet[1] / 4);
            return;
        case 0x83:
            GuestMemory::Write(address(packet[3], packet[4]), std::as_bytes(std::span(queue.constantRam).subspan(packet[1] / 4, packet[2])), 4);
            return;
        case 0x37: {
            const auto destination = address(packet[2], packet[3]);
            static const bool traceWrites = std::getenv("ANYPS5_TRACE_LABELS") != nullptr;
            if (traceWrites) std::fprintf(stderr, "[write] data 0x%llx %zu dwords {%08x %08x} control 0x%08x%s (queue 0x%x)\n", static_cast<unsigned long long>(destination), packet.size() - 4, packet[4], packet.size() > 5 ? packet[5] : 0u, packet[1], (packet[1] & 0x10000u) != 0 ? " (single address)" : "", queue.id);
            if ((packet[1] & 0x10000u) != 0) {
                for (const auto& value : packet.subspan(4)) GuestMemory::Write(destination, std::as_bytes(std::span(&value, 1)), 4);
            } else GuestMemory::Write(destination, std::as_bytes(packet.subspan(4)), 4);
            return;
        }
        case 0x40: {
            const auto source = ((packet[1] & 0xfu) << 1u) | ((packet[1] >> 30u) & 1u);
            static const bool traceCopies = std::getenv("ANYPS5_TRACE_LABELS") != nullptr;
            if (traceCopies) std::fprintf(stderr, "[write] copy 0x%llx from 0x%llx (%s) %u bytes\n", static_cast<unsigned long long>(address(packet[4], packet[5])), static_cast<unsigned long long>(address(packet[2], packet[3])), source >= 10 ? "immediate" : "memory", (packet[1] & 0x10000u) != 0 ? 8u : 4u);
            copyMemory(address(packet[2], packet[3]), address(packet[4], packet[5]), (packet[1] & 0x10000u) != 0 ? 8 : 4, source >= 10);
            return;
        }
        case 0x50: {
            require(!DmaGdsDestination(packet) && !DmaGdsSource(packet), "GDS DMA_DATA transfers are executed by the driver, not the packet executor");
            static const bool traceDma = std::getenv("ANYPS5_TRACE_LABELS") != nullptr;
            if (traceDma && (packet[6] & 0x3ffffffu) <= 64) std::fprintf(stderr, "[write] dma 0x%llx from 0x%llx (%s) %u bytes\n", static_cast<unsigned long long>(address(packet[4], packet[5])), static_cast<unsigned long long>(address(packet[2], packet[3])), dmaSource(packet) == 2 ? "immediate" : "memory", packet[6] & 0x3ffffffu);
            copyMemory(address(packet[2], packet[3]), address(packet[4], packet[5]), packet[6] & 0x3ffffffu, dmaSource(packet) == 2);
            return;
        }
        case 0x49: {
            // End-of-pipe release: the caller has already waited for the GPU. Data selectors:
            // 1 = 32-bit immediate, 2 = 64-bit immediate, 3 = 64-bit GPU timestamp.
            const auto dataSelect = (packet[2] >> 29u) & 7u;
            const auto interrupt = (packet[2] >> 24u) & 7u;
            if (dataSelect == 0 || interrupt == 4) return;
            const auto destination = address(packet[3], packet[4]);
            static const bool traceLabels = std::getenv("ANYPS5_TRACE_LABELS") != nullptr;
            if (traceLabels) std::fprintf(stderr, "[label] release 0x%llx select %u value 0x%08x%08x interrupt %u packet {%08x %08x %08x %08x %08x %08x %08x %08x} (queue 0x%x)\n", static_cast<unsigned long long>(destination), dataSelect, packet[6], packet[5], interrupt, packet[0], packet[1], packet[2], packet[3], packet[4], packet[5], packet[6], packet.size() > 7 ? packet[7] : 0u, queue.id);
            if (dataSelect == 1) {
                const std::uint32_t value = packet[5];
                GuestMemory::Write(destination, std::as_bytes(std::span(&value, 1)), 4);
            } else {
                std::uint64_t value = dataSelect == 2 ? address(packet[5], packet[6]) : static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
                GuestMemory::Write(destination, std::as_bytes(std::span(&value, 1)), 8);
            }
            return;
        }
        default: throw std::runtime_error("packet " + Name(packet[0]) + " requires driver execution");
    }
}

}

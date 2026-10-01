#ifndef CORE_SHADER_RECOMPILER_BDAABI_HPP
#define CORE_SHADER_RECOMPILER_BDAABI_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

namespace ShaderRecompiler::BdaAbi {

inline constexpr std::uint32_t Version = 1;
inline constexpr std::uint32_t Read = 1;
inline constexpr std::uint32_t Write = 2;

struct Header {
    std::uint32_t version;
    std::uint32_t count;
    std::uint32_t entryBytes;
    std::uint32_t reserved;
};

struct Range {
    std::uint64_t begin;
    std::uint64_t end;
    std::uint64_t deviceAddress;
    std::uint32_t permissions;
    std::uint32_t reserved;
};

enum class FaultState : std::uint32_t { Empty, Writing, Ready };
enum class FaultReason : std::uint32_t { Unmapped = 1, Permission, Overflow, InvalidTable, InvalidRectangle };

struct Fault {
    FaultState state;
    FaultReason reason;
    std::uint64_t address;
    std::uint32_t bytes;
    std::uint32_t stage;
    std::uint32_t instruction;
    std::uint32_t reserved;
};

static_assert(std::is_standard_layout_v<Header> && std::is_trivially_copyable_v<Header> && sizeof(Header) == 16);
static_assert(std::is_standard_layout_v<Range> && std::is_trivially_copyable_v<Range> && sizeof(Range) == 32);
static_assert(offsetof(Range, deviceAddress) == 16 && offsetof(Range, permissions) == 24);
static_assert(std::is_standard_layout_v<Fault> && std::is_trivially_copyable_v<Fault> && sizeof(Fault) == 32);
static_assert(offsetof(Fault, address) == 8 && offsetof(Fault, instruction) == 24);

[[nodiscard]] inline bool IsValidTable(std::span<const std::byte> table) {
    if (table.size() < sizeof(Header) || (table.size() - sizeof(Header)) % sizeof(Range) != 0u) {
        return false;
    }
    Header header {};
    std::memcpy(&header, table.data(), sizeof(header));
    if (header.version != Version || header.entryBytes != sizeof(Range) || header.reserved != 0u || header.count != (table.size() - sizeof(Header)) / sizeof(Range)) {
        return false;
    }
    std::uint64_t previousEnd = 0;
    for (std::size_t index = 0; index < header.count; ++index) {
        Range range {};
        std::memcpy(&range, table.data() + sizeof(Header) + index * sizeof(Range), sizeof(range));
        if (range.reserved != 0u || range.begin >= range.end || range.begin < previousEnd || range.deviceAddress == 0u || (range.permissions & ~(Read | Write)) != 0u || range.end - range.begin > std::numeric_limits<std::uint64_t>::max() - range.deviceAddress) {
            return false;
        }
        previousEnd = range.end;
    }
    return true;
}

}

#endif

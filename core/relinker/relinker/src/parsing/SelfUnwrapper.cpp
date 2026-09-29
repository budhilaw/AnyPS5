#include <relinker/parsing/SelfUnwrapper.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <cstring>

namespace Relinker {

namespace {

constexpr std::uint32_t SelfMagic = 0x1D3D154Fu;
constexpr std::uint32_t SelfMagicAlternate = 0xEEF51454u;
constexpr std::size_t SelfHeaderSize = 0x20;
constexpr std::size_t SelfEntrySize = 0x20;
constexpr std::size_t ElfHeaderSize = 0x40;

constexpr std::uint64_t PropertyOrdered = 0x1;
constexpr std::uint64_t PropertyEncrypted = 0x2;
constexpr std::uint64_t PropertySigned = 0x4;
constexpr std::uint64_t PropertyCompressed = 0x8;
constexpr std::uint64_t PropertyHasBlocks = 0x800;
constexpr unsigned PropertyProgramHeaderShift = 20;
constexpr std::uint64_t PropertyProgramHeaderMask = 0xFFF;

constexpr std::uint32_t ElfMagic = 0x464C457Fu;

}

bool IsSelf(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < SelfHeaderSize) return false;
    const auto magic = Io::ReadU32(bytes, 0);
    return magic == SelfMagic || magic == SelfMagicAlternate;
}

SelfUnwrapResult UnwrapSelf(const std::vector<std::uint8_t>& self) {
    if (!IsSelf(self)) throw Domain::RelinkerException("Input is not a SELF");
    const auto fileSize = Io::ReadU64(self, 0x10);
    const auto entryCount = Io::ReadU16(self, 0x18);
    if (fileSize > self.size()) throw Domain::RelinkerException("SELF header file size exceeds the input");
    if (entryCount == 0) throw Domain::RelinkerException("SELF has no segment entries");

    const std::size_t elfOffset = SelfHeaderSize + static_cast<std::size_t>(entryCount) * SelfEntrySize;
    if (elfOffset + ElfHeaderSize > self.size() || Io::ReadU32(self, elfOffset) != ElfMagic) throw Domain::RelinkerException("SELF does not embed an ELF header after its segment table", elfOffset);
    if (self[elfOffset + 4] != 2 || self[elfOffset + 5] != 1) throw Domain::RelinkerException("SELF embeds an ELF that is not 64-bit little-endian", elfOffset);

    const auto programHeaderOffset = Io::ReadU64(self, elfOffset + 0x20);
    const auto programHeaderEntrySize = Io::ReadU16(self, elfOffset + 0x36);
    const auto programHeaderCount = Io::ReadU16(self, elfOffset + 0x38);
    if (programHeaderEntrySize != 0x38 || programHeaderCount == 0) throw Domain::RelinkerException("SELF embeds an ELF with an unsupported program header table", elfOffset);
    const std::size_t programHeadersEnd = static_cast<std::size_t>(programHeaderOffset) + static_cast<std::size_t>(programHeaderCount) * programHeaderEntrySize;
    if (programHeaderOffset < ElfHeaderSize || elfOffset + programHeadersEnd > self.size()) throw Domain::RelinkerException("SELF embedded program header table is out of bounds", elfOffset);

    struct Segment { std::uint64_t Offset; std::uint64_t FileSize; };
    std::vector<Segment> programHeaders(programHeaderCount);
    std::size_t elfSize = programHeadersEnd;
    for (std::uint16_t index = 0; index < programHeaderCount; ++index) {
        const std::size_t header = elfOffset + static_cast<std::size_t>(programHeaderOffset) + static_cast<std::size_t>(index) * programHeaderEntrySize;
        programHeaders[index] = {Io::ReadU64(self, header + 0x08), Io::ReadU64(self, header + 0x20)};
        if (programHeaders[index].FileSize > UINT64_MAX - programHeaders[index].Offset) throw Domain::RelinkerException("SELF embedded program header overflows", header);
        elfSize = std::max(elfSize, static_cast<std::size_t>(programHeaders[index].Offset + programHeaders[index].FileSize));
    }

    SelfUnwrapResult result;
    result.Elf.assign(elfSize, 0);
    std::memcpy(result.Elf.data(), self.data() + elfOffset, programHeadersEnd);

    std::vector<bool> supplied(programHeaderCount, false);
    for (std::uint16_t index = 0; index < entryCount; ++index) {
        const std::size_t entry = SelfHeaderSize + static_cast<std::size_t>(index) * SelfEntrySize;
        const auto properties = Io::ReadU64(self, entry);
        if ((properties & PropertyHasBlocks) == 0) continue;
        if ((properties & PropertyEncrypted) != 0) throw Domain::RelinkerException("SELF segment is encrypted; only fake-signed (plaintext) SELF files can be unwrapped", entry);
        if ((properties & PropertyCompressed) != 0) throw Domain::RelinkerException("SELF segment is compressed; unsupported", entry);
        (void)PropertyOrdered;
        (void)PropertySigned;
        const auto target = static_cast<std::uint16_t>((properties >> PropertyProgramHeaderShift) & PropertyProgramHeaderMask);
        const auto offset = Io::ReadU64(self, entry + 0x08);
        const auto size = Io::ReadU64(self, entry + 0x10);
        if (target >= programHeaderCount) throw Domain::RelinkerException("SELF segment refers to a missing program header", entry);
        if (size != programHeaders[target].FileSize) throw Domain::RelinkerException("SELF segment size disagrees with its program header", entry);
        if (size > self.size() || offset > self.size() - size) throw Domain::RelinkerException("SELF segment data is out of bounds", entry);
        if (supplied[target]) throw Domain::RelinkerException("SELF supplies a program header twice", entry);
        std::memcpy(result.Elf.data() + programHeaders[target].Offset, self.data() + offset, static_cast<std::size_t>(size));
        supplied[target] = true;
        ++result.CopiedSegments;
    }
    if (result.CopiedSegments == 0) throw Domain::RelinkerException("SELF supplies no segment data");

    for (std::uint16_t index = 0; index < programHeaderCount; ++index) {
        if (supplied[index] || programHeaders[index].FileSize == 0) continue;
        const auto first = programHeaders[index].Offset;
        const auto last = first + programHeaders[index].FileSize;
        bool covered = false;
        for (std::uint16_t other = 0; other < programHeaderCount && !covered; ++other) {
            covered = supplied[other] && programHeaders[other].Offset <= first && last <= programHeaders[other].Offset + programHeaders[other].FileSize;
        }
        if (!covered) result.MissingProgramHeaders.push_back(index);
    }
    return result;
}

}

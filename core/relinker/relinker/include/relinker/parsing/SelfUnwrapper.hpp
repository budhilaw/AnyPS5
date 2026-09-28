#ifndef RELINKER_PARSING_SELFUNWRAPPER_HPP
#define RELINKER_PARSING_SELFUNWRAPPER_HPP

#include <cstdint>
#include <vector>

namespace Relinker {

// A fake-signed SELF ("fself", as produced by console-side dumpers) wraps a plaintext ELF:
// a SELF header, a segment table, the original ELF header and program headers, then the
// segment payloads with a digest table in front of each. Unwrapping rebuilds the ELF file
// layout the program headers describe. Encrypted or compressed segments are refused.

struct SelfUnwrapResult {
    std::vector<std::uint8_t> Elf;
    std::size_t CopiedSegments = 0;
    // Program headers with file contents that no SELF segment supplies (left zero-filled).
    std::vector<std::uint16_t> MissingProgramHeaders;
};

bool IsSelf(const std::vector<std::uint8_t>& bytes);
SelfUnwrapResult UnwrapSelf(const std::vector<std::uint8_t>& self);

}

#endif

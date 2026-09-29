#ifndef RELINKER_PARSING_SELFUNWRAPPER_HPP
#define RELINKER_PARSING_SELFUNWRAPPER_HPP

#include <cstdint>
#include <vector>

namespace Relinker {


struct SelfUnwrapResult {
    std::vector<std::uint8_t> Elf;
    std::size_t CopiedSegments = 0;
    std::vector<std::uint16_t> MissingProgramHeaders;
};

bool IsSelf(const std::vector<std::uint8_t>& bytes);
SelfUnwrapResult UnwrapSelf(const std::vector<std::uint8_t>& self);

}

#endif

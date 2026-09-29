#ifndef ELFPATCHER_DARWIN_MACHOTLSREWRITER_HPP
#define ELFPATCHER_DARWIN_MACHOTLSREWRITER_HPP

#include <domain/Types.hpp>
#include <cstdint>
#include <vector>

namespace Elfpatcher::Darwin {

inline constexpr std::uint32_t GuestTcbSlot = 767;

struct TlsRewriteResult {
    std::size_t RewrittenAccesses = 0;
};

TlsRewriteResult RewriteTlsAccesses(std::vector<std::uint8_t>& image, const std::vector<Domain::ProgramHeader>& headers, const Domain::ProgramHeader* tls, bool requireTls);

}

#endif

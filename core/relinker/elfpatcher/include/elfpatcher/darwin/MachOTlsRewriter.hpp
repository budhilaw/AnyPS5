#ifndef ELFPATCHER_DARWIN_MACHOTLSREWRITER_HPP
#define ELFPATCHER_DARWIN_MACHOTLSREWRITER_HPP

#include <domain/Types.hpp>
#include <cstdint>
#include <vector>

namespace Elfpatcher::Darwin {

// The pthread TSD slot that holds the guest thread control block pointer on macOS. Dynamic keys
// are handed out upward from 256 and the array ends at 767, so the top slot is only reached when
// a process has exhausted its keys. libkernel.prx reads the slot from the __tls metadata.
inline constexpr std::uint32_t GuestTcbSlot = 767;

struct TlsRewriteResult {
    std::size_t RewrittenAccesses = 0;
};

// Rewrites every guest read of the thread control block pointer (`mov %fs:0, %rax`, the only
// form the guest ABI support accepts, as on Windows) into `mov %gs:slot*8, %rax` at identical
// length inside the executable segments of `image` (ELF file layout). Throws on any other
// %fs-relative instruction and on TLS access without a usable PT_TLS.
TlsRewriteResult RewriteTlsAccesses(std::vector<std::uint8_t>& image, const std::vector<Domain::ProgramHeader>& headers, const Domain::ProgramHeader* tls, bool requireTls);

}

#endif

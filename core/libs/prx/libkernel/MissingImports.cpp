#include <cstdint>
#include "prx/libc/include/General.hpp"

// Imports of PS5 titles that this library does not implement yet. Every entry point reports
// itself, so a title's first use is diagnosed instead of silently misbehaving. Names come from
// NID matching against known symbol names; the rest are listed by NID (see docs/TechnicalDebt.md).

extern "C" {

// libScePosix is served by libkernel.prx on PS5.
int APS5_VABI shutdown_nid_postfix() { NotImplemented_nid_no_patch("shutdown"); return 0; }
int APS5_VABI getpeername_nid_postfix() { NotImplemented_nid_no_patch("getpeername"); return 0; }
int APS5_VABI sendmsg_nid_postfix() { NotImplemented_nid_no_patch("sendmsg"); return 0; }
int APS5_VABI recvmsg_nid_postfix() { NotImplemented_nid_no_patch("recvmsg"); return 0; }

}

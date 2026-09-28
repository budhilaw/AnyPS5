#include <cstdint>
#include "prx/libc/include/General.hpp"

// Imports of PS5 titles that this library does not implement yet. Every entry point reports
// itself, so a title's first use is diagnosed instead of silently misbehaving. Names come from
// NID matching against known symbol names; the rest are listed by NID (see docs/TechnicalDebt.md).

extern "C" {

int APS5_VABI sceWebBrowserDialogOpen_nid_postfix() { NotImplemented_nid_no_patch("sceWebBrowserDialogOpen"); return 0; }

int APS5_VABI sceWebBrowserDialogUpdateStatus_nid_postfix() { NotImplemented_nid_no_patch("sceWebBrowserDialogUpdateStatus"); return 0; }

}

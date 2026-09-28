#include <cstdint>
#include "prx/libc/include/General.hpp"

// Imports of PS5 titles that this library does not implement yet. Every entry point reports
// itself, so a title's first use is diagnosed instead of silently misbehaving. Entries are listed
// by NID when their name is unknown (see docs/TechnicalDebt.md).

extern "C" {

int APS5_VABI sceLibcMspaceMallocStats_nid_postfix() { NotImplemented_nid_no_patch("sceLibcMspaceMallocStats"); return 0; }
int APS5_VABI sceLibcMspaceMallocStatsFast_nid_postfix() { NotImplemented_nid_no_patch("sceLibcMspaceMallocStatsFast"); return 0; }



APS5_EXPORT("802pFCwC9w0", libcUnknown_802pFCwC9w0);
int APS5_VABI libcUnknown_802pFCwC9w0() { NotImplemented_nid_no_patch(__func__); return 0; }

// The il2cpp garbage collector queries a list of named module segments through this pair
// (query(-1, &count, &bytes), then fill(count, buffer, bytes, &filled)) to register other
// modules' data as roots. The list format is unknown; a failing query is handled by the caller,
// which then registers no foreign roots.
APS5_EXPORT("MTnuKt7HiN0", libcUnknown_MTnuKt7HiN0);
int APS5_VABI libcUnknown_MTnuKt7HiN0() { return -1; }


APS5_EXPORT("aK1Ymf-NhAs", libcUnknown_aK1Ymf_minus_NhAs);
int APS5_VABI libcUnknown_aK1Ymf_minus_NhAs() { NotImplemented_nid_no_patch(__func__); return 0; }

APS5_EXPORT("bRujIheWlB0", libcUnknown_bRujIheWlB0);
int APS5_VABI libcUnknown_bRujIheWlB0() { NotImplemented_nid_no_patch(__func__); return 0; }

APS5_EXPORT("eVFYZnYNDo0", libcUnknown_eVFYZnYNDo0);
int APS5_VABI libcUnknown_eVFYZnYNDo0() { NotImplemented_nid_no_patch(__func__); return 0; }

// The il2cpp garbage collector queries a list of named module segments through this pair
// (query(-1, &count, &bytes), then fill(count, buffer, bytes, &filled)) to register other
// modules' data as roots. The list format is unknown; a failing query is handled by the caller,
// which then registers no foreign roots.
APS5_EXPORT("sMko2YZqDNQ", libcUnknown_sMko2YZqDNQ);
int APS5_VABI libcUnknown_sMko2YZqDNQ() { return -1; }

APS5_EXPORT("vYWK2Pz8vGE", libcUnknown_vYWK2Pz8vGE);
int APS5_VABI libcUnknown_vYWK2Pz8vGE() { NotImplemented_nid_no_patch(__func__); return 0; }

}

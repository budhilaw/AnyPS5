#include <cstdint>
#include "prx/libc/include/General.hpp"

// Imports of PS5 titles that this library does not implement yet. Every entry point reports
// itself, so a title's first use is diagnosed instead of silently misbehaving. Names come from
// NID matching against known symbol names; the rest are listed by NID (see docs/TechnicalDebt.md).

extern "C" {

APS5_EXPORT("Ikfdt-rIqCE", libSceAgcUnknown_Ikfdt_minus_rIqCE);
int APS5_VABI libSceAgcUnknown_Ikfdt_minus_rIqCE() { NotImplemented_nid_no_patch(__func__); return 0; }

APS5_EXPORT("T6xuVw0KUJo", libSceAgcUnknown_T6xuVw0KUJo);
int APS5_VABI libSceAgcUnknown_T6xuVw0KUJo() { NotImplemented_nid_no_patch(__func__); return 0; }

}

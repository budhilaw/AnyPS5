#include <cstdint>
#include "prx/libc/include/General.hpp"
// Imports of PS5 titles that this library does not implement yet (listed by NID, see
// docs/TechnicalDebt.md); the named entry points live in Export.cpp.
extern "C" {
APS5_EXPORT("5VlQSzXW-SQ", libSceHttp2Unknown_5VlQSzXW_minus_SQ);
int APS5_VABI libSceHttp2Unknown_5VlQSzXW_minus_SQ() { NotImplemented_nid_no_patch(__func__); return 0; }
}

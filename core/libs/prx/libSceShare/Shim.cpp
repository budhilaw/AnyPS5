#include "prx/libc/include/General.hpp"
extern "C" int APS5_VABI sceShareTerminate(void);
extern "C" void* libSceShareShim_nid_no_patch() { return reinterpret_cast<void*>(&sceShareTerminate); }

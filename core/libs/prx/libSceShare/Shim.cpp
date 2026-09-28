// PS5 titles import the share API from the module named libSceShare, while this project keeps
// its implementation in libSceShare.native (the console's providing module). This library only
// depends on that one so a module loaded at run time binds its imports there; the reference
// below keeps the dependency alive.
#include "prx/libc/include/General.hpp"
extern "C" int APS5_VABI sceShareTerminate(void);
extern "C" void* libSceShareShim_nid_no_patch() { return reinterpret_cast<void*>(&sceShareTerminate); }

#include <cstdint>
#include "prx/libc/include/General.hpp"


extern "C" {

int APS5_VABI shutdown_nid_postfix() { NotImplemented_nid_no_patch("shutdown"); return 0; }
int APS5_VABI getpeername_nid_postfix() { NotImplemented_nid_no_patch("getpeername"); return 0; }
int APS5_VABI sendmsg_nid_postfix() { NotImplemented_nid_no_patch("sendmsg"); return 0; }
int APS5_VABI recvmsg_nid_postfix() { NotImplemented_nid_no_patch("recvmsg"); return 0; }

}

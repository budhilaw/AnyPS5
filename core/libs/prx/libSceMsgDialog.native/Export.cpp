#include <cstdint>
#include "prx/libc/include/General.hpp"

// Titles import the message dialog through this module name; the implementation lives in
// libSceMsgDialog, which this library depends on so it is loaded alongside.
uint32_t Need_libSceMsgDialogNative = 1;

extern "C" {
APS5_DUMMY_FUN
}

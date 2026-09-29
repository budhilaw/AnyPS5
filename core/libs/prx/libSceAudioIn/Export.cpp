#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioInGetSilentState(int handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAudioInInput(int handle, void* dest) {
 (void)handle;
 (void)dest;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAudioInAsyncOpen_nid_postfix(int userId, std::uint32_t type, std::uint32_t index, std::uint32_t length, std::uint32_t frequency, std::uint32_t parameter) {
 (void)userId; (void)type; (void)index; (void)length; (void)frequency; (void)parameter;
 return static_cast<int>(0x80260109u);
}

int APS5_VABI sceAudioInOpen(int user_id, uint32_t type, uint32_t index, uint32_t len, uint32_t freq, uint32_t param) {
 (void)user_id;
 (void)type;
 (void)index;
 (void)len;
 (void)freq;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

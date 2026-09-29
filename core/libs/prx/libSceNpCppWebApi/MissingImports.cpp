#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mutex>
#include <set>
#include <string>
#include "prx/libc/include/General.hpp"


namespace {

bool readable(std::uint64_t address) {
    if (address == 0) return false;
    mach_vm_address_t start = address;
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    if (mach_vm_region(mach_task_self(), &start, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS) return false;
    return start <= address && address + 32 <= start + size && (info.protection & VM_PROT_READ) != 0;
}

std::string describe(std::uint64_t value) {
    char text[120];
    if (!readable(value)) { std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value)); return text; }
    const auto* bytes = reinterpret_cast<const unsigned char*>(value);
    std::string result = text;
    std::snprintf(text, sizeof(text), "0x%llx ->", static_cast<unsigned long long>(value));
    result = text;
    for (int index = 0; index < 32; ++index) { std::snprintf(text, sizeof(text), " %02x", bytes[index]); result += text; }
    result += " '";
    for (int index = 0; index < 32 && bytes[index] != 0; ++index) result += (bytes[index] >= 0x20 && bytes[index] < 0x7f) ? static_cast<char>(bytes[index]) : '.';
    return result + "'";
}

int Unavailable(const char* name, const void* caller, std::uint64_t a = 0, std::uint64_t b = 0, std::uint64_t c = 0) {
    static std::mutex mutex;
    static std::set<std::string> reported;
    std::lock_guard lock(mutex);
    if (reported.insert(name).second) APS5_LOG_OUT("%s unavailable, caller=%p\n  arg0 %s\n  arg1 %s\n  arg2 0x%llx", name, caller, describe(a).c_str(), describe(b).c_str(), static_cast<unsigned long long>(c));
    static const bool succeed = std::getenv("ANYPS5_NP_STUB_SUCCESS") != nullptr;
    return succeed ? 0 : static_cast<int>(0x80550003u);
}

}

extern "C" {

APS5_EXPORT("+0jo0J7h4CM", sceNpCppWebApiUnknown__plus_0jo0J7h4CM);
int APS5_VABI sceNpCppWebApiUnknown__plus_0jo0J7h4CM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+3KiBw-4CQY", sceNpCppWebApiUnknown__plus_3KiBw_minus_4CQY);
int APS5_VABI sceNpCppWebApiUnknown__plus_3KiBw_minus_4CQY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+5xMqRsZgfE", sceNpCppWebApiUnknown__plus_5xMqRsZgfE);
int APS5_VABI sceNpCppWebApiUnknown__plus_5xMqRsZgfE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+631th1Z4DU", sceNpCppWebApiUnknown__plus_631th1Z4DU);
int APS5_VABI sceNpCppWebApiUnknown__plus_631th1Z4DU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+6Xo+7GdUGM", sceNpCppWebApiUnknown__plus_6Xo_plus_7GdUGM);
int APS5_VABI sceNpCppWebApiUnknown__plus_6Xo_plus_7GdUGM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+79nk0KPcOg", sceNpCppWebApiUnknown__plus_79nk0KPcOg);
int APS5_VABI sceNpCppWebApiUnknown__plus_79nk0KPcOg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+7fmt-RxtKo", sceNpCppWebApiUnknown__plus_7fmt_minus_RxtKo);
int APS5_VABI sceNpCppWebApiUnknown__plus_7fmt_minus_RxtKo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+8CQKp-kv0E", sceNpCppWebApiUnknown__plus_8CQKp_minus_kv0E);
int APS5_VABI sceNpCppWebApiUnknown__plus_8CQKp_minus_kv0E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+DSNphhYdd4", sceNpCppWebApiUnknown__plus_DSNphhYdd4);
int APS5_VABI sceNpCppWebApiUnknown__plus_DSNphhYdd4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+HWmt-AJocg", sceNpCppWebApiUnknown__plus_HWmt_minus_AJocg);
int APS5_VABI sceNpCppWebApiUnknown__plus_HWmt_minus_AJocg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+JJtgAjVwF8", sceNpCppWebApiUnknown__plus_JJtgAjVwF8);
int APS5_VABI sceNpCppWebApiUnknown__plus_JJtgAjVwF8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+L01lTUdzPw", sceNpCppWebApiUnknown__plus_L01lTUdzPw);
int APS5_VABI sceNpCppWebApiUnknown__plus_L01lTUdzPw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+Mu7bP5WvzQ", sceNpCppWebApiUnknown__plus_Mu7bP5WvzQ);
int APS5_VABI sceNpCppWebApiUnknown__plus_Mu7bP5WvzQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+OmKePlbzEo", sceNpCppWebApiUnknown__plus_OmKePlbzEo);
int APS5_VABI sceNpCppWebApiUnknown__plus_OmKePlbzEo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+Ts4tkJjqS8", sceNpCppWebApiUnknown__plus_Ts4tkJjqS8);
int APS5_VABI sceNpCppWebApiUnknown__plus_Ts4tkJjqS8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+W7fL6PWOhw", sceNpCppWebApiUnknown__plus_W7fL6PWOhw);
int APS5_VABI sceNpCppWebApiUnknown__plus_W7fL6PWOhw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+Xe2-TJDSJg", sceNpCppWebApiUnknown__plus_Xe2_minus_TJDSJg);
int APS5_VABI sceNpCppWebApiUnknown__plus_Xe2_minus_TJDSJg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+YrfcHy+etk", sceNpCppWebApiUnknown__plus_YrfcHy_plus_etk);
int APS5_VABI sceNpCppWebApiUnknown__plus_YrfcHy_plus_etk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+aNQf0t6YIc", sceNpCppWebApiUnknown__plus_aNQf0t6YIc);
int APS5_VABI sceNpCppWebApiUnknown__plus_aNQf0t6YIc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+bKHGz+nfxA", sceNpCppWebApiUnknown__plus_bKHGz_plus_nfxA);
int APS5_VABI sceNpCppWebApiUnknown__plus_bKHGz_plus_nfxA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+cHQSR34crs", sceNpCppWebApiUnknown__plus_cHQSR34crs);
int APS5_VABI sceNpCppWebApiUnknown__plus_cHQSR34crs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+e3Bdk+MqN8", sceNpCppWebApiUnknown__plus_e3Bdk_plus_MqN8);
int APS5_VABI sceNpCppWebApiUnknown__plus_e3Bdk_plus_MqN8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+f3mpQWp5pU", sceNpCppWebApiUnknown__plus_f3mpQWp5pU);
int APS5_VABI sceNpCppWebApiUnknown__plus_f3mpQWp5pU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+ffmW8Y1MMY", sceNpCppWebApiUnknown__plus_ffmW8Y1MMY);
int APS5_VABI sceNpCppWebApiUnknown__plus_ffmW8Y1MMY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+gSMGXHgrPU", sceNpCppWebApiUnknown__plus_gSMGXHgrPU);
int APS5_VABI sceNpCppWebApiUnknown__plus_gSMGXHgrPU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+htBBgi85YA", sceNpCppWebApiUnknown__plus_htBBgi85YA);
int APS5_VABI sceNpCppWebApiUnknown__plus_htBBgi85YA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+iXDFPESNGs", sceNpCppWebApiUnknown__plus_iXDFPESNGs);
int APS5_VABI sceNpCppWebApiUnknown__plus_iXDFPESNGs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+isUKw4zud4", sceNpCppWebApiUnknown__plus_isUKw4zud4);
int APS5_VABI sceNpCppWebApiUnknown__plus_isUKw4zud4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+k2KKLVS+t8", sceNpCppWebApiUnknown__plus_k2KKLVS_plus_t8);
int APS5_VABI sceNpCppWebApiUnknown__plus_k2KKLVS_plus_t8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+kE3dyEXHQQ", sceNpCppWebApiUnknown__plus_kE3dyEXHQQ);
int APS5_VABI sceNpCppWebApiUnknown__plus_kE3dyEXHQQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+ljJoKsr054", sceNpCppWebApiUnknown__plus_ljJoKsr054);
int APS5_VABI sceNpCppWebApiUnknown__plus_ljJoKsr054(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+oKFbYmGql8", sceNpCppWebApiUnknown__plus_oKFbYmGql8);
int APS5_VABI sceNpCppWebApiUnknown__plus_oKFbYmGql8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+oqyRiwxW18", sceNpCppWebApiUnknown__plus_oqyRiwxW18);
int APS5_VABI sceNpCppWebApiUnknown__plus_oqyRiwxW18(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+pICIYLYO18", sceNpCppWebApiUnknown__plus_pICIYLYO18);
int APS5_VABI sceNpCppWebApiUnknown__plus_pICIYLYO18(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+r0zCIliT6c", sceNpCppWebApiUnknown__plus_r0zCIliT6c);
int APS5_VABI sceNpCppWebApiUnknown__plus_r0zCIliT6c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("+rWjmNf5G5E", sceNpCppWebApiUnknown__plus_rWjmNf5G5E);
int APS5_VABI sceNpCppWebApiUnknown__plus_rWjmNf5G5E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-0Vv7HKZaBM", sceNpCppWebApiUnknown__minus_0Vv7HKZaBM);
int APS5_VABI sceNpCppWebApiUnknown__minus_0Vv7HKZaBM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-4qn5SHAuoQ", sceNpCppWebApiUnknown__minus_4qn5SHAuoQ);
int APS5_VABI sceNpCppWebApiUnknown__minus_4qn5SHAuoQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-8Jm4zqdPMs", sceNpCppWebApiUnknown__minus_8Jm4zqdPMs);
int APS5_VABI sceNpCppWebApiUnknown__minus_8Jm4zqdPMs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-AuznSOJ2AQ", sceNpCppWebApiUnknown__minus_AuznSOJ2AQ);
int APS5_VABI sceNpCppWebApiUnknown__minus_AuznSOJ2AQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-BJ6og-w7Ho", sceNpCppWebApiUnknown__minus_BJ6og_minus_w7Ho);
int APS5_VABI sceNpCppWebApiUnknown__minus_BJ6og_minus_w7Ho(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-D8MD+5yuAw", sceNpCppWebApiUnknown__minus_D8MD_plus_5yuAw);
int APS5_VABI sceNpCppWebApiUnknown__minus_D8MD_plus_5yuAw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-DDmi3EClBw", sceNpCppWebApiUnknown__minus_DDmi3EClBw);
int APS5_VABI sceNpCppWebApiUnknown__minus_DDmi3EClBw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-DG+cSNIXSk", sceNpCppWebApiUnknown__minus_DG_plus_cSNIXSk);
int APS5_VABI sceNpCppWebApiUnknown__minus_DG_plus_cSNIXSk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-EL6-8BIXf4", sceNpCppWebApiUnknown__minus_EL6_minus_8BIXf4);
int APS5_VABI sceNpCppWebApiUnknown__minus_EL6_minus_8BIXf4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-G+2nwDM4jY", sceNpCppWebApiUnknown__minus_G_plus_2nwDM4jY);
int APS5_VABI sceNpCppWebApiUnknown__minus_G_plus_2nwDM4jY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-KwWzv43lB8", sceNpCppWebApiUnknown__minus_KwWzv43lB8);
int APS5_VABI sceNpCppWebApiUnknown__minus_KwWzv43lB8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-NWoybNRyYQ", sceNpCppWebApiUnknown__minus_NWoybNRyYQ);
int APS5_VABI sceNpCppWebApiUnknown__minus_NWoybNRyYQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-P4MeSsk4WA", sceNpCppWebApiUnknown__minus_P4MeSsk4WA);
int APS5_VABI sceNpCppWebApiUnknown__minus_P4MeSsk4WA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-PXuTfPzcT4", sceNpCppWebApiUnknown__minus_PXuTfPzcT4);
int APS5_VABI sceNpCppWebApiUnknown__minus_PXuTfPzcT4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-PimsYZDKEQ", sceNpCppWebApiUnknown__minus_PimsYZDKEQ);
int APS5_VABI sceNpCppWebApiUnknown__minus_PimsYZDKEQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-QRPOVJU4aU", sceNpCppWebApiUnknown__minus_QRPOVJU4aU);
int APS5_VABI sceNpCppWebApiUnknown__minus_QRPOVJU4aU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-QZoGsAmbh8", sceNpCppWebApiUnknown__minus_QZoGsAmbh8);
int APS5_VABI sceNpCppWebApiUnknown__minus_QZoGsAmbh8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-QgqOT5u2Vk", sceNpCppWebApiUnknown__minus_QgqOT5u2Vk);
int APS5_VABI sceNpCppWebApiUnknown__minus_QgqOT5u2Vk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-QkxkxtVpvI", sceNpCppWebApiUnknown__minus_QkxkxtVpvI);
int APS5_VABI sceNpCppWebApiUnknown__minus_QkxkxtVpvI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-QnkldOVkvg", sceNpCppWebApiUnknown__minus_QnkldOVkvg);
int APS5_VABI sceNpCppWebApiUnknown__minus_QnkldOVkvg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-Rjp3-YViXc", sceNpCppWebApiUnknown__minus_Rjp3_minus_YViXc);
int APS5_VABI sceNpCppWebApiUnknown__minus_Rjp3_minus_YViXc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-RudVpLkf3o", sceNpCppWebApiUnknown__minus_RudVpLkf3o);
int APS5_VABI sceNpCppWebApiUnknown__minus_RudVpLkf3o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-UjDJUZo-yA", sceNpCppWebApiUnknown__minus_UjDJUZo_minus_yA);
int APS5_VABI sceNpCppWebApiUnknown__minus_UjDJUZo_minus_yA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-XNfwZTFkWw", sceNpCppWebApiUnknown__minus_XNfwZTFkWw);
int APS5_VABI sceNpCppWebApiUnknown__minus_XNfwZTFkWw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-bI+1NDXkHM", sceNpCppWebApiUnknown__minus_bI_plus_1NDXkHM);
int APS5_VABI sceNpCppWebApiUnknown__minus_bI_plus_1NDXkHM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-cLIfSisruo", sceNpCppWebApiUnknown__minus_cLIfSisruo);
int APS5_VABI sceNpCppWebApiUnknown__minus_cLIfSisruo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-cZ3SCHmrb4", sceNpCppWebApiUnknown__minus_cZ3SCHmrb4);
int APS5_VABI sceNpCppWebApiUnknown__minus_cZ3SCHmrb4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-dN5muz7ECM", sceNpCppWebApiUnknown__minus_dN5muz7ECM);
int APS5_VABI sceNpCppWebApiUnknown__minus_dN5muz7ECM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-f7k+OSHj3g", sceNpCppWebApiUnknown__minus_f7k_plus_OSHj3g);
int APS5_VABI sceNpCppWebApiUnknown__minus_f7k_plus_OSHj3g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-hJRce8wn1U", sceNpCppWebApiUnknown__minus_hJRce8wn1U);
int APS5_VABI sceNpCppWebApiUnknown__minus_hJRce8wn1U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-haXZfTh0pg", sceNpCppWebApiUnknown__minus_haXZfTh0pg);
int APS5_VABI sceNpCppWebApiUnknown__minus_haXZfTh0pg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-m--kjUtWb0", sceNpCppWebApiUnknown__minus_m_minus__minus_kjUtWb0);
int APS5_VABI sceNpCppWebApiUnknown__minus_m_minus__minus_kjUtWb0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-m1cU01p3fY", sceNpCppWebApiUnknown__minus_m1cU01p3fY);
int APS5_VABI sceNpCppWebApiUnknown__minus_m1cU01p3fY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-nIs+yxg514", sceNpCppWebApiUnknown__minus_nIs_plus_yxg514);
int APS5_VABI sceNpCppWebApiUnknown__minus_nIs_plus_yxg514(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-p6Rz-s2DY8", sceNpCppWebApiUnknown__minus_p6Rz_minus_s2DY8);
int APS5_VABI sceNpCppWebApiUnknown__minus_p6Rz_minus_s2DY8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-pQZrVK6Lj0", sceNpCppWebApiUnknown__minus_pQZrVK6Lj0);
int APS5_VABI sceNpCppWebApiUnknown__minus_pQZrVK6Lj0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-q5NR3+q2MM", sceNpCppWebApiUnknown__minus_q5NR3_plus_q2MM);
int APS5_VABI sceNpCppWebApiUnknown__minus_q5NR3_plus_q2MM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-qlyOH8CIzg", sceNpCppWebApiUnknown__minus_qlyOH8CIzg);
int APS5_VABI sceNpCppWebApiUnknown__minus_qlyOH8CIzg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-t8eK4WxVIo", sceNpCppWebApiUnknown__minus_t8eK4WxVIo);
int APS5_VABI sceNpCppWebApiUnknown__minus_t8eK4WxVIo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("-uzJDP09EZA", sceNpCppWebApiUnknown__minus_uzJDP09EZA);
int APS5_VABI sceNpCppWebApiUnknown__minus_uzJDP09EZA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0-JKIdd-Rvk", sceNpCppWebApiUnknown_0_minus_JKIdd_minus_Rvk);
int APS5_VABI sceNpCppWebApiUnknown_0_minus_JKIdd_minus_Rvk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("01ZGV2VvBPU", sceNpCppWebApiUnknown_01ZGV2VvBPU);
int APS5_VABI sceNpCppWebApiUnknown_01ZGV2VvBPU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("05E7yxe+58U", sceNpCppWebApiUnknown_05E7yxe_plus_58U);
int APS5_VABI sceNpCppWebApiUnknown_05E7yxe_plus_58U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("05OxhjjEjKg", sceNpCppWebApiUnknown_05OxhjjEjKg);
int APS5_VABI sceNpCppWebApiUnknown_05OxhjjEjKg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("09K3Rbhq3MM", sceNpCppWebApiUnknown_09K3Rbhq3MM);
int APS5_VABI sceNpCppWebApiUnknown_09K3Rbhq3MM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0AgCOypbQ90", sceNpCppWebApiUnknown_0AgCOypbQ90);
int APS5_VABI sceNpCppWebApiUnknown_0AgCOypbQ90(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0B44XhgWEI8", sceNpCppWebApiUnknown_0B44XhgWEI8);
int APS5_VABI sceNpCppWebApiUnknown_0B44XhgWEI8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0BortfYmQRk", sceNpCppWebApiUnknown_0BortfYmQRk);
int APS5_VABI sceNpCppWebApiUnknown_0BortfYmQRk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0CAesfH963Q", sceNpCppWebApiUnknown_0CAesfH963Q);
int APS5_VABI sceNpCppWebApiUnknown_0CAesfH963Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0ELeILzx6IU", sceNpCppWebApiUnknown_0ELeILzx6IU);
int APS5_VABI sceNpCppWebApiUnknown_0ELeILzx6IU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0Gqs466l0tQ", sceNpCppWebApiUnknown_0Gqs466l0tQ);
int APS5_VABI sceNpCppWebApiUnknown_0Gqs466l0tQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0LmvVuAAk2I", sceNpCppWebApiUnknown_0LmvVuAAk2I);
int APS5_VABI sceNpCppWebApiUnknown_0LmvVuAAk2I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0PT319lwTJ8", sceNpCppWebApiUnknown_0PT319lwTJ8);
int APS5_VABI sceNpCppWebApiUnknown_0PT319lwTJ8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0PWWUoAJ5AQ", sceNpCppWebApiUnknown_0PWWUoAJ5AQ);
int APS5_VABI sceNpCppWebApiUnknown_0PWWUoAJ5AQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0Q4OH-nX6q4", sceNpCppWebApiUnknown_0Q4OH_minus_nX6q4);
int APS5_VABI sceNpCppWebApiUnknown_0Q4OH_minus_nX6q4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0Q6fQRvfhUs", sceNpCppWebApiUnknown_0Q6fQRvfhUs);
int APS5_VABI sceNpCppWebApiUnknown_0Q6fQRvfhUs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0RGtfPgW4AU", sceNpCppWebApiUnknown_0RGtfPgW4AU);
int APS5_VABI sceNpCppWebApiUnknown_0RGtfPgW4AU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0UJ6QCPKLD0", sceNpCppWebApiUnknown_0UJ6QCPKLD0);
int APS5_VABI sceNpCppWebApiUnknown_0UJ6QCPKLD0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0UNp2Ey5bw4", sceNpCppWebApiUnknown_0UNp2Ey5bw4);
int APS5_VABI sceNpCppWebApiUnknown_0UNp2Ey5bw4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0WFbrRUy+N4", sceNpCppWebApiUnknown_0WFbrRUy_plus_N4);
int APS5_VABI sceNpCppWebApiUnknown_0WFbrRUy_plus_N4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0YED2+p7aiI", sceNpCppWebApiUnknown_0YED2_plus_p7aiI);
int APS5_VABI sceNpCppWebApiUnknown_0YED2_plus_p7aiI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
int APS5_VABI sceNpCommerceDialogInitialize_nid_postfix() { NotImplemented_nid_no_patch("sceNpCommerceDialogInitialize"); return 0; }
APS5_EXPORT("0blWarRALu4", sceNpCppWebApiUnknown_0blWarRALu4);
int APS5_VABI sceNpCppWebApiUnknown_0blWarRALu4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0cHb7R21ttg", sceNpCppWebApiUnknown_0cHb7R21ttg);
int APS5_VABI sceNpCppWebApiUnknown_0cHb7R21ttg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0d+KWEyu588", sceNpCppWebApiUnknown_0d_plus_KWEyu588);
int APS5_VABI sceNpCppWebApiUnknown_0d_plus_KWEyu588(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0f5+eBcgB14", sceNpCppWebApiUnknown_0f5_plus_eBcgB14);
int APS5_VABI sceNpCppWebApiUnknown_0f5_plus_eBcgB14(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0fHBm+BJ3xQ", sceNpCppWebApiUnknown_0fHBm_plus_BJ3xQ);
int APS5_VABI sceNpCppWebApiUnknown_0fHBm_plus_BJ3xQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0fy7OoflkxE", sceNpCppWebApiUnknown_0fy7OoflkxE);
int APS5_VABI sceNpCppWebApiUnknown_0fy7OoflkxE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0hidak6-XOo", sceNpCppWebApiUnknown_0hidak6_minus_XOo);
int APS5_VABI sceNpCppWebApiUnknown_0hidak6_minus_XOo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0i331fsne3g", sceNpCppWebApiUnknown_0i331fsne3g);
int APS5_VABI sceNpCppWebApiUnknown_0i331fsne3g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0jei2te6zqo", sceNpCppWebApiUnknown_0jei2te6zqo);
int APS5_VABI sceNpCppWebApiUnknown_0jei2te6zqo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0juFlJ9R6ss", sceNpCppWebApiUnknown_0juFlJ9R6ss);
int APS5_VABI sceNpCppWebApiUnknown_0juFlJ9R6ss(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0lWfUsWc54A", sceNpCppWebApiUnknown_0lWfUsWc54A);
int APS5_VABI sceNpCppWebApiUnknown_0lWfUsWc54A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0mylfGVkteI", sceNpCppWebApiUnknown_0mylfGVkteI);
int APS5_VABI sceNpCppWebApiUnknown_0mylfGVkteI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0oIoKcDA0Fw", sceNpCppWebApiUnknown_0oIoKcDA0Fw);
int APS5_VABI sceNpCppWebApiUnknown_0oIoKcDA0Fw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0oLuG3JxP8Q", sceNpCppWebApiUnknown_0oLuG3JxP8Q);
int APS5_VABI sceNpCppWebApiUnknown_0oLuG3JxP8Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0pwb8fOBwpQ", sceNpCppWebApiUnknown_0pwb8fOBwpQ);
int APS5_VABI sceNpCppWebApiUnknown_0pwb8fOBwpQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0tss2TNi7lI", sceNpCppWebApiUnknown_0tss2TNi7lI);
int APS5_VABI sceNpCppWebApiUnknown_0tss2TNi7lI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("0zZqWJ1vWtY", sceNpCppWebApiUnknown_0zZqWJ1vWtY);
int APS5_VABI sceNpCppWebApiUnknown_0zZqWJ1vWtY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1-TVrBGsKl4", sceNpCppWebApiUnknown_1_minus_TVrBGsKl4);
int APS5_VABI sceNpCppWebApiUnknown_1_minus_TVrBGsKl4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1-jw2YaVrug", sceNpCppWebApiUnknown_1_minus_jw2YaVrug);
int APS5_VABI sceNpCppWebApiUnknown_1_minus_jw2YaVrug(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("13KoEaVmuJ8", sceNpCppWebApiUnknown_13KoEaVmuJ8);
int APS5_VABI sceNpCppWebApiUnknown_13KoEaVmuJ8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("15qvbsddabA", sceNpCppWebApiUnknown_15qvbsddabA);
int APS5_VABI sceNpCppWebApiUnknown_15qvbsddabA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1Ae30UkB0f8", sceNpCppWebApiUnknown_1Ae30UkB0f8);
int APS5_VABI sceNpCppWebApiUnknown_1Ae30UkB0f8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1CXuBSTalPk", sceNpCppWebApiUnknown_1CXuBSTalPk);
int APS5_VABI sceNpCppWebApiUnknown_1CXuBSTalPk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1ECw9RP03o4", sceNpCppWebApiUnknown_1ECw9RP03o4);
int APS5_VABI sceNpCppWebApiUnknown_1ECw9RP03o4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1FITx-pXXA0", sceNpCppWebApiUnknown_1FITx_minus_pXXA0);
int APS5_VABI sceNpCppWebApiUnknown_1FITx_minus_pXXA0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1FkQgc1yllA", sceNpCppWebApiUnknown_1FkQgc1yllA);
int APS5_VABI sceNpCppWebApiUnknown_1FkQgc1yllA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1JV7Mv590Cc", sceNpCppWebApiUnknown_1JV7Mv590Cc);
int APS5_VABI sceNpCppWebApiUnknown_1JV7Mv590Cc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1JeltNNTV9w", sceNpCppWebApiUnknown_1JeltNNTV9w);
int APS5_VABI sceNpCppWebApiUnknown_1JeltNNTV9w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1Li76cHWOz0", sceNpCppWebApiUnknown_1Li76cHWOz0);
int APS5_VABI sceNpCppWebApiUnknown_1Li76cHWOz0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1LkTUUovTg0", sceNpCppWebApiUnknown_1LkTUUovTg0);
int APS5_VABI sceNpCppWebApiUnknown_1LkTUUovTg0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1NFlJKXWi-I", sceNpCppWebApiUnknown_1NFlJKXWi_minus_I);
int APS5_VABI sceNpCppWebApiUnknown_1NFlJKXWi_minus_I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1VbEHp0e18w", sceNpCppWebApiUnknown_1VbEHp0e18w);
int APS5_VABI sceNpCppWebApiUnknown_1VbEHp0e18w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1WCDRJQQWwM", sceNpCppWebApiUnknown_1WCDRJQQWwM);
int APS5_VABI sceNpCppWebApiUnknown_1WCDRJQQWwM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1XJ6huZVErw", sceNpCppWebApiUnknown_1XJ6huZVErw);
int APS5_VABI sceNpCppWebApiUnknown_1XJ6huZVErw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1YEU5F0Bm98", sceNpCppWebApiUnknown_1YEU5F0Bm98);
int APS5_VABI sceNpCppWebApiUnknown_1YEU5F0Bm98(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1Z-ZRnYadZk", sceNpCppWebApiUnknown_1Z_minus_ZRnYadZk);
int APS5_VABI sceNpCppWebApiUnknown_1Z_minus_ZRnYadZk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1ZOjxb5kVLs", sceNpCppWebApiUnknown_1ZOjxb5kVLs);
int APS5_VABI sceNpCppWebApiUnknown_1ZOjxb5kVLs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1bf+c7ebx0A", sceNpCppWebApiUnknown_1bf_plus_c7ebx0A);
int APS5_VABI sceNpCppWebApiUnknown_1bf_plus_c7ebx0A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1crJbUuDMoE", sceNpCppWebApiUnknown_1crJbUuDMoE);
int APS5_VABI sceNpCppWebApiUnknown_1crJbUuDMoE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1fSMXUsmn68", sceNpCppWebApiUnknown_1fSMXUsmn68);
int APS5_VABI sceNpCppWebApiUnknown_1fSMXUsmn68(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1hVd+ogdk94", sceNpCppWebApiUnknown_1hVd_plus_ogdk94);
int APS5_VABI sceNpCppWebApiUnknown_1hVd_plus_ogdk94(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1iq5Jtw4jVs", sceNpCppWebApiUnknown_1iq5Jtw4jVs);
int APS5_VABI sceNpCppWebApiUnknown_1iq5Jtw4jVs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1m9Qb8CcPhk", sceNpCppWebApiUnknown_1m9Qb8CcPhk);
int APS5_VABI sceNpCppWebApiUnknown_1m9Qb8CcPhk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1nafvh3INrY", sceNpCppWebApiUnknown_1nafvh3INrY);
int APS5_VABI sceNpCppWebApiUnknown_1nafvh3INrY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1navP4CR1VI", sceNpCppWebApiUnknown_1navP4CR1VI);
int APS5_VABI sceNpCppWebApiUnknown_1navP4CR1VI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1nuXnpuD75M", sceNpCppWebApiUnknown_1nuXnpuD75M);
int APS5_VABI sceNpCppWebApiUnknown_1nuXnpuD75M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1rlHMruxwHk", sceNpCppWebApiUnknown_1rlHMruxwHk);
int APS5_VABI sceNpCppWebApiUnknown_1rlHMruxwHk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1wpCXuFzH10", sceNpCppWebApiUnknown_1wpCXuFzH10);
int APS5_VABI sceNpCppWebApiUnknown_1wpCXuFzH10(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1xYklDtHGtw", sceNpCppWebApiUnknown_1xYklDtHGtw);
int APS5_VABI sceNpCppWebApiUnknown_1xYklDtHGtw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("1xoLMLSYmE8", sceNpCppWebApiUnknown_1xoLMLSYmE8);
int APS5_VABI sceNpCppWebApiUnknown_1xoLMLSYmE8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("203JffNGrbg", sceNpCppWebApiUnknown_203JffNGrbg);
int APS5_VABI sceNpCppWebApiUnknown_203JffNGrbg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("21XnfADVJHc", sceNpCppWebApiUnknown_21XnfADVJHc);
int APS5_VABI sceNpCppWebApiUnknown_21XnfADVJHc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("23Bii2HafS4", sceNpCppWebApiUnknown_23Bii2HafS4);
int APS5_VABI sceNpCppWebApiUnknown_23Bii2HafS4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("23l-xRYzmaY", sceNpCppWebApiUnknown_23l_minus_xRYzmaY);
int APS5_VABI sceNpCppWebApiUnknown_23l_minus_xRYzmaY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("25OYNt0MWG8", sceNpCppWebApiUnknown_25OYNt0MWG8);
int APS5_VABI sceNpCppWebApiUnknown_25OYNt0MWG8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("26SufKG5x4M", sceNpCppWebApiUnknown_26SufKG5x4M);
int APS5_VABI sceNpCppWebApiUnknown_26SufKG5x4M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("26wN8Yjf9Fk", sceNpCppWebApiUnknown_26wN8Yjf9Fk);
int APS5_VABI sceNpCppWebApiUnknown_26wN8Yjf9Fk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("29DLZns5UrA", sceNpCppWebApiUnknown_29DLZns5UrA);
int APS5_VABI sceNpCppWebApiUnknown_29DLZns5UrA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2AA9QT1IUgs", sceNpCppWebApiUnknown_2AA9QT1IUgs);
int APS5_VABI sceNpCppWebApiUnknown_2AA9QT1IUgs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2AE+1w-vUeo", sceNpCppWebApiUnknown_2AE_plus_1w_minus_vUeo);
int APS5_VABI sceNpCppWebApiUnknown_2AE_plus_1w_minus_vUeo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2HRyZzguZzs", sceNpCppWebApiUnknown_2HRyZzguZzs);
int APS5_VABI sceNpCppWebApiUnknown_2HRyZzguZzs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2HSrPeSIBV0", sceNpCppWebApiUnknown_2HSrPeSIBV0);
int APS5_VABI sceNpCppWebApiUnknown_2HSrPeSIBV0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2JEkKBeGsPM", sceNpCppWebApiUnknown_2JEkKBeGsPM);
int APS5_VABI sceNpCppWebApiUnknown_2JEkKBeGsPM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2KqNbE73+aI", sceNpCppWebApiUnknown_2KqNbE73_plus_aI);
int APS5_VABI sceNpCppWebApiUnknown_2KqNbE73_plus_aI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2LooX97-5b8", sceNpCppWebApiUnknown_2LooX97_minus_5b8);
int APS5_VABI sceNpCppWebApiUnknown_2LooX97_minus_5b8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2OwIlPSvjKw", sceNpCppWebApiUnknown_2OwIlPSvjKw);
int APS5_VABI sceNpCppWebApiUnknown_2OwIlPSvjKw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2V-RL56o5cU", sceNpCppWebApiUnknown_2V_minus_RL56o5cU);
int APS5_VABI sceNpCppWebApiUnknown_2V_minus_RL56o5cU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2VXjgJz9nqs", sceNpCppWebApiUnknown_2VXjgJz9nqs);
int APS5_VABI sceNpCppWebApiUnknown_2VXjgJz9nqs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2bAMWBUewu0", sceNpCppWebApiUnknown_2bAMWBUewu0);
int APS5_VABI sceNpCppWebApiUnknown_2bAMWBUewu0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2d4md5rdtQc", sceNpCppWebApiUnknown_2d4md5rdtQc);
int APS5_VABI sceNpCppWebApiUnknown_2d4md5rdtQc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2eoKrZYhFTw", sceNpCppWebApiUnknown_2eoKrZYhFTw);
int APS5_VABI sceNpCppWebApiUnknown_2eoKrZYhFTw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2gTQ81js94I", sceNpCppWebApiUnknown_2gTQ81js94I);
int APS5_VABI sceNpCppWebApiUnknown_2gTQ81js94I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2iNYrcGDF6g", sceNpCppWebApiUnknown_2iNYrcGDF6g);
int APS5_VABI sceNpCppWebApiUnknown_2iNYrcGDF6g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2naa5fV2BkU", sceNpCppWebApiUnknown_2naa5fV2BkU);
int APS5_VABI sceNpCppWebApiUnknown_2naa5fV2BkU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2teIv3bTDTA", sceNpCppWebApiUnknown_2teIv3bTDTA);
int APS5_VABI sceNpCppWebApiUnknown_2teIv3bTDTA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("2uouw57e7qM", sceNpCppWebApiUnknown_2uouw57e7qM);
int APS5_VABI sceNpCppWebApiUnknown_2uouw57e7qM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("30Vzv4jmzxc", sceNpCppWebApiUnknown_30Vzv4jmzxc);
int APS5_VABI sceNpCppWebApiUnknown_30Vzv4jmzxc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("30jjAlUhKRU", sceNpCppWebApiUnknown_30jjAlUhKRU);
int APS5_VABI sceNpCppWebApiUnknown_30jjAlUhKRU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("33WTIzHLTe4", sceNpCppWebApiUnknown_33WTIzHLTe4);
int APS5_VABI sceNpCppWebApiUnknown_33WTIzHLTe4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("36FCNqdy-fE", sceNpCppWebApiUnknown_36FCNqdy_minus_fE);
int APS5_VABI sceNpCppWebApiUnknown_36FCNqdy_minus_fE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("37N6qDFqSjs", sceNpCppWebApiUnknown_37N6qDFqSjs);
int APS5_VABI sceNpCppWebApiUnknown_37N6qDFqSjs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("38MHJa5nLlU", sceNpCppWebApiUnknown_38MHJa5nLlU);
int APS5_VABI sceNpCppWebApiUnknown_38MHJa5nLlU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("39c+H8bSITU", sceNpCppWebApiUnknown_39c_plus_H8bSITU);
int APS5_VABI sceNpCppWebApiUnknown_39c_plus_H8bSITU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3EZDHVOP7vk", sceNpCppWebApiUnknown_3EZDHVOP7vk);
int APS5_VABI sceNpCppWebApiUnknown_3EZDHVOP7vk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3FtHDoI1J+w", sceNpCppWebApiUnknown_3FtHDoI1J_plus_w);
int APS5_VABI sceNpCppWebApiUnknown_3FtHDoI1J_plus_w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3Ka458TAnU0", sceNpCppWebApiUnknown_3Ka458TAnU0);
int APS5_VABI sceNpCppWebApiUnknown_3Ka458TAnU0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3L1hfNaKKJE", sceNpCppWebApiUnknown_3L1hfNaKKJE);
int APS5_VABI sceNpCppWebApiUnknown_3L1hfNaKKJE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3LFVKmoavF4", sceNpCppWebApiUnknown_3LFVKmoavF4);
int APS5_VABI sceNpCppWebApiUnknown_3LFVKmoavF4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3Qef+GNRYkc", sceNpCppWebApiUnknown_3Qef_plus_GNRYkc);
int APS5_VABI sceNpCppWebApiUnknown_3Qef_plus_GNRYkc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3ToWJ-uk1Uk", sceNpCppWebApiUnknown_3ToWJ_minus_uk1Uk);
int APS5_VABI sceNpCppWebApiUnknown_3ToWJ_minus_uk1Uk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3TyDpOiEZnQ", sceNpCppWebApiUnknown_3TyDpOiEZnQ);
int APS5_VABI sceNpCppWebApiUnknown_3TyDpOiEZnQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3UZcxMW6mkM", sceNpCppWebApiUnknown_3UZcxMW6mkM);
int APS5_VABI sceNpCppWebApiUnknown_3UZcxMW6mkM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3Ua-YPM5wKw", sceNpCppWebApiUnknown_3Ua_minus_YPM5wKw);
int APS5_VABI sceNpCppWebApiUnknown_3Ua_minus_YPM5wKw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3WhVurvjPns", sceNpCppWebApiUnknown_3WhVurvjPns);
int APS5_VABI sceNpCppWebApiUnknown_3WhVurvjPns(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3YQ1qY5ePUk", sceNpCppWebApiUnknown_3YQ1qY5ePUk);
int APS5_VABI sceNpCppWebApiUnknown_3YQ1qY5ePUk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3Yk4YqA8QVk", sceNpCppWebApiUnknown_3Yk4YqA8QVk);
int APS5_VABI sceNpCppWebApiUnknown_3Yk4YqA8QVk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3Yn6A-wreds", sceNpCppWebApiUnknown_3Yn6A_minus_wreds);
int APS5_VABI sceNpCppWebApiUnknown_3Yn6A_minus_wreds(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3bjEFf8OhTs", sceNpCppWebApiUnknown_3bjEFf8OhTs);
int APS5_VABI sceNpCppWebApiUnknown_3bjEFf8OhTs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3c1SP6vjqcs", sceNpCppWebApiUnknown_3c1SP6vjqcs);
int APS5_VABI sceNpCppWebApiUnknown_3c1SP6vjqcs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3dtZDlg1TYg", sceNpCppWebApiUnknown_3dtZDlg1TYg);
int APS5_VABI sceNpCppWebApiUnknown_3dtZDlg1TYg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3iNVb-F3hUk", sceNpCppWebApiUnknown_3iNVb_minus_F3hUk);
int APS5_VABI sceNpCppWebApiUnknown_3iNVb_minus_F3hUk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3jUeuJshJS4", sceNpCppWebApiUnknown_3jUeuJshJS4);
int APS5_VABI sceNpCppWebApiUnknown_3jUeuJshJS4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3lC95nbky6U", sceNpCppWebApiUnknown_3lC95nbky6U);
int APS5_VABI sceNpCppWebApiUnknown_3lC95nbky6U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3nUMOKQgTvU", sceNpCppWebApiUnknown_3nUMOKQgTvU);
int APS5_VABI sceNpCppWebApiUnknown_3nUMOKQgTvU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3nl2tr-OsCI", sceNpCppWebApiUnknown_3nl2tr_minus_OsCI);
int APS5_VABI sceNpCppWebApiUnknown_3nl2tr_minus_OsCI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3oE80CtovHg", sceNpCppWebApiUnknown_3oE80CtovHg);
int APS5_VABI sceNpCppWebApiUnknown_3oE80CtovHg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3pEjZ5H4+1g", sceNpCppWebApiUnknown_3pEjZ5H4_plus_1g);
int APS5_VABI sceNpCppWebApiUnknown_3pEjZ5H4_plus_1g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3qMDe77-7-E", sceNpCppWebApiUnknown_3qMDe77_minus_7_minus_E);
int APS5_VABI sceNpCppWebApiUnknown_3qMDe77_minus_7_minus_E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3rK86aObY4E", sceNpCppWebApiUnknown_3rK86aObY4E);
int APS5_VABI sceNpCppWebApiUnknown_3rK86aObY4E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3sNsQzNdylI", sceNpCppWebApiUnknown_3sNsQzNdylI);
int APS5_VABI sceNpCppWebApiUnknown_3sNsQzNdylI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3xoP69U8NGs", sceNpCppWebApiUnknown_3xoP69U8NGs);
int APS5_VABI sceNpCppWebApiUnknown_3xoP69U8NGs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("3zIKKxAWo38", sceNpCppWebApiUnknown_3zIKKxAWo38);
int APS5_VABI sceNpCppWebApiUnknown_3zIKKxAWo38(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("40BxQHhkVGI", sceNpCppWebApiUnknown_40BxQHhkVGI);
int APS5_VABI sceNpCppWebApiUnknown_40BxQHhkVGI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("43D-Qbm3Tos", sceNpCppWebApiUnknown_43D_minus_Qbm3Tos);
int APS5_VABI sceNpCppWebApiUnknown_43D_minus_Qbm3Tos(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("449uAhTZ1oc", sceNpCppWebApiUnknown_449uAhTZ1oc);
int APS5_VABI sceNpCppWebApiUnknown_449uAhTZ1oc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("47JlRwShq3M", sceNpCppWebApiUnknown_47JlRwShq3M);
int APS5_VABI sceNpCppWebApiUnknown_47JlRwShq3M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("48vx--WiBLU", sceNpCppWebApiUnknown_48vx_minus__minus_WiBLU);
int APS5_VABI sceNpCppWebApiUnknown_48vx_minus__minus_WiBLU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4C7We-mJFQI", sceNpCppWebApiUnknown_4C7We_minus_mJFQI);
int APS5_VABI sceNpCppWebApiUnknown_4C7We_minus_mJFQI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4DhGHHZbPhk", sceNpCppWebApiUnknown_4DhGHHZbPhk);
int APS5_VABI sceNpCppWebApiUnknown_4DhGHHZbPhk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4FR0WyuPqTo", sceNpCppWebApiUnknown_4FR0WyuPqTo);
int APS5_VABI sceNpCppWebApiUnknown_4FR0WyuPqTo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4GdlGMgWm2Y", sceNpCppWebApiUnknown_4GdlGMgWm2Y);
int APS5_VABI sceNpCppWebApiUnknown_4GdlGMgWm2Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4J5F23VgTjY", sceNpCppWebApiUnknown_4J5F23VgTjY);
int APS5_VABI sceNpCppWebApiUnknown_4J5F23VgTjY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4JGDOwffWGU", sceNpCppWebApiUnknown_4JGDOwffWGU);
int APS5_VABI sceNpCppWebApiUnknown_4JGDOwffWGU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4LFb2ce6BJI", sceNpCppWebApiUnknown_4LFb2ce6BJI);
int APS5_VABI sceNpCppWebApiUnknown_4LFb2ce6BJI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4MuLwBniwNY", sceNpCppWebApiUnknown_4MuLwBniwNY);
int APS5_VABI sceNpCppWebApiUnknown_4MuLwBniwNY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4Qn1ujWQLUQ", sceNpCppWebApiUnknown_4Qn1ujWQLUQ);
int APS5_VABI sceNpCppWebApiUnknown_4Qn1ujWQLUQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4RycqlFa1NQ", sceNpCppWebApiUnknown_4RycqlFa1NQ);
int APS5_VABI sceNpCppWebApiUnknown_4RycqlFa1NQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4TGeM5jD7x0", sceNpCppWebApiUnknown_4TGeM5jD7x0);
int APS5_VABI sceNpCppWebApiUnknown_4TGeM5jD7x0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4TiZy24yFoQ", sceNpCppWebApiUnknown_4TiZy24yFoQ);
int APS5_VABI sceNpCppWebApiUnknown_4TiZy24yFoQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4V0H+brwuCU", sceNpCppWebApiUnknown_4V0H_plus_brwuCU);
int APS5_VABI sceNpCppWebApiUnknown_4V0H_plus_brwuCU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4VRZ2RXGspA", sceNpCppWebApiUnknown_4VRZ2RXGspA);
int APS5_VABI sceNpCppWebApiUnknown_4VRZ2RXGspA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4VUDXmoiZeQ", sceNpCppWebApiUnknown_4VUDXmoiZeQ);
int APS5_VABI sceNpCppWebApiUnknown_4VUDXmoiZeQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4Wq-RRWM+q0", sceNpCppWebApiUnknown_4Wq_minus_RRWM_plus_q0);
int APS5_VABI sceNpCppWebApiUnknown_4Wq_minus_RRWM_plus_q0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4XF8LD0SLnQ", sceNpCppWebApiUnknown_4XF8LD0SLnQ);
int APS5_VABI sceNpCppWebApiUnknown_4XF8LD0SLnQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4XZd94Wa2Ak", sceNpCppWebApiUnknown_4XZd94Wa2Ak);
int APS5_VABI sceNpCppWebApiUnknown_4XZd94Wa2Ak(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4XkrdavSRUg", sceNpCppWebApiUnknown_4XkrdavSRUg);
int APS5_VABI sceNpCppWebApiUnknown_4XkrdavSRUg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4Y9-t1KCIn4", sceNpCppWebApiUnknown_4Y9_minus_t1KCIn4);
int APS5_VABI sceNpCppWebApiUnknown_4Y9_minus_t1KCIn4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4ZcisJGzxgM", sceNpCppWebApiUnknown_4ZcisJGzxgM);
int APS5_VABI sceNpCppWebApiUnknown_4ZcisJGzxgM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4afM3Xt0+Ig", sceNpCppWebApiUnknown_4afM3Xt0_plus_Ig);
int APS5_VABI sceNpCppWebApiUnknown_4afM3Xt0_plus_Ig(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4eahqnUQCls", sceNpCppWebApiUnknown_4eahqnUQCls);
int APS5_VABI sceNpCppWebApiUnknown_4eahqnUQCls(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4ephroQdYfA", sceNpCppWebApiUnknown_4ephroQdYfA);
int APS5_VABI sceNpCppWebApiUnknown_4ephroQdYfA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4g9GjN7SKoM", sceNpCppWebApiUnknown_4g9GjN7SKoM);
int APS5_VABI sceNpCppWebApiUnknown_4g9GjN7SKoM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4gJAjpddSVo", sceNpCppWebApiUnknown_4gJAjpddSVo);
int APS5_VABI sceNpCppWebApiUnknown_4gJAjpddSVo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4gN9AATFpTU", sceNpCppWebApiUnknown_4gN9AATFpTU);
int APS5_VABI sceNpCppWebApiUnknown_4gN9AATFpTU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4gg-uNDZg88", sceNpCppWebApiUnknown_4gg_minus_uNDZg88);
int APS5_VABI sceNpCppWebApiUnknown_4gg_minus_uNDZg88(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4hDGZkbevi0", sceNpCppWebApiUnknown_4hDGZkbevi0);
int APS5_VABI sceNpCppWebApiUnknown_4hDGZkbevi0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4ia3TbCbRVc", sceNpCppWebApiUnknown_4ia3TbCbRVc);
int APS5_VABI sceNpCppWebApiUnknown_4ia3TbCbRVc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4mBPHrRMft8", sceNpCppWebApiUnknown_4mBPHrRMft8);
int APS5_VABI sceNpCppWebApiUnknown_4mBPHrRMft8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4neldM1aEtI", sceNpCppWebApiUnknown_4neldM1aEtI);
int APS5_VABI sceNpCppWebApiUnknown_4neldM1aEtI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4qJE+JALDrA", sceNpCppWebApiUnknown_4qJE_plus_JALDrA);
int APS5_VABI sceNpCppWebApiUnknown_4qJE_plus_JALDrA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4qwDUAMxe4E", sceNpCppWebApiUnknown_4qwDUAMxe4E);
int APS5_VABI sceNpCppWebApiUnknown_4qwDUAMxe4E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4rpx-P+kdJQ", sceNpCppWebApiUnknown_4rpx_minus_P_plus_kdJQ);
int APS5_VABI sceNpCppWebApiUnknown_4rpx_minus_P_plus_kdJQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4tEx7tt+b7g", sceNpCppWebApiUnknown_4tEx7tt_plus_b7g);
int APS5_VABI sceNpCppWebApiUnknown_4tEx7tt_plus_b7g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4yhgTW7UB6c", sceNpCppWebApiUnknown_4yhgTW7UB6c);
int APS5_VABI sceNpCppWebApiUnknown_4yhgTW7UB6c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("4zrm6VrgIAw", sceNpCppWebApiUnknown_4zrm6VrgIAw);
int APS5_VABI sceNpCppWebApiUnknown_4zrm6VrgIAw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("50oxAQ8IzrQ", sceNpCppWebApiUnknown_50oxAQ8IzrQ);
int APS5_VABI sceNpCppWebApiUnknown_50oxAQ8IzrQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("52AlYvq+dmk", sceNpCppWebApiUnknown_52AlYvq_plus_dmk);
int APS5_VABI sceNpCppWebApiUnknown_52AlYvq_plus_dmk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("53DH+ZDnTP4", sceNpCppWebApiUnknown_53DH_plus_ZDnTP4);
int APS5_VABI sceNpCppWebApiUnknown_53DH_plus_ZDnTP4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("53eCdQ7MMEc", sceNpCppWebApiUnknown_53eCdQ7MMEc);
int APS5_VABI sceNpCppWebApiUnknown_53eCdQ7MMEc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("56eZPuFnFFo", sceNpCppWebApiUnknown_56eZPuFnFFo);
int APS5_VABI sceNpCppWebApiUnknown_56eZPuFnFFo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("57ST1U27ZxY", sceNpCppWebApiUnknown_57ST1U27ZxY);
int APS5_VABI sceNpCppWebApiUnknown_57ST1U27ZxY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("58HnTZbC0+k", sceNpCppWebApiUnknown_58HnTZbC0_plus_k);
int APS5_VABI sceNpCppWebApiUnknown_58HnTZbC0_plus_k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("59ZoWcKl5v8", sceNpCppWebApiUnknown_59ZoWcKl5v8);
int APS5_VABI sceNpCppWebApiUnknown_59ZoWcKl5v8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5A1JkOFCwqg", sceNpCppWebApiUnknown_5A1JkOFCwqg);
int APS5_VABI sceNpCppWebApiUnknown_5A1JkOFCwqg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5A1ghKVPits", sceNpCppWebApiUnknown_5A1ghKVPits);
int APS5_VABI sceNpCppWebApiUnknown_5A1ghKVPits(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5F7D2WLcblc", sceNpCppWebApiUnknown_5F7D2WLcblc);
int APS5_VABI sceNpCppWebApiUnknown_5F7D2WLcblc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5HZxpDKT1Xs", sceNpCppWebApiUnknown_5HZxpDKT1Xs);
int APS5_VABI sceNpCppWebApiUnknown_5HZxpDKT1Xs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5HeaBTx0AVA", sceNpCppWebApiUnknown_5HeaBTx0AVA);
int APS5_VABI sceNpCppWebApiUnknown_5HeaBTx0AVA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5IkBkh+MZYw", sceNpCppWebApiUnknown_5IkBkh_plus_MZYw);
int APS5_VABI sceNpCppWebApiUnknown_5IkBkh_plus_MZYw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5JmzZt8twAo", sceNpCppWebApiUnknown_5JmzZt8twAo);
int APS5_VABI sceNpCppWebApiUnknown_5JmzZt8twAo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5KRRH0MFHz4", sceNpCppWebApiUnknown_5KRRH0MFHz4);
int APS5_VABI sceNpCppWebApiUnknown_5KRRH0MFHz4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5LoVY9nQRTY", sceNpCppWebApiUnknown_5LoVY9nQRTY);
int APS5_VABI sceNpCppWebApiUnknown_5LoVY9nQRTY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5N6MwahrroM", sceNpCppWebApiUnknown_5N6MwahrroM);
int APS5_VABI sceNpCppWebApiUnknown_5N6MwahrroM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5OCoDxN-Cqc", sceNpCppWebApiUnknown_5OCoDxN_minus_Cqc);
int APS5_VABI sceNpCppWebApiUnknown_5OCoDxN_minus_Cqc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5QJRueHuTBo", sceNpCppWebApiUnknown_5QJRueHuTBo);
int APS5_VABI sceNpCppWebApiUnknown_5QJRueHuTBo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5RSKjWE2avA", sceNpCppWebApiUnknown_5RSKjWE2avA);
int APS5_VABI sceNpCppWebApiUnknown_5RSKjWE2avA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5SdJJgbOHZc", sceNpCppWebApiUnknown_5SdJJgbOHZc);
int APS5_VABI sceNpCppWebApiUnknown_5SdJJgbOHZc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5T5H6ER1lvs", sceNpCppWebApiUnknown_5T5H6ER1lvs);
int APS5_VABI sceNpCppWebApiUnknown_5T5H6ER1lvs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5Tgg-rL58Cs", sceNpCppWebApiUnknown_5Tgg_minus_rL58Cs);
int APS5_VABI sceNpCppWebApiUnknown_5Tgg_minus_rL58Cs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5TwSfklv4m8", sceNpCppWebApiUnknown_5TwSfklv4m8);
int APS5_VABI sceNpCppWebApiUnknown_5TwSfklv4m8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5UV12de1fGo", sceNpCppWebApiUnknown_5UV12de1fGo);
int APS5_VABI sceNpCppWebApiUnknown_5UV12de1fGo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5W-tQqvnH+8", sceNpCppWebApiUnknown_5W_minus_tQqvnH_plus_8);
int APS5_VABI sceNpCppWebApiUnknown_5W_minus_tQqvnH_plus_8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5XS0mKp8q9I", sceNpCppWebApiUnknown_5XS0mKp8q9I);
int APS5_VABI sceNpCppWebApiUnknown_5XS0mKp8q9I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5ZmHq5-n58M", sceNpCppWebApiUnknown_5ZmHq5_minus_n58M);
int APS5_VABI sceNpCppWebApiUnknown_5ZmHq5_minus_n58M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5avJQwMJgsY", sceNpCppWebApiUnknown_5avJQwMJgsY);
int APS5_VABI sceNpCppWebApiUnknown_5avJQwMJgsY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5cuWIWh72Wc", sceNpCppWebApiUnknown_5cuWIWh72Wc);
int APS5_VABI sceNpCppWebApiUnknown_5cuWIWh72Wc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5f9xDKxYjbY", sceNpCppWebApiUnknown_5f9xDKxYjbY);
int APS5_VABI sceNpCppWebApiUnknown_5f9xDKxYjbY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5p8LHGPi5oc", sceNpCppWebApiUnknown_5p8LHGPi5oc);
int APS5_VABI sceNpCppWebApiUnknown_5p8LHGPi5oc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5prlZSNAmwg", sceNpCppWebApiUnknown_5prlZSNAmwg);
int APS5_VABI sceNpCppWebApiUnknown_5prlZSNAmwg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5qBXmEPTZpI", sceNpCppWebApiUnknown_5qBXmEPTZpI);
int APS5_VABI sceNpCppWebApiUnknown_5qBXmEPTZpI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5s-tzz2VeoE", sceNpCppWebApiUnknown_5s_minus_tzz2VeoE);
int APS5_VABI sceNpCppWebApiUnknown_5s_minus_tzz2VeoE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5tW9QOKdw7g", sceNpCppWebApiUnknown_5tW9QOKdw7g);
int APS5_VABI sceNpCppWebApiUnknown_5tW9QOKdw7g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5vTnxovHQM4", sceNpCppWebApiUnknown_5vTnxovHQM4);
int APS5_VABI sceNpCppWebApiUnknown_5vTnxovHQM4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5wM1AxqBnvM", sceNpCppWebApiUnknown_5wM1AxqBnvM);
int APS5_VABI sceNpCppWebApiUnknown_5wM1AxqBnvM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("5yiOmhy5nQQ", sceNpCppWebApiUnknown_5yiOmhy5nQQ);
int APS5_VABI sceNpCppWebApiUnknown_5yiOmhy5nQQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6-RfgdMbM4k", sceNpCppWebApiUnknown_6_minus_RfgdMbM4k);
int APS5_VABI sceNpCppWebApiUnknown_6_minus_RfgdMbM4k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("60PxGiP12yY", sceNpCppWebApiUnknown_60PxGiP12yY);
int APS5_VABI sceNpCppWebApiUnknown_60PxGiP12yY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("61l7lw7GPpM", sceNpCppWebApiUnknown_61l7lw7GPpM);
int APS5_VABI sceNpCppWebApiUnknown_61l7lw7GPpM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("62r3MEB9wLs", sceNpCppWebApiUnknown_62r3MEB9wLs);
int APS5_VABI sceNpCppWebApiUnknown_62r3MEB9wLs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("63mPNKdUOZY", sceNpCppWebApiUnknown_63mPNKdUOZY);
int APS5_VABI sceNpCppWebApiUnknown_63mPNKdUOZY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("64d1Ql4ALKM", sceNpCppWebApiUnknown_64d1Ql4ALKM);
int APS5_VABI sceNpCppWebApiUnknown_64d1Ql4ALKM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("64t1tScUT7U", sceNpCppWebApiUnknown_64t1tScUT7U);
int APS5_VABI sceNpCppWebApiUnknown_64t1tScUT7U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("67UMDvxRnTI", sceNpCppWebApiUnknown_67UMDvxRnTI);
int APS5_VABI sceNpCppWebApiUnknown_67UMDvxRnTI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6DYUrB0WWL4", sceNpCppWebApiUnknown_6DYUrB0WWL4);
int APS5_VABI sceNpCppWebApiUnknown_6DYUrB0WWL4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6Es3CbUEwQE", sceNpCppWebApiUnknown_6Es3CbUEwQE);
int APS5_VABI sceNpCppWebApiUnknown_6Es3CbUEwQE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6FK4IOnANkc", sceNpCppWebApiUnknown_6FK4IOnANkc);
int APS5_VABI sceNpCppWebApiUnknown_6FK4IOnANkc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6G21TZ477uY", sceNpCppWebApiUnknown_6G21TZ477uY);
int APS5_VABI sceNpCppWebApiUnknown_6G21TZ477uY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6G5AbU2+roc", sceNpCppWebApiUnknown_6G5AbU2_plus_roc);
int APS5_VABI sceNpCppWebApiUnknown_6G5AbU2_plus_roc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6G5fpY0ODcA", sceNpCppWebApiUnknown_6G5fpY0ODcA);
int APS5_VABI sceNpCppWebApiUnknown_6G5fpY0ODcA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6Itto9pqaus", sceNpCppWebApiUnknown_6Itto9pqaus);
int APS5_VABI sceNpCppWebApiUnknown_6Itto9pqaus(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6K1g7+ztzAs", sceNpCppWebApiUnknown_6K1g7_plus_ztzAs);
int APS5_VABI sceNpCppWebApiUnknown_6K1g7_plus_ztzAs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6MORSoGtTEY", sceNpCppWebApiUnknown_6MORSoGtTEY);
int APS5_VABI sceNpCppWebApiUnknown_6MORSoGtTEY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6MqnXbffvVg", sceNpCppWebApiUnknown_6MqnXbffvVg);
int APS5_VABI sceNpCppWebApiUnknown_6MqnXbffvVg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6P8utWRG8zc", sceNpCppWebApiUnknown_6P8utWRG8zc);
int APS5_VABI sceNpCppWebApiUnknown_6P8utWRG8zc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6PJIccfp+Mw", sceNpCppWebApiUnknown_6PJIccfp_plus_Mw);
int APS5_VABI sceNpCppWebApiUnknown_6PJIccfp_plus_Mw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6QL0WMBsvbs", sceNpCppWebApiUnknown_6QL0WMBsvbs);
int APS5_VABI sceNpCppWebApiUnknown_6QL0WMBsvbs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6V1Fjm8zfe4", sceNpCppWebApiUnknown_6V1Fjm8zfe4);
int APS5_VABI sceNpCppWebApiUnknown_6V1Fjm8zfe4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6WU-DPPrc2E", sceNpCppWebApiUnknown_6WU_minus_DPPrc2E);
int APS5_VABI sceNpCppWebApiUnknown_6WU_minus_DPPrc2E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6Xv4OUcazNs", sceNpCppWebApiUnknown_6Xv4OUcazNs);
int APS5_VABI sceNpCppWebApiUnknown_6Xv4OUcazNs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6ZLWXIRBFuI", sceNpCppWebApiUnknown_6ZLWXIRBFuI);
int APS5_VABI sceNpCppWebApiUnknown_6ZLWXIRBFuI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6gBvZEOI9hM", sceNpCppWebApiUnknown_6gBvZEOI9hM);
int APS5_VABI sceNpCppWebApiUnknown_6gBvZEOI9hM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6jdNCEi6cFI", sceNpCppWebApiUnknown_6jdNCEi6cFI);
int APS5_VABI sceNpCppWebApiUnknown_6jdNCEi6cFI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6jztgmozVEI", sceNpCppWebApiUnknown_6jztgmozVEI);
int APS5_VABI sceNpCppWebApiUnknown_6jztgmozVEI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6kr44xgsuW0", sceNpCppWebApiUnknown_6kr44xgsuW0);
int APS5_VABI sceNpCppWebApiUnknown_6kr44xgsuW0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6lLDKa1S3EI", sceNpCppWebApiUnknown_6lLDKa1S3EI);
int APS5_VABI sceNpCppWebApiUnknown_6lLDKa1S3EI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6oL3-+pe6-I", sceNpCppWebApiUnknown_6oL3_minus__plus_pe6_minus_I);
int APS5_VABI sceNpCppWebApiUnknown_6oL3_minus__plus_pe6_minus_I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6q3HbnByPQ8", sceNpCppWebApiUnknown_6q3HbnByPQ8);
int APS5_VABI sceNpCppWebApiUnknown_6q3HbnByPQ8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6rbFd1xKksc", sceNpCppWebApiUnknown_6rbFd1xKksc);
int APS5_VABI sceNpCppWebApiUnknown_6rbFd1xKksc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6t2tqubghqY", sceNpCppWebApiUnknown_6t2tqubghqY);
int APS5_VABI sceNpCppWebApiUnknown_6t2tqubghqY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("6xfC1QjOWmk", sceNpCppWebApiUnknown_6xfC1QjOWmk);
int APS5_VABI sceNpCppWebApiUnknown_6xfC1QjOWmk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7+icaL29TYs", sceNpCppWebApiUnknown_7_plus_icaL29TYs);
int APS5_VABI sceNpCppWebApiUnknown_7_plus_icaL29TYs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7-AW9mVPxtI", sceNpCppWebApiUnknown_7_minus_AW9mVPxtI);
int APS5_VABI sceNpCppWebApiUnknown_7_minus_AW9mVPxtI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7-BX2Jeii7c", sceNpCppWebApiUnknown_7_minus_BX2Jeii7c);
int APS5_VABI sceNpCppWebApiUnknown_7_minus_BX2Jeii7c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("73I4BBiAx3c", sceNpCppWebApiUnknown_73I4BBiAx3c);
int APS5_VABI sceNpCppWebApiUnknown_73I4BBiAx3c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("73XG6dPCOr0", sceNpCppWebApiUnknown_73XG6dPCOr0);
int APS5_VABI sceNpCppWebApiUnknown_73XG6dPCOr0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("75Hbvij1fk8", sceNpCppWebApiUnknown_75Hbvij1fk8);
int APS5_VABI sceNpCppWebApiUnknown_75Hbvij1fk8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("76U8Y1XJ5CE", sceNpCppWebApiUnknown_76U8Y1XJ5CE);
int APS5_VABI sceNpCppWebApiUnknown_76U8Y1XJ5CE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7Af2XxmI-m8", sceNpCppWebApiUnknown_7Af2XxmI_minus_m8);
int APS5_VABI sceNpCppWebApiUnknown_7Af2XxmI_minus_m8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7Bc6AIhycYA", sceNpCppWebApiUnknown_7Bc6AIhycYA);
int APS5_VABI sceNpCppWebApiUnknown_7Bc6AIhycYA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7DhW72gDfsE", sceNpCppWebApiUnknown_7DhW72gDfsE);
int APS5_VABI sceNpCppWebApiUnknown_7DhW72gDfsE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7GiaU8HwHlE", sceNpCppWebApiUnknown_7GiaU8HwHlE);
int APS5_VABI sceNpCppWebApiUnknown_7GiaU8HwHlE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7IDdNe0C2tM", sceNpCppWebApiUnknown_7IDdNe0C2tM);
int APS5_VABI sceNpCppWebApiUnknown_7IDdNe0C2tM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7IgV8DBuzJQ", sceNpCppWebApiUnknown_7IgV8DBuzJQ);
int APS5_VABI sceNpCppWebApiUnknown_7IgV8DBuzJQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7IhJrsLLTcg", sceNpCppWebApiUnknown_7IhJrsLLTcg);
int APS5_VABI sceNpCppWebApiUnknown_7IhJrsLLTcg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7JWHhmrDvEU", sceNpCppWebApiUnknown_7JWHhmrDvEU);
int APS5_VABI sceNpCppWebApiUnknown_7JWHhmrDvEU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7JghICrSrmk", sceNpCppWebApiUnknown_7JghICrSrmk);
int APS5_VABI sceNpCppWebApiUnknown_7JghICrSrmk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7LaBdIC2qEU", sceNpCppWebApiUnknown_7LaBdIC2qEU);
int APS5_VABI sceNpCppWebApiUnknown_7LaBdIC2qEU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7Qq+PAPNTUM", sceNpCppWebApiUnknown_7Qq_plus_PAPNTUM);
int APS5_VABI sceNpCppWebApiUnknown_7Qq_plus_PAPNTUM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7VnK1K-gAEU", sceNpCppWebApiUnknown_7VnK1K_minus_gAEU);
int APS5_VABI sceNpCppWebApiUnknown_7VnK1K_minus_gAEU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7WIhb86+Kl4", sceNpCppWebApiUnknown_7WIhb86_plus_Kl4);
int APS5_VABI sceNpCppWebApiUnknown_7WIhb86_plus_Kl4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7X3wSywr7+0", sceNpCppWebApiUnknown_7X3wSywr7_plus_0);
int APS5_VABI sceNpCppWebApiUnknown_7X3wSywr7_plus_0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7dI1j4ItuHE", sceNpCppWebApiUnknown_7dI1j4ItuHE);
int APS5_VABI sceNpCppWebApiUnknown_7dI1j4ItuHE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7dqFi7Dvzgo", sceNpCppWebApiUnknown_7dqFi7Dvzgo);
int APS5_VABI sceNpCppWebApiUnknown_7dqFi7Dvzgo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7fXs6GxEVi4", sceNpCppWebApiUnknown_7fXs6GxEVi4);
int APS5_VABI sceNpCppWebApiUnknown_7fXs6GxEVi4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7iX8e9aZI3Y", sceNpCppWebApiUnknown_7iX8e9aZI3Y);
int APS5_VABI sceNpCppWebApiUnknown_7iX8e9aZI3Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7jbnFXytxmQ", sceNpCppWebApiUnknown_7jbnFXytxmQ);
int APS5_VABI sceNpCppWebApiUnknown_7jbnFXytxmQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7k8IDS3dnQA", sceNpCppWebApiUnknown_7k8IDS3dnQA);
int APS5_VABI sceNpCppWebApiUnknown_7k8IDS3dnQA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7kSGOVXdivc", sceNpCppWebApiUnknown_7kSGOVXdivc);
int APS5_VABI sceNpCppWebApiUnknown_7kSGOVXdivc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7ubnYQ7QZ1E", sceNpCppWebApiUnknown_7ubnYQ7QZ1E);
int APS5_VABI sceNpCppWebApiUnknown_7ubnYQ7QZ1E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7vDR+PWf33w", sceNpCppWebApiUnknown_7vDR_plus_PWf33w);
int APS5_VABI sceNpCppWebApiUnknown_7vDR_plus_PWf33w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7vtYVsbjXtM", sceNpCppWebApiUnknown_7vtYVsbjXtM);
int APS5_VABI sceNpCppWebApiUnknown_7vtYVsbjXtM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7wPqwjG-mPE", sceNpCppWebApiUnknown_7wPqwjG_minus_mPE);
int APS5_VABI sceNpCppWebApiUnknown_7wPqwjG_minus_mPE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7yXITS65REs", sceNpCppWebApiUnknown_7yXITS65REs);
int APS5_VABI sceNpCppWebApiUnknown_7yXITS65REs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("7yuDXvqIQF4", sceNpCppWebApiUnknown_7yuDXvqIQF4);
int APS5_VABI sceNpCppWebApiUnknown_7yuDXvqIQF4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("83kEPQNWy8A", sceNpCppWebApiUnknown_83kEPQNWy8A);
int APS5_VABI sceNpCppWebApiUnknown_83kEPQNWy8A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("86jNzbmb9Xk", sceNpCppWebApiUnknown_86jNzbmb9Xk);
int APS5_VABI sceNpCppWebApiUnknown_86jNzbmb9Xk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("88Rk5FV9QAs", sceNpCppWebApiUnknown_88Rk5FV9QAs);
int APS5_VABI sceNpCppWebApiUnknown_88Rk5FV9QAs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8BF0G0pf8+k", sceNpCppWebApiUnknown_8BF0G0pf8_plus_k);
int APS5_VABI sceNpCppWebApiUnknown_8BF0G0pf8_plus_k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8C36kDPJiS0", sceNpCppWebApiUnknown_8C36kDPJiS0);
int APS5_VABI sceNpCppWebApiUnknown_8C36kDPJiS0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8EuesH8mP8c", sceNpCppWebApiUnknown_8EuesH8mP8c);
int APS5_VABI sceNpCppWebApiUnknown_8EuesH8mP8c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8KfpQloQjDs", sceNpCppWebApiUnknown_8KfpQloQjDs);
int APS5_VABI sceNpCppWebApiUnknown_8KfpQloQjDs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8MExInxqXhE", sceNpCppWebApiUnknown_8MExInxqXhE);
int APS5_VABI sceNpCppWebApiUnknown_8MExInxqXhE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8MZi4-tr47E", sceNpCppWebApiUnknown_8MZi4_minus_tr47E);
int APS5_VABI sceNpCppWebApiUnknown_8MZi4_minus_tr47E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8MtIPCdWOGI", sceNpCppWebApiUnknown_8MtIPCdWOGI);
int APS5_VABI sceNpCppWebApiUnknown_8MtIPCdWOGI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8NNHrZe2Em4", sceNpCppWebApiUnknown_8NNHrZe2Em4);
int APS5_VABI sceNpCppWebApiUnknown_8NNHrZe2Em4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8NUB97h5hMg", sceNpCppWebApiUnknown_8NUB97h5hMg);
int APS5_VABI sceNpCppWebApiUnknown_8NUB97h5hMg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8O-6RHUTD7k", sceNpCppWebApiUnknown_8O_minus_6RHUTD7k);
int APS5_VABI sceNpCppWebApiUnknown_8O_minus_6RHUTD7k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8PoKVfl28n0", sceNpCppWebApiUnknown_8PoKVfl28n0);
int APS5_VABI sceNpCppWebApiUnknown_8PoKVfl28n0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8S10WsVXo5k", sceNpCppWebApiUnknown_8S10WsVXo5k);
int APS5_VABI sceNpCppWebApiUnknown_8S10WsVXo5k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8SHfQyTniLo", sceNpCppWebApiUnknown_8SHfQyTniLo);
int APS5_VABI sceNpCppWebApiUnknown_8SHfQyTniLo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8U-CFwUXaoA", sceNpCppWebApiUnknown_8U_minus_CFwUXaoA);
int APS5_VABI sceNpCppWebApiUnknown_8U_minus_CFwUXaoA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8VWyc8ke-uw", sceNpCppWebApiUnknown_8VWyc8ke_minus_uw);
int APS5_VABI sceNpCppWebApiUnknown_8VWyc8ke_minus_uw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8YtcWzYdIRs", sceNpCppWebApiUnknown_8YtcWzYdIRs);
int APS5_VABI sceNpCppWebApiUnknown_8YtcWzYdIRs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8ZNgnjz-3LQ", sceNpCppWebApiUnknown_8ZNgnjz_minus_3LQ);
int APS5_VABI sceNpCppWebApiUnknown_8ZNgnjz_minus_3LQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8bDoKOYvyrM", sceNpCppWebApiUnknown_8bDoKOYvyrM);
int APS5_VABI sceNpCppWebApiUnknown_8bDoKOYvyrM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8cmupXHZgAw", sceNpCppWebApiUnknown_8cmupXHZgAw);
int APS5_VABI sceNpCppWebApiUnknown_8cmupXHZgAw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8dWkWjo5EeE", sceNpCppWebApiUnknown_8dWkWjo5EeE);
int APS5_VABI sceNpCppWebApiUnknown_8dWkWjo5EeE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8eqBiqB72AE", sceNpCppWebApiUnknown_8eqBiqB72AE);
int APS5_VABI sceNpCppWebApiUnknown_8eqBiqB72AE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8fkLx5ejxY8", sceNpCppWebApiUnknown_8fkLx5ejxY8);
int APS5_VABI sceNpCppWebApiUnknown_8fkLx5ejxY8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8i5JWALfIoY", sceNpCppWebApiUnknown_8i5JWALfIoY);
int APS5_VABI sceNpCppWebApiUnknown_8i5JWALfIoY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8pgCKvHWKq4", sceNpCppWebApiUnknown_8pgCKvHWKq4);
int APS5_VABI sceNpCppWebApiUnknown_8pgCKvHWKq4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8qYmcJgmz70", sceNpCppWebApiUnknown_8qYmcJgmz70);
int APS5_VABI sceNpCppWebApiUnknown_8qYmcJgmz70(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8sRfYnV7XlA", sceNpCppWebApiUnknown_8sRfYnV7XlA);
int APS5_VABI sceNpCppWebApiUnknown_8sRfYnV7XlA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8vgoB2QpCq4", sceNpCppWebApiUnknown_8vgoB2QpCq4);
int APS5_VABI sceNpCppWebApiUnknown_8vgoB2QpCq4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8x++mBOUeso", sceNpCppWebApiUnknown_8x_plus__plus_mBOUeso);
int APS5_VABI sceNpCppWebApiUnknown_8x_plus__plus_mBOUeso(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("8xtAsAizlvE", sceNpCppWebApiUnknown_8xtAsAizlvE);
int APS5_VABI sceNpCppWebApiUnknown_8xtAsAizlvE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("96S8rlsntBU", sceNpCppWebApiUnknown_96S8rlsntBU);
int APS5_VABI sceNpCppWebApiUnknown_96S8rlsntBU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9AOMjk-ovyE", sceNpCppWebApiUnknown_9AOMjk_minus_ovyE);
int APS5_VABI sceNpCppWebApiUnknown_9AOMjk_minus_ovyE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9AuVWpMjbqc", sceNpCppWebApiUnknown_9AuVWpMjbqc);
int APS5_VABI sceNpCppWebApiUnknown_9AuVWpMjbqc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9G-fqSz4G8U", sceNpCppWebApiUnknown_9G_minus_fqSz4G8U);
int APS5_VABI sceNpCppWebApiUnknown_9G_minus_fqSz4G8U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9IygJEdu7Lg", sceNpCppWebApiUnknown_9IygJEdu7Lg);
int APS5_VABI sceNpCppWebApiUnknown_9IygJEdu7Lg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9KUZFjI1IxA", sceNpCppWebApiUnknown_9KUZFjI1IxA);
int APS5_VABI sceNpCppWebApiUnknown_9KUZFjI1IxA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9KX8MktrvHA", sceNpCppWebApiUnknown_9KX8MktrvHA);
int APS5_VABI sceNpCppWebApiUnknown_9KX8MktrvHA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9Km5hNg0azA", sceNpCppWebApiUnknown_9Km5hNg0azA);
int APS5_VABI sceNpCppWebApiUnknown_9Km5hNg0azA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9LOj0XIPK2A", sceNpCppWebApiUnknown_9LOj0XIPK2A);
int APS5_VABI sceNpCppWebApiUnknown_9LOj0XIPK2A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9OV-NYh2TAs", sceNpCppWebApiUnknown_9OV_minus_NYh2TAs);
int APS5_VABI sceNpCppWebApiUnknown_9OV_minus_NYh2TAs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9OXzqmMrdq4", sceNpCppWebApiUnknown_9OXzqmMrdq4);
int APS5_VABI sceNpCppWebApiUnknown_9OXzqmMrdq4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9PJfPHVOrVs", sceNpCppWebApiUnknown_9PJfPHVOrVs);
int APS5_VABI sceNpCppWebApiUnknown_9PJfPHVOrVs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9VDL1bE1nFM", sceNpCppWebApiUnknown_9VDL1bE1nFM);
int APS5_VABI sceNpCppWebApiUnknown_9VDL1bE1nFM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9VjSpYjnLBo", sceNpCppWebApiUnknown_9VjSpYjnLBo);
int APS5_VABI sceNpCppWebApiUnknown_9VjSpYjnLBo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9WSKZEhdVGE", sceNpCppWebApiUnknown_9WSKZEhdVGE);
int APS5_VABI sceNpCppWebApiUnknown_9WSKZEhdVGE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9XRswf+XVIk", sceNpCppWebApiUnknown_9XRswf_plus_XVIk);
int APS5_VABI sceNpCppWebApiUnknown_9XRswf_plus_XVIk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9YWXkd3AZAk", sceNpCppWebApiUnknown_9YWXkd3AZAk);
int APS5_VABI sceNpCppWebApiUnknown_9YWXkd3AZAk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9ZBtW0l-Uo0", sceNpCppWebApiUnknown_9ZBtW0l_minus_Uo0);
int APS5_VABI sceNpCppWebApiUnknown_9ZBtW0l_minus_Uo0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9Zu5MbzfzhY", sceNpCppWebApiUnknown_9Zu5MbzfzhY);
int APS5_VABI sceNpCppWebApiUnknown_9Zu5MbzfzhY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9aM5CHAbjdY", sceNpCppWebApiUnknown_9aM5CHAbjdY);
int APS5_VABI sceNpCppWebApiUnknown_9aM5CHAbjdY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9dRg+ERq5hI", sceNpCppWebApiUnknown_9dRg_plus_ERq5hI);
int APS5_VABI sceNpCppWebApiUnknown_9dRg_plus_ERq5hI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9euZEpiR6ek", sceNpCppWebApiUnknown_9euZEpiR6ek);
int APS5_VABI sceNpCppWebApiUnknown_9euZEpiR6ek(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9eywfTNJkrk", sceNpCppWebApiUnknown_9eywfTNJkrk);
int APS5_VABI sceNpCppWebApiUnknown_9eywfTNJkrk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9ikEdF-YHFo", sceNpCppWebApiUnknown_9ikEdF_minus_YHFo);
int APS5_VABI sceNpCppWebApiUnknown_9ikEdF_minus_YHFo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9lI2hrGpisE", sceNpCppWebApiUnknown_9lI2hrGpisE);
int APS5_VABI sceNpCppWebApiUnknown_9lI2hrGpisE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9m-AZF+kAyU", sceNpCppWebApiUnknown_9m_minus_AZF_plus_kAyU);
int APS5_VABI sceNpCppWebApiUnknown_9m_minus_AZF_plus_kAyU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9mb2J5lb7iM", sceNpCppWebApiUnknown_9mb2J5lb7iM);
int APS5_VABI sceNpCppWebApiUnknown_9mb2J5lb7iM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9p16KzQjIf4", sceNpCppWebApiUnknown_9p16KzQjIf4);
int APS5_VABI sceNpCppWebApiUnknown_9p16KzQjIf4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9pj40npxRRM", sceNpCppWebApiUnknown_9pj40npxRRM);
int APS5_VABI sceNpCppWebApiUnknown_9pj40npxRRM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9q5c19kl2ig", sceNpCppWebApiUnknown_9q5c19kl2ig);
int APS5_VABI sceNpCppWebApiUnknown_9q5c19kl2ig(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9r7dM3puxMk", sceNpCppWebApiUnknown_9r7dM3puxMk);
int APS5_VABI sceNpCppWebApiUnknown_9r7dM3puxMk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9s1E1w7ONEE", sceNpCppWebApiUnknown_9s1E1w7ONEE);
int APS5_VABI sceNpCppWebApiUnknown_9s1E1w7ONEE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9tF1UEXU2A4", sceNpCppWebApiUnknown_9tF1UEXU2A4);
int APS5_VABI sceNpCppWebApiUnknown_9tF1UEXU2A4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9uCGe-uPU+g", sceNpCppWebApiUnknown_9uCGe_minus_uPU_plus_g);
int APS5_VABI sceNpCppWebApiUnknown_9uCGe_minus_uPU_plus_g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9xuw6ZuXkC4", sceNpCppWebApiUnknown_9xuw6ZuXkC4);
int APS5_VABI sceNpCppWebApiUnknown_9xuw6ZuXkC4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("9y9YsMBbUq0", sceNpCppWebApiUnknown_9y9YsMBbUq0);
int APS5_VABI sceNpCppWebApiUnknown_9y9YsMBbUq0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("A+BSZ1YgF2o", sceNpCppWebApiUnknown_A_plus_BSZ1YgF2o);
int APS5_VABI sceNpCppWebApiUnknown_A_plus_BSZ1YgF2o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("A2p5aEz8Hoc", sceNpCppWebApiUnknown_A2p5aEz8Hoc);
int APS5_VABI sceNpCppWebApiUnknown_A2p5aEz8Hoc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("A3vS8-6YhBM", sceNpCppWebApiUnknown_A3vS8_minus_6YhBM);
int APS5_VABI sceNpCppWebApiUnknown_A3vS8_minus_6YhBM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("A6FC9-s8BTk", sceNpCppWebApiUnknown_A6FC9_minus_s8BTk);
int APS5_VABI sceNpCppWebApiUnknown_A6FC9_minus_s8BTk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("A6rq9qRjbzo", sceNpCppWebApiUnknown_A6rq9qRjbzo);
int APS5_VABI sceNpCppWebApiUnknown_A6rq9qRjbzo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("A8JdfjuehJM", sceNpCppWebApiUnknown_A8JdfjuehJM);
int APS5_VABI sceNpCppWebApiUnknown_A8JdfjuehJM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("A8LsCPXwRI4", sceNpCppWebApiUnknown_A8LsCPXwRI4);
int APS5_VABI sceNpCppWebApiUnknown_A8LsCPXwRI4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AAj9X+4aGYA", sceNpCppWebApiUnknown_AAj9X_plus_4aGYA);
int APS5_VABI sceNpCppWebApiUnknown_AAj9X_plus_4aGYA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AAxn2SfQwPw", sceNpCppWebApiUnknown_AAxn2SfQwPw);
int APS5_VABI sceNpCppWebApiUnknown_AAxn2SfQwPw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ANB19+6-3fg", sceNpCppWebApiUnknown_ANB19_plus_6_minus_3fg);
int APS5_VABI sceNpCppWebApiUnknown_ANB19_plus_6_minus_3fg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("APXMNs2+0I0", sceNpCppWebApiUnknown_APXMNs2_plus_0I0);
int APS5_VABI sceNpCppWebApiUnknown_APXMNs2_plus_0I0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ASXpsyCEU2A", sceNpCppWebApiUnknown_ASXpsyCEU2A);
int APS5_VABI sceNpCppWebApiUnknown_ASXpsyCEU2A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AUkgSifPWfE", sceNpCppWebApiUnknown_AUkgSifPWfE);
int APS5_VABI sceNpCppWebApiUnknown_AUkgSifPWfE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AUn539IKYCk", sceNpCppWebApiUnknown_AUn539IKYCk);
int APS5_VABI sceNpCppWebApiUnknown_AUn539IKYCk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AWF7N--deiQ", sceNpCppWebApiUnknown_AWF7N_minus__minus_deiQ);
int APS5_VABI sceNpCppWebApiUnknown_AWF7N_minus__minus_deiQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AXObhMCkmRs", sceNpCppWebApiUnknown_AXObhMCkmRs);
int APS5_VABI sceNpCppWebApiUnknown_AXObhMCkmRs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AZmSVK2KvL4", sceNpCppWebApiUnknown_AZmSVK2KvL4);
int APS5_VABI sceNpCppWebApiUnknown_AZmSVK2KvL4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AawHDpsapd4", sceNpCppWebApiUnknown_AawHDpsapd4);
int APS5_VABI sceNpCppWebApiUnknown_AawHDpsapd4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AcHirYSl0HA", sceNpCppWebApiUnknown_AcHirYSl0HA);
int APS5_VABI sceNpCppWebApiUnknown_AcHirYSl0HA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AcMT1Y+7ixU", sceNpCppWebApiUnknown_AcMT1Y_plus_7ixU);
int APS5_VABI sceNpCppWebApiUnknown_AcMT1Y_plus_7ixU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AehsN7SFbQA", sceNpCppWebApiUnknown_AehsN7SFbQA);
int APS5_VABI sceNpCppWebApiUnknown_AehsN7SFbQA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AfK1sH3UHkE", sceNpCppWebApiUnknown_AfK1sH3UHkE);
int APS5_VABI sceNpCppWebApiUnknown_AfK1sH3UHkE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AgDmpIIO0Co", sceNpCppWebApiUnknown_AgDmpIIO0Co);
int APS5_VABI sceNpCppWebApiUnknown_AgDmpIIO0Co(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AgbJ+Bkbp2U", sceNpCppWebApiUnknown_AgbJ_plus_Bkbp2U);
int APS5_VABI sceNpCppWebApiUnknown_AgbJ_plus_Bkbp2U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AgxXyvaaer0", sceNpCppWebApiUnknown_AgxXyvaaer0);
int APS5_VABI sceNpCppWebApiUnknown_AgxXyvaaer0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ahku9ydLj9w", sceNpCppWebApiUnknown_Ahku9ydLj9w);
int APS5_VABI sceNpCppWebApiUnknown_Ahku9ydLj9w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AhnlG38gTkk", sceNpCppWebApiUnknown_AhnlG38gTkk);
int APS5_VABI sceNpCppWebApiUnknown_AhnlG38gTkk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AkZYKwb26Fc", sceNpCppWebApiUnknown_AkZYKwb26Fc);
int APS5_VABI sceNpCppWebApiUnknown_AkZYKwb26Fc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AtvOgVdBLNA", sceNpCppWebApiUnknown_AtvOgVdBLNA);
int APS5_VABI sceNpCppWebApiUnknown_AtvOgVdBLNA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AuxqfHk3+D4", sceNpCppWebApiUnknown_AuxqfHk3_plus_D4);
int APS5_VABI sceNpCppWebApiUnknown_AuxqfHk3_plus_D4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AxAWyeoZW30", sceNpCppWebApiUnknown_AxAWyeoZW30);
int APS5_VABI sceNpCppWebApiUnknown_AxAWyeoZW30(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Az-pLnhSwJM", sceNpCppWebApiUnknown_Az_minus_pLnhSwJM);
int APS5_VABI sceNpCppWebApiUnknown_Az_minus_pLnhSwJM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("AzRzzMkPW-g", sceNpCppWebApiUnknown_AzRzzMkPW_minus_g);
int APS5_VABI sceNpCppWebApiUnknown_AzRzzMkPW_minus_g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("B0ABlx18Qbo", sceNpCppWebApiUnknown_B0ABlx18Qbo);
int APS5_VABI sceNpCppWebApiUnknown_B0ABlx18Qbo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("B12iWctD-b8", sceNpCppWebApiUnknown_B12iWctD_minus_b8);
int APS5_VABI sceNpCppWebApiUnknown_B12iWctD_minus_b8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("B1Qm4MdV7iQ", sceNpCppWebApiUnknown_B1Qm4MdV7iQ);
int APS5_VABI sceNpCppWebApiUnknown_B1Qm4MdV7iQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("B2omeKs6CsM", sceNpCppWebApiUnknown_B2omeKs6CsM);
int APS5_VABI sceNpCppWebApiUnknown_B2omeKs6CsM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("B6WC6bg4wbY", sceNpCppWebApiUnknown_B6WC6bg4wbY);
int APS5_VABI sceNpCppWebApiUnknown_B6WC6bg4wbY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("B6bE1qTByNU", sceNpCppWebApiUnknown_B6bE1qTByNU);
int APS5_VABI sceNpCppWebApiUnknown_B6bE1qTByNU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BBoN-+1fjTA", sceNpCppWebApiUnknown_BBoN_minus__plus_1fjTA);
int APS5_VABI sceNpCppWebApiUnknown_BBoN_minus__plus_1fjTA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BCATnoGUjrs", sceNpCppWebApiUnknown_BCATnoGUjrs);
int APS5_VABI sceNpCppWebApiUnknown_BCATnoGUjrs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BCUDW4v2m9E", sceNpCppWebApiUnknown_BCUDW4v2m9E);
int APS5_VABI sceNpCppWebApiUnknown_BCUDW4v2m9E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BCsyLvW91To", sceNpCppWebApiUnknown_BCsyLvW91To);
int APS5_VABI sceNpCppWebApiUnknown_BCsyLvW91To(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BEM9Ptmji9U", sceNpCppWebApiUnknown_BEM9Ptmji9U);
int APS5_VABI sceNpCppWebApiUnknown_BEM9Ptmji9U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BHvfIGdarBQ", sceNpCppWebApiUnknown_BHvfIGdarBQ);
int APS5_VABI sceNpCppWebApiUnknown_BHvfIGdarBQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BKEXfrDxpLY", sceNpCppWebApiUnknown_BKEXfrDxpLY);
int APS5_VABI sceNpCppWebApiUnknown_BKEXfrDxpLY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BLeUlxPwr5w", sceNpCppWebApiUnknown_BLeUlxPwr5w);
int APS5_VABI sceNpCppWebApiUnknown_BLeUlxPwr5w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BQ-8I4FcCQs", sceNpCppWebApiUnknown_BQ_minus_8I4FcCQs);
int APS5_VABI sceNpCppWebApiUnknown_BQ_minus_8I4FcCQs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BUP2bHKphL0", sceNpCppWebApiUnknown_BUP2bHKphL0);
int APS5_VABI sceNpCppWebApiUnknown_BUP2bHKphL0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BUkvLLlPGXU", sceNpCppWebApiUnknown_BUkvLLlPGXU);
int APS5_VABI sceNpCppWebApiUnknown_BUkvLLlPGXU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BWDILfRcisI", sceNpCppWebApiUnknown_BWDILfRcisI);
int APS5_VABI sceNpCppWebApiUnknown_BWDILfRcisI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BY1jyWLZrcA", sceNpCppWebApiUnknown_BY1jyWLZrcA);
int APS5_VABI sceNpCppWebApiUnknown_BY1jyWLZrcA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BYZVTUTk-DI", sceNpCppWebApiUnknown_BYZVTUTk_minus_DI);
int APS5_VABI sceNpCppWebApiUnknown_BYZVTUTk_minus_DI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Bbc2pDcwP0g", sceNpCppWebApiUnknown_Bbc2pDcwP0g);
int APS5_VABI sceNpCppWebApiUnknown_Bbc2pDcwP0g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BciGnJSuegU", sceNpCppWebApiUnknown_BciGnJSuegU);
int APS5_VABI sceNpCppWebApiUnknown_BciGnJSuegU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BcrC80L8i0Q", sceNpCppWebApiUnknown_BcrC80L8i0Q);
int APS5_VABI sceNpCppWebApiUnknown_BcrC80L8i0Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BetvGamJgGo", sceNpCppWebApiUnknown_BetvGamJgGo);
int APS5_VABI sceNpCppWebApiUnknown_BetvGamJgGo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BgDtc93upnM", sceNpCppWebApiUnknown_BgDtc93upnM);
int APS5_VABI sceNpCppWebApiUnknown_BgDtc93upnM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BgJDgr16f0k", sceNpCppWebApiUnknown_BgJDgr16f0k);
int APS5_VABI sceNpCppWebApiUnknown_BgJDgr16f0k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BlKPFkqR5+c", sceNpCppWebApiUnknown_BlKPFkqR5_plus_c);
int APS5_VABI sceNpCppWebApiUnknown_BlKPFkqR5_plus_c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BnQpaYneH60", sceNpCppWebApiUnknown_BnQpaYneH60);
int APS5_VABI sceNpCppWebApiUnknown_BnQpaYneH60(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BoakAg6TFe4", sceNpCppWebApiUnknown_BoakAg6TFe4);
int APS5_VABI sceNpCppWebApiUnknown_BoakAg6TFe4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BsE-m8JxIOg", sceNpCppWebApiUnknown_BsE_minus_m8JxIOg);
int APS5_VABI sceNpCppWebApiUnknown_BsE_minus_m8JxIOg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BxeiWYklF7I", sceNpCppWebApiUnknown_BxeiWYklF7I);
int APS5_VABI sceNpCppWebApiUnknown_BxeiWYklF7I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("BzPo4ufGQYc", sceNpCppWebApiUnknown_BzPo4ufGQYc);
int APS5_VABI sceNpCppWebApiUnknown_BzPo4ufGQYc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("C1-M65KwtjU", sceNpCppWebApiUnknown_C1_minus_M65KwtjU);
int APS5_VABI sceNpCppWebApiUnknown_C1_minus_M65KwtjU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("C5vg1mRnJ90", sceNpCppWebApiUnknown_C5vg1mRnJ90);
int APS5_VABI sceNpCppWebApiUnknown_C5vg1mRnJ90(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("C6skkzFAAGI", sceNpCppWebApiUnknown_C6skkzFAAGI);
int APS5_VABI sceNpCppWebApiUnknown_C6skkzFAAGI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("C7Joqb7LNII", sceNpCppWebApiUnknown_C7Joqb7LNII);
int APS5_VABI sceNpCppWebApiUnknown_C7Joqb7LNII(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("C7UDvuDYXaQ", sceNpCppWebApiUnknown_C7UDvuDYXaQ);
int APS5_VABI sceNpCppWebApiUnknown_C7UDvuDYXaQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("C7dvUSk3aRk", sceNpCppWebApiUnknown_C7dvUSk3aRk);
int APS5_VABI sceNpCppWebApiUnknown_C7dvUSk3aRk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("C8Ww8ZdVW1o", sceNpCppWebApiUnknown_C8Ww8ZdVW1o);
int APS5_VABI sceNpCppWebApiUnknown_C8Ww8ZdVW1o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CAM9NbC8Jus", sceNpCppWebApiUnknown_CAM9NbC8Jus);
int APS5_VABI sceNpCppWebApiUnknown_CAM9NbC8Jus(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CAeLxSO6Fzw", sceNpCppWebApiUnknown_CAeLxSO6Fzw);
int APS5_VABI sceNpCppWebApiUnknown_CAeLxSO6Fzw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CEWIEloY-0Q", sceNpCppWebApiUnknown_CEWIEloY_minus_0Q);
int APS5_VABI sceNpCppWebApiUnknown_CEWIEloY_minus_0Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CGxP3DGxYqs", sceNpCppWebApiUnknown_CGxP3DGxYqs);
int APS5_VABI sceNpCppWebApiUnknown_CGxP3DGxYqs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CMZjWpI9jMA", sceNpCppWebApiUnknown_CMZjWpI9jMA);
int APS5_VABI sceNpCppWebApiUnknown_CMZjWpI9jMA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CPprJmIqsvM", sceNpCppWebApiUnknown_CPprJmIqsvM);
int APS5_VABI sceNpCppWebApiUnknown_CPprJmIqsvM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CQ+fWei6Y1s", sceNpCppWebApiUnknown_CQ_plus_fWei6Y1s);
int APS5_VABI sceNpCppWebApiUnknown_CQ_plus_fWei6Y1s(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CQNMLobe25U", sceNpCppWebApiUnknown_CQNMLobe25U);
int APS5_VABI sceNpCppWebApiUnknown_CQNMLobe25U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CSjP0YyyBU0", sceNpCppWebApiUnknown_CSjP0YyyBU0);
int APS5_VABI sceNpCppWebApiUnknown_CSjP0YyyBU0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CWfrxh5cD90", sceNpCppWebApiUnknown_CWfrxh5cD90);
int APS5_VABI sceNpCppWebApiUnknown_CWfrxh5cD90(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CZbH3YP7JV4", sceNpCppWebApiUnknown_CZbH3YP7JV4);
int APS5_VABI sceNpCppWebApiUnknown_CZbH3YP7JV4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CaDhP62-xG0", sceNpCppWebApiUnknown_CaDhP62_minus_xG0);
int APS5_VABI sceNpCppWebApiUnknown_CaDhP62_minus_xG0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CaKfQZSlGbc", sceNpCppWebApiUnknown_CaKfQZSlGbc);
int APS5_VABI sceNpCppWebApiUnknown_CaKfQZSlGbc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Cb3IRwKPJPI", sceNpCppWebApiUnknown_Cb3IRwKPJPI);
int APS5_VABI sceNpCppWebApiUnknown_Cb3IRwKPJPI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CdUqp-LHLno", sceNpCppWebApiUnknown_CdUqp_minus_LHLno);
int APS5_VABI sceNpCppWebApiUnknown_CdUqp_minus_LHLno(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ce3mEsqP6jI", sceNpCppWebApiUnknown_Ce3mEsqP6jI);
int APS5_VABI sceNpCppWebApiUnknown_Ce3mEsqP6jI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CfkjC51cAZU", sceNpCppWebApiUnknown_CfkjC51cAZU);
int APS5_VABI sceNpCppWebApiUnknown_CfkjC51cAZU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CfoCiLo7xX8", sceNpCppWebApiUnknown_CfoCiLo7xX8);
int APS5_VABI sceNpCppWebApiUnknown_CfoCiLo7xX8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ChCtHSG7DPY", sceNpCppWebApiUnknown_ChCtHSG7DPY);
int APS5_VABI sceNpCppWebApiUnknown_ChCtHSG7DPY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ChFhSECNG40", sceNpCppWebApiUnknown_ChFhSECNG40);
int APS5_VABI sceNpCppWebApiUnknown_ChFhSECNG40(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Cmcn3RSODj4", sceNpCppWebApiUnknown_Cmcn3RSODj4);
int APS5_VABI sceNpCppWebApiUnknown_Cmcn3RSODj4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Cn5Shf4vRoc", sceNpCppWebApiUnknown_Cn5Shf4vRoc);
int APS5_VABI sceNpCppWebApiUnknown_Cn5Shf4vRoc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CqJuNXo5yiM", sceNpCppWebApiUnknown_CqJuNXo5yiM);
int APS5_VABI sceNpCppWebApiUnknown_CqJuNXo5yiM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CqRoIOhp7V4", sceNpCppWebApiUnknown_CqRoIOhp7V4);
int APS5_VABI sceNpCppWebApiUnknown_CqRoIOhp7V4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CtygVRCL+bA", sceNpCppWebApiUnknown_CtygVRCL_plus_bA);
int APS5_VABI sceNpCppWebApiUnknown_CtygVRCL_plus_bA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("CyDZEzEPRjA", sceNpCppWebApiUnknown_CyDZEzEPRjA);
int APS5_VABI sceNpCppWebApiUnknown_CyDZEzEPRjA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("D3tUMw-ZqcM", sceNpCppWebApiUnknown_D3tUMw_minus_ZqcM);
int APS5_VABI sceNpCppWebApiUnknown_D3tUMw_minus_ZqcM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("D5KQhCgzND8", sceNpCppWebApiUnknown_D5KQhCgzND8);
int APS5_VABI sceNpCppWebApiUnknown_D5KQhCgzND8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DDOy5lL-PfA", sceNpCppWebApiUnknown_DDOy5lL_minus_PfA);
int APS5_VABI sceNpCppWebApiUnknown_DDOy5lL_minus_PfA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DE2yor5gKVY", sceNpCppWebApiUnknown_DE2yor5gKVY);
int APS5_VABI sceNpCppWebApiUnknown_DE2yor5gKVY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DJxQnDALoL0", sceNpCppWebApiUnknown_DJxQnDALoL0);
int APS5_VABI sceNpCppWebApiUnknown_DJxQnDALoL0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DNmJw98YzUU", sceNpCppWebApiUnknown_DNmJw98YzUU);
int APS5_VABI sceNpCppWebApiUnknown_DNmJw98YzUU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DOUicDZmVk4", sceNpCppWebApiUnknown_DOUicDZmVk4);
int APS5_VABI sceNpCppWebApiUnknown_DOUicDZmVk4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DPemPzWoppg", sceNpCppWebApiUnknown_DPemPzWoppg);
int APS5_VABI sceNpCppWebApiUnknown_DPemPzWoppg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DQrMjSgCx1M", sceNpCppWebApiUnknown_DQrMjSgCx1M);
int APS5_VABI sceNpCppWebApiUnknown_DQrMjSgCx1M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DSb51YJKRYA", sceNpCppWebApiUnknown_DSb51YJKRYA);
int APS5_VABI sceNpCppWebApiUnknown_DSb51YJKRYA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DT39dZn0J+c", sceNpCppWebApiUnknown_DT39dZn0J_plus_c);
int APS5_VABI sceNpCppWebApiUnknown_DT39dZn0J_plus_c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DVq1Uvd+XxQ", sceNpCppWebApiUnknown_DVq1Uvd_plus_XxQ);
int APS5_VABI sceNpCppWebApiUnknown_DVq1Uvd_plus_XxQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DVqpN8zCZWo", sceNpCppWebApiUnknown_DVqpN8zCZWo);
int APS5_VABI sceNpCppWebApiUnknown_DVqpN8zCZWo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DWCwZfHeHQI", sceNpCppWebApiUnknown_DWCwZfHeHQI);
int APS5_VABI sceNpCppWebApiUnknown_DWCwZfHeHQI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DWN6mKqkOqU", sceNpCppWebApiUnknown_DWN6mKqkOqU);
int APS5_VABI sceNpCppWebApiUnknown_DWN6mKqkOqU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DXlC3IrK2Ts", sceNpCppWebApiUnknown_DXlC3IrK2Ts);
int APS5_VABI sceNpCppWebApiUnknown_DXlC3IrK2Ts(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DZ8Zj-5bC38", sceNpCppWebApiUnknown_DZ8Zj_minus_5bC38);
int APS5_VABI sceNpCppWebApiUnknown_DZ8Zj_minus_5bC38(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DZaCL+NWLBo", sceNpCppWebApiUnknown_DZaCL_plus_NWLBo);
int APS5_VABI sceNpCppWebApiUnknown_DZaCL_plus_NWLBo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DeFsyDHokeY", sceNpCppWebApiUnknown_DeFsyDHokeY);
int APS5_VABI sceNpCppWebApiUnknown_DeFsyDHokeY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DfSCDRA3EjY", sceNpCppWebApiUnknown_DfSCDRA3EjY);
int APS5_VABI sceNpCppWebApiUnknown_DfSCDRA3EjY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DfzMrc+9P1g", sceNpCppWebApiUnknown_DfzMrc_plus_9P1g);
int APS5_VABI sceNpCppWebApiUnknown_DfzMrc_plus_9P1g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DhOq7Ks23EA", sceNpCppWebApiUnknown_DhOq7Ks23EA);
int APS5_VABI sceNpCppWebApiUnknown_DhOq7Ks23EA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Dinz9rnAyzA", sceNpCppWebApiUnknown_Dinz9rnAyzA);
int APS5_VABI sceNpCppWebApiUnknown_Dinz9rnAyzA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DkYfIk955L4", sceNpCppWebApiUnknown_DkYfIk955L4);
int APS5_VABI sceNpCppWebApiUnknown_DkYfIk955L4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DlWmn2ZQuWY", sceNpCppWebApiUnknown_DlWmn2ZQuWY);
int APS5_VABI sceNpCppWebApiUnknown_DlWmn2ZQuWY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DrekXkm+oxg", sceNpCppWebApiUnknown_DrekXkm_plus_oxg);
int APS5_VABI sceNpCppWebApiUnknown_DrekXkm_plus_oxg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DsjBETa6xGg", sceNpCppWebApiUnknown_DsjBETa6xGg);
int APS5_VABI sceNpCppWebApiUnknown_DsjBETa6xGg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DsvJnhblA0I", sceNpCppWebApiUnknown_DsvJnhblA0I);
int APS5_VABI sceNpCppWebApiUnknown_DsvJnhblA0I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DuW5ZqZv-70", sceNpCppWebApiUnknown_DuW5ZqZv_minus_70);
int APS5_VABI sceNpCppWebApiUnknown_DuW5ZqZv_minus_70(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("DvnJjzxbnvc", sceNpCppWebApiUnknown_DvnJjzxbnvc);
int APS5_VABI sceNpCppWebApiUnknown_DvnJjzxbnvc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Dw+vAxY-WYE", sceNpCppWebApiUnknown_Dw_plus_vAxY_minus_WYE);
int APS5_VABI sceNpCppWebApiUnknown_Dw_plus_vAxY_minus_WYE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E+2HDjUAhf8", sceNpCppWebApiUnknown_E_plus_2HDjUAhf8);
int APS5_VABI sceNpCppWebApiUnknown_E_plus_2HDjUAhf8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E+AhHrpXb-8", sceNpCppWebApiUnknown_E_plus_AhHrpXb_minus_8);
int APS5_VABI sceNpCppWebApiUnknown_E_plus_AhHrpXb_minus_8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E+lI7Bs1zlw", sceNpCppWebApiUnknown_E_plus_lI7Bs1zlw);
int APS5_VABI sceNpCppWebApiUnknown_E_plus_lI7Bs1zlw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E0CTYxeKBnw", sceNpCppWebApiUnknown_E0CTYxeKBnw);
int APS5_VABI sceNpCppWebApiUnknown_E0CTYxeKBnw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E1TeIZhG7XA", sceNpCppWebApiUnknown_E1TeIZhG7XA);
int APS5_VABI sceNpCppWebApiUnknown_E1TeIZhG7XA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E2QjFehvp2Y", sceNpCppWebApiUnknown_E2QjFehvp2Y);
int APS5_VABI sceNpCppWebApiUnknown_E2QjFehvp2Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E4mMHoKVG-A", sceNpCppWebApiUnknown_E4mMHoKVG_minus_A);
int APS5_VABI sceNpCppWebApiUnknown_E4mMHoKVG_minus_A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E7ABorgo8J0", sceNpCppWebApiUnknown_E7ABorgo8J0);
int APS5_VABI sceNpCppWebApiUnknown_E7ABorgo8J0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E7xSoysJA2o", sceNpCppWebApiUnknown_E7xSoysJA2o);
int APS5_VABI sceNpCppWebApiUnknown_E7xSoysJA2o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E8BmtbByVKs", sceNpCppWebApiUnknown_E8BmtbByVKs);
int APS5_VABI sceNpCppWebApiUnknown_E8BmtbByVKs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("E8pPjlz943E", sceNpCppWebApiUnknown_E8pPjlz943E);
int APS5_VABI sceNpCppWebApiUnknown_E8pPjlz943E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EBjE6sitQCI", sceNpCppWebApiUnknown_EBjE6sitQCI);
int APS5_VABI sceNpCppWebApiUnknown_EBjE6sitQCI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EGYpmwU7ra4", sceNpCppWebApiUnknown_EGYpmwU7ra4);
int APS5_VABI sceNpCppWebApiUnknown_EGYpmwU7ra4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EHQEDVXZ0TI", sceNpCppWebApiUnknown_EHQEDVXZ0TI);
int APS5_VABI sceNpCppWebApiUnknown_EHQEDVXZ0TI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EIHpnU9SEKQ", sceNpCppWebApiUnknown_EIHpnU9SEKQ);
int APS5_VABI sceNpCppWebApiUnknown_EIHpnU9SEKQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ELZCcxSXaX8", sceNpCppWebApiUnknown_ELZCcxSXaX8);
int APS5_VABI sceNpCppWebApiUnknown_ELZCcxSXaX8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EOKBl05t-hk", sceNpCppWebApiUnknown_EOKBl05t_minus_hk);
int APS5_VABI sceNpCppWebApiUnknown_EOKBl05t_minus_hk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EPzYWmAKBJ0", sceNpCppWebApiUnknown_EPzYWmAKBJ0);
int APS5_VABI sceNpCppWebApiUnknown_EPzYWmAKBJ0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ERa-uKoAwd0", sceNpCppWebApiUnknown_ERa_minus_uKoAwd0);
int APS5_VABI sceNpCppWebApiUnknown_ERa_minus_uKoAwd0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ERuf9y0DY84", sceNpCppWebApiUnknown_ERuf9y0DY84);
int APS5_VABI sceNpCppWebApiUnknown_ERuf9y0DY84(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ESZ1wU1W4BA", sceNpCppWebApiUnknown_ESZ1wU1W4BA);
int APS5_VABI sceNpCppWebApiUnknown_ESZ1wU1W4BA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EYrJ5HrUGAs", sceNpCppWebApiUnknown_EYrJ5HrUGAs);
int APS5_VABI sceNpCppWebApiUnknown_EYrJ5HrUGAs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ea16yNLmh7w", sceNpCppWebApiUnknown_Ea16yNLmh7w);
int APS5_VABI sceNpCppWebApiUnknown_Ea16yNLmh7w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EaRDIeh-baM", sceNpCppWebApiUnknown_EaRDIeh_minus_baM);
int APS5_VABI sceNpCppWebApiUnknown_EaRDIeh_minus_baM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EbPVS06zVAo", sceNpCppWebApiUnknown_EbPVS06zVAo);
int APS5_VABI sceNpCppWebApiUnknown_EbPVS06zVAo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EcLWz4nKnY8", sceNpCppWebApiUnknown_EcLWz4nKnY8);
int APS5_VABI sceNpCppWebApiUnknown_EcLWz4nKnY8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EiMkgQsOfU0", sceNpCppWebApiUnknown_EiMkgQsOfU0);
int APS5_VABI sceNpCppWebApiUnknown_EiMkgQsOfU0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EnnlFAkLUKc", sceNpCppWebApiUnknown_EnnlFAkLUKc);
int APS5_VABI sceNpCppWebApiUnknown_EnnlFAkLUKc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ep1uD0m4qR0", sceNpCppWebApiUnknown_Ep1uD0m4qR0);
int APS5_VABI sceNpCppWebApiUnknown_Ep1uD0m4qR0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Er5RROJCXYo", sceNpCppWebApiUnknown_Er5RROJCXYo);
int APS5_VABI sceNpCppWebApiUnknown_Er5RROJCXYo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Erp13lgq9JM", sceNpCppWebApiUnknown_Erp13lgq9JM);
int APS5_VABI sceNpCppWebApiUnknown_Erp13lgq9JM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EtfRt5hez60", sceNpCppWebApiUnknown_EtfRt5hez60);
int APS5_VABI sceNpCppWebApiUnknown_EtfRt5hez60(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Eu95jmqn5Rw", sceNpCppWebApiUnknown_Eu95jmqn5Rw);
int APS5_VABI sceNpCppWebApiUnknown_Eu95jmqn5Rw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ev7SF7bb70o", sceNpCppWebApiUnknown_Ev7SF7bb70o);
int APS5_VABI sceNpCppWebApiUnknown_Ev7SF7bb70o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("EviQs3RBmoM", sceNpCppWebApiUnknown_EviQs3RBmoM);
int APS5_VABI sceNpCppWebApiUnknown_EviQs3RBmoM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Evq+NhTb3yg", sceNpCppWebApiUnknown_Evq_plus_NhTb3yg);
int APS5_VABI sceNpCppWebApiUnknown_Evq_plus_NhTb3yg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Eze5Qz4pO6A", sceNpCppWebApiUnknown_Eze5Qz4pO6A);
int APS5_VABI sceNpCppWebApiUnknown_Eze5Qz4pO6A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F-KitvVOrpQ", sceNpCppWebApiUnknown_F_minus_KitvVOrpQ);
int APS5_VABI sceNpCppWebApiUnknown_F_minus_KitvVOrpQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F-ejRKJlEiQ", sceNpCppWebApiUnknown_F_minus_ejRKJlEiQ);
int APS5_VABI sceNpCppWebApiUnknown_F_minus_ejRKJlEiQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F0X4ZzUC2FY", sceNpCppWebApiUnknown_F0X4ZzUC2FY);
int APS5_VABI sceNpCppWebApiUnknown_F0X4ZzUC2FY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F3Eg4srFrVw", sceNpCppWebApiUnknown_F3Eg4srFrVw);
int APS5_VABI sceNpCppWebApiUnknown_F3Eg4srFrVw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F4sJ-phip74", sceNpCppWebApiUnknown_F4sJ_minus_phip74);
int APS5_VABI sceNpCppWebApiUnknown_F4sJ_minus_phip74(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F5Up4uEenjU", sceNpCppWebApiUnknown_F5Up4uEenjU);
int APS5_VABI sceNpCppWebApiUnknown_F5Up4uEenjU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F5yaU6TEU8g", sceNpCppWebApiUnknown_F5yaU6TEU8g);
int APS5_VABI sceNpCppWebApiUnknown_F5yaU6TEU8g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F6cWxIx+kIQ", sceNpCppWebApiUnknown_F6cWxIx_plus_kIQ);
int APS5_VABI sceNpCppWebApiUnknown_F6cWxIx_plus_kIQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F6g64Yst1FQ", sceNpCppWebApiUnknown_F6g64Yst1FQ);
int APS5_VABI sceNpCppWebApiUnknown_F6g64Yst1FQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F8eFQFx6JhU", sceNpCppWebApiUnknown_F8eFQFx6JhU);
int APS5_VABI sceNpCppWebApiUnknown_F8eFQFx6JhU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("F9HtJ08qQIs", sceNpCppWebApiUnknown_F9HtJ08qQIs);
int APS5_VABI sceNpCppWebApiUnknown_F9HtJ08qQIs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FBePYJidCUU", sceNpCppWebApiUnknown_FBePYJidCUU);
int APS5_VABI sceNpCppWebApiUnknown_FBePYJidCUU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FCbkGMqZDbY", sceNpCppWebApiUnknown_FCbkGMqZDbY);
int APS5_VABI sceNpCppWebApiUnknown_FCbkGMqZDbY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FCcPee0M5uw", sceNpCppWebApiUnknown_FCcPee0M5uw);
int APS5_VABI sceNpCppWebApiUnknown_FCcPee0M5uw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FHfpa+Rs+cg", sceNpCppWebApiUnknown_FHfpa_plus_Rs_plus_cg);
int APS5_VABI sceNpCppWebApiUnknown_FHfpa_plus_Rs_plus_cg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FIIcqX1R3SQ", sceNpCppWebApiUnknown_FIIcqX1R3SQ);
int APS5_VABI sceNpCppWebApiUnknown_FIIcqX1R3SQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FKVhj3L1HF8", sceNpCppWebApiUnknown_FKVhj3L1HF8);
int APS5_VABI sceNpCppWebApiUnknown_FKVhj3L1HF8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FL3K2lvYq-E", sceNpCppWebApiUnknown_FL3K2lvYq_minus_E);
int APS5_VABI sceNpCppWebApiUnknown_FL3K2lvYq_minus_E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FQowxL1xbgc", sceNpCppWebApiUnknown_FQowxL1xbgc);
int APS5_VABI sceNpCppWebApiUnknown_FQowxL1xbgc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FTJxu+itNSs", sceNpCppWebApiUnknown_FTJxu_plus_itNSs);
int APS5_VABI sceNpCppWebApiUnknown_FTJxu_plus_itNSs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FUP09azNviM", sceNpCppWebApiUnknown_FUP09azNviM);
int APS5_VABI sceNpCppWebApiUnknown_FUP09azNviM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FWSsCcJfRzk", sceNpCppWebApiUnknown_FWSsCcJfRzk);
int APS5_VABI sceNpCppWebApiUnknown_FWSsCcJfRzk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FYWPelwNlow", sceNpCppWebApiUnknown_FYWPelwNlow);
int APS5_VABI sceNpCppWebApiUnknown_FYWPelwNlow(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FZXAWifD-Zk", sceNpCppWebApiUnknown_FZXAWifD_minus_Zk);
int APS5_VABI sceNpCppWebApiUnknown_FZXAWifD_minus_Zk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FaZCwvM2Cjo", sceNpCppWebApiUnknown_FaZCwvM2Cjo);
int APS5_VABI sceNpCppWebApiUnknown_FaZCwvM2Cjo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Far+0EDHNT4", sceNpCppWebApiUnknown_Far_plus_0EDHNT4);
int APS5_VABI sceNpCppWebApiUnknown_Far_plus_0EDHNT4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Fbhq5mnuB4I", sceNpCppWebApiUnknown_Fbhq5mnuB4I);
int APS5_VABI sceNpCppWebApiUnknown_Fbhq5mnuB4I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FigcgjQNk-Y", sceNpCppWebApiUnknown_FigcgjQNk_minus_Y);
int APS5_VABI sceNpCppWebApiUnknown_FigcgjQNk_minus_Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Fts2nXFk6F0", sceNpCppWebApiUnknown_Fts2nXFk6F0);
int APS5_VABI sceNpCppWebApiUnknown_Fts2nXFk6F0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FwKWIx0rgyY", sceNpCppWebApiUnknown_FwKWIx0rgyY);
int APS5_VABI sceNpCppWebApiUnknown_FwKWIx0rgyY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FySvwNKJp2U", sceNpCppWebApiUnknown_FySvwNKJp2U);
int APS5_VABI sceNpCppWebApiUnknown_FySvwNKJp2U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FypKAVi0Xhw", sceNpCppWebApiUnknown_FypKAVi0Xhw);
int APS5_VABI sceNpCppWebApiUnknown_FypKAVi0Xhw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FywKncPHxLc", sceNpCppWebApiUnknown_FywKncPHxLc);
int APS5_VABI sceNpCppWebApiUnknown_FywKncPHxLc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FzJljjQlYEg", sceNpCppWebApiUnknown_FzJljjQlYEg);
int APS5_VABI sceNpCppWebApiUnknown_FzJljjQlYEg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("FzaSoS-xZuI", sceNpCppWebApiUnknown_FzaSoS_minus_xZuI);
int APS5_VABI sceNpCppWebApiUnknown_FzaSoS_minus_xZuI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("G+BEH0hlDrQ", sceNpCppWebApiUnknown_G_plus_BEH0hlDrQ);
int APS5_VABI sceNpCppWebApiUnknown_G_plus_BEH0hlDrQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("G-rWYY8TfUY", sceNpCppWebApiUnknown_G_minus_rWYY8TfUY);
int APS5_VABI sceNpCppWebApiUnknown_G_minus_rWYY8TfUY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("G15ALic2GPE", sceNpCppWebApiUnknown_G15ALic2GPE);
int APS5_VABI sceNpCppWebApiUnknown_G15ALic2GPE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("G23Rtu5LBvI", sceNpCppWebApiUnknown_G23Rtu5LBvI);
int APS5_VABI sceNpCppWebApiUnknown_G23Rtu5LBvI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("G29jd4UdmU8", sceNpCppWebApiUnknown_G29jd4UdmU8);
int APS5_VABI sceNpCppWebApiUnknown_G29jd4UdmU8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("G3h-NDHnyW4", sceNpCppWebApiUnknown_G3h_minus_NDHnyW4);
int APS5_VABI sceNpCppWebApiUnknown_G3h_minus_NDHnyW4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("G46aDJx5Qx8", sceNpCppWebApiUnknown_G46aDJx5Qx8);
int APS5_VABI sceNpCppWebApiUnknown_G46aDJx5Qx8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("G47L4jUy648", sceNpCppWebApiUnknown_G47L4jUy648);
int APS5_VABI sceNpCppWebApiUnknown_G47L4jUy648(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("G4QJQ2hZwWQ", sceNpCppWebApiUnknown_G4QJQ2hZwWQ);
int APS5_VABI sceNpCppWebApiUnknown_G4QJQ2hZwWQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("G4WTZycq2nM", sceNpCppWebApiUnknown_G4WTZycq2nM);
int APS5_VABI sceNpCppWebApiUnknown_G4WTZycq2nM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GBILv-xY3eU", sceNpCppWebApiUnknown_GBILv_minus_xY3eU);
int APS5_VABI sceNpCppWebApiUnknown_GBILv_minus_xY3eU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GCqL5V0xULY", sceNpCppWebApiUnknown_GCqL5V0xULY);
int APS5_VABI sceNpCppWebApiUnknown_GCqL5V0xULY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GIBhqoeiiho", sceNpCppWebApiUnknown_GIBhqoeiiho);
int APS5_VABI sceNpCppWebApiUnknown_GIBhqoeiiho(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GIRZIwKcwS0", sceNpCppWebApiUnknown_GIRZIwKcwS0);
int APS5_VABI sceNpCppWebApiUnknown_GIRZIwKcwS0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GJu2OLJK5Eo", sceNpCppWebApiUnknown_GJu2OLJK5Eo);
int APS5_VABI sceNpCppWebApiUnknown_GJu2OLJK5Eo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GKHM54nGcrE", sceNpCppWebApiUnknown_GKHM54nGcrE);
int APS5_VABI sceNpCppWebApiUnknown_GKHM54nGcrE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GKT4kP85kaE", sceNpCppWebApiUnknown_GKT4kP85kaE);
int APS5_VABI sceNpCppWebApiUnknown_GKT4kP85kaE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GMSN1x4y+cs", sceNpCppWebApiUnknown_GMSN1x4y_plus_cs);
int APS5_VABI sceNpCppWebApiUnknown_GMSN1x4y_plus_cs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GN92gdelkN0", sceNpCppWebApiUnknown_GN92gdelkN0);
int APS5_VABI sceNpCppWebApiUnknown_GN92gdelkN0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GNJWLgaqjds", sceNpCppWebApiUnknown_GNJWLgaqjds);
int APS5_VABI sceNpCppWebApiUnknown_GNJWLgaqjds(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GOv-80iBOBg", sceNpCppWebApiUnknown_GOv_minus_80iBOBg);
int APS5_VABI sceNpCppWebApiUnknown_GOv_minus_80iBOBg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GPADomkQZRg", sceNpCppWebApiUnknown_GPADomkQZRg);
int APS5_VABI sceNpCppWebApiUnknown_GPADomkQZRg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GQLXi2KQaXk", sceNpCppWebApiUnknown_GQLXi2KQaXk);
int APS5_VABI sceNpCppWebApiUnknown_GQLXi2KQaXk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GQcdYACGMnM", sceNpCppWebApiUnknown_GQcdYACGMnM);
int APS5_VABI sceNpCppWebApiUnknown_GQcdYACGMnM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GR+C-qDzxRs", sceNpCppWebApiUnknown_GR_plus_C_minus_qDzxRs);
int APS5_VABI sceNpCppWebApiUnknown_GR_plus_C_minus_qDzxRs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GSdscWmFHSM", sceNpCppWebApiUnknown_GSdscWmFHSM);
int APS5_VABI sceNpCppWebApiUnknown_GSdscWmFHSM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GVEbyNroOEI", sceNpCppWebApiUnknown_GVEbyNroOEI);
int APS5_VABI sceNpCppWebApiUnknown_GVEbyNroOEI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GVNd-uYjLkE", sceNpCppWebApiUnknown_GVNd_minus_uYjLkE);
int APS5_VABI sceNpCppWebApiUnknown_GVNd_minus_uYjLkE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GX4DLMQ9488", sceNpCppWebApiUnknown_GX4DLMQ9488);
int APS5_VABI sceNpCppWebApiUnknown_GX4DLMQ9488(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GZxQA0G4NKY", sceNpCppWebApiUnknown_GZxQA0G4NKY);
int APS5_VABI sceNpCppWebApiUnknown_GZxQA0G4NKY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GaS8QZThdBQ", sceNpCppWebApiUnknown_GaS8QZThdBQ);
int APS5_VABI sceNpCppWebApiUnknown_GaS8QZThdBQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GbhE629vwg8", sceNpCppWebApiUnknown_GbhE629vwg8);
int APS5_VABI sceNpCppWebApiUnknown_GbhE629vwg8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Gc3f8YcDNrE", sceNpCppWebApiUnknown_Gc3f8YcDNrE);
int APS5_VABI sceNpCppWebApiUnknown_Gc3f8YcDNrE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GfGTD16X2pI", sceNpCppWebApiUnknown_GfGTD16X2pI);
int APS5_VABI sceNpCppWebApiUnknown_GfGTD16X2pI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GhX55L+EUH4", sceNpCppWebApiUnknown_GhX55L_plus_EUH4);
int APS5_VABI sceNpCppWebApiUnknown_GhX55L_plus_EUH4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GiBMpBpiYd0", sceNpCppWebApiUnknown_GiBMpBpiYd0);
int APS5_VABI sceNpCppWebApiUnknown_GiBMpBpiYd0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GiSQp3uJjac", sceNpCppWebApiUnknown_GiSQp3uJjac);
int APS5_VABI sceNpCppWebApiUnknown_GiSQp3uJjac(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GoMPhCyp74o", sceNpCppWebApiUnknown_GoMPhCyp74o);
int APS5_VABI sceNpCppWebApiUnknown_GoMPhCyp74o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GqF-5J6eSqg", sceNpCppWebApiUnknown_GqF_minus_5J6eSqg);
int APS5_VABI sceNpCppWebApiUnknown_GqF_minus_5J6eSqg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GwWn958m6g4", sceNpCppWebApiUnknown_GwWn958m6g4);
int APS5_VABI sceNpCppWebApiUnknown_GwWn958m6g4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GxDlxgJNRIg", sceNpCppWebApiUnknown_GxDlxgJNRIg);
int APS5_VABI sceNpCppWebApiUnknown_GxDlxgJNRIg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GxSTsMB0WyE", sceNpCppWebApiUnknown_GxSTsMB0WyE);
int APS5_VABI sceNpCppWebApiUnknown_GxSTsMB0WyE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("GzqrUd4brhA", sceNpCppWebApiUnknown_GzqrUd4brhA);
int APS5_VABI sceNpCppWebApiUnknown_GzqrUd4brhA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("H+8UBOwfScI", sceNpCppWebApiUnknown_H_plus_8UBOwfScI);
int APS5_VABI sceNpCppWebApiUnknown_H_plus_8UBOwfScI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("H0ZS0dFIzWU", sceNpCppWebApiUnknown_H0ZS0dFIzWU);
int APS5_VABI sceNpCppWebApiUnknown_H0ZS0dFIzWU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("H1HqUgxuDy0", sceNpCppWebApiUnknown_H1HqUgxuDy0);
int APS5_VABI sceNpCppWebApiUnknown_H1HqUgxuDy0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("H4vlWuQWtMk", sceNpCppWebApiUnknown_H4vlWuQWtMk);
int APS5_VABI sceNpCppWebApiUnknown_H4vlWuQWtMk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("H6q4N5ctWQ0", sceNpCppWebApiUnknown_H6q4N5ctWQ0);
int APS5_VABI sceNpCppWebApiUnknown_H6q4N5ctWQ0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("H7OgYdLtOB4", sceNpCppWebApiUnknown_H7OgYdLtOB4);
int APS5_VABI sceNpCppWebApiUnknown_H7OgYdLtOB4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("H9RbjogD+00", sceNpCppWebApiUnknown_H9RbjogD_plus_00);
int APS5_VABI sceNpCppWebApiUnknown_H9RbjogD_plus_00(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HCytjP1sjnU", sceNpCppWebApiUnknown_HCytjP1sjnU);
int APS5_VABI sceNpCppWebApiUnknown_HCytjP1sjnU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HDRkIEPgcMI", sceNpCppWebApiUnknown_HDRkIEPgcMI);
int APS5_VABI sceNpCppWebApiUnknown_HDRkIEPgcMI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HGQWVqwU2Fo", sceNpCppWebApiUnknown_HGQWVqwU2Fo);
int APS5_VABI sceNpCppWebApiUnknown_HGQWVqwU2Fo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HHLMFjjEbW4", sceNpCppWebApiUnknown_HHLMFjjEbW4);
int APS5_VABI sceNpCppWebApiUnknown_HHLMFjjEbW4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HI4KqGUlOUc", sceNpCppWebApiUnknown_HI4KqGUlOUc);
int APS5_VABI sceNpCppWebApiUnknown_HI4KqGUlOUc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HIkwgAuwsAo", sceNpCppWebApiUnknown_HIkwgAuwsAo);
int APS5_VABI sceNpCppWebApiUnknown_HIkwgAuwsAo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HJFAd2piPpA", sceNpCppWebApiUnknown_HJFAd2piPpA);
int APS5_VABI sceNpCppWebApiUnknown_HJFAd2piPpA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HKl8qYMHUxs", sceNpCppWebApiUnknown_HKl8qYMHUxs);
int APS5_VABI sceNpCppWebApiUnknown_HKl8qYMHUxs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HMEvP18jwH4", sceNpCppWebApiUnknown_HMEvP18jwH4);
int APS5_VABI sceNpCppWebApiUnknown_HMEvP18jwH4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HNMbgs32WLc", sceNpCppWebApiUnknown_HNMbgs32WLc);
int APS5_VABI sceNpCppWebApiUnknown_HNMbgs32WLc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HNqknqWLsHA", sceNpCppWebApiUnknown_HNqknqWLsHA);
int APS5_VABI sceNpCppWebApiUnknown_HNqknqWLsHA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HQ1+McfhhIM", sceNpCppWebApiUnknown_HQ1_plus_McfhhIM);
int APS5_VABI sceNpCppWebApiUnknown_HQ1_plus_McfhhIM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HSmw7UUvbs4", sceNpCppWebApiUnknown_HSmw7UUvbs4);
int APS5_VABI sceNpCppWebApiUnknown_HSmw7UUvbs4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HU-6QxP6Tz0", sceNpCppWebApiUnknown_HU_minus_6QxP6Tz0);
int APS5_VABI sceNpCppWebApiUnknown_HU_minus_6QxP6Tz0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HV7HZmJquy4", sceNpCppWebApiUnknown_HV7HZmJquy4);
int APS5_VABI sceNpCppWebApiUnknown_HV7HZmJquy4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HVwOHqFWcFc", sceNpCppWebApiUnknown_HVwOHqFWcFc);
int APS5_VABI sceNpCppWebApiUnknown_HVwOHqFWcFc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HYMJRbCn4Hg", sceNpCppWebApiUnknown_HYMJRbCn4Hg);
int APS5_VABI sceNpCppWebApiUnknown_HYMJRbCn4Hg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HZFv+Ur-OLw", sceNpCppWebApiUnknown_HZFv_plus_Ur_minus_OLw);
int APS5_VABI sceNpCppWebApiUnknown_HZFv_plus_Ur_minus_OLw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HfKZzf2Bgk0", sceNpCppWebApiUnknown_HfKZzf2Bgk0);
int APS5_VABI sceNpCppWebApiUnknown_HfKZzf2Bgk0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HfnTwwuLjAI", sceNpCppWebApiUnknown_HfnTwwuLjAI);
int APS5_VABI sceNpCppWebApiUnknown_HfnTwwuLjAI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HidVSBhMHPM", sceNpCppWebApiUnknown_HidVSBhMHPM);
int APS5_VABI sceNpCppWebApiUnknown_HidVSBhMHPM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HiyGzpGVU+8", sceNpCppWebApiUnknown_HiyGzpGVU_plus_8);
int APS5_VABI sceNpCppWebApiUnknown_HiyGzpGVU_plus_8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HjVPfi0omIY", sceNpCppWebApiUnknown_HjVPfi0omIY);
int APS5_VABI sceNpCppWebApiUnknown_HjVPfi0omIY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HlHb8LLob6o", sceNpCppWebApiUnknown_HlHb8LLob6o);
int APS5_VABI sceNpCppWebApiUnknown_HlHb8LLob6o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Hn-T0hnkwg8", sceNpCppWebApiUnknown_Hn_minus_T0hnkwg8);
int APS5_VABI sceNpCppWebApiUnknown_Hn_minus_T0hnkwg8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HoXiuWuma4I", sceNpCppWebApiUnknown_HoXiuWuma4I);
int APS5_VABI sceNpCppWebApiUnknown_HoXiuWuma4I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("HyplwWzKSjo", sceNpCppWebApiUnknown_HyplwWzKSjo);
int APS5_VABI sceNpCppWebApiUnknown_HyplwWzKSjo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("I-cP9AIOPqU", sceNpCppWebApiUnknown_I_minus_cP9AIOPqU);
int APS5_VABI sceNpCppWebApiUnknown_I_minus_cP9AIOPqU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("I0U0A7ZCYYo", sceNpCppWebApiUnknown_I0U0A7ZCYYo);
int APS5_VABI sceNpCppWebApiUnknown_I0U0A7ZCYYo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("I2QC8PYhJWY", sceNpCppWebApiUnknown_I2QC8PYhJWY);
int APS5_VABI sceNpCppWebApiUnknown_I2QC8PYhJWY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("I2g+qmRNJrM", sceNpCppWebApiUnknown_I2g_plus_qmRNJrM);
int APS5_VABI sceNpCppWebApiUnknown_I2g_plus_qmRNJrM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("I4Myxk+roNI", sceNpCppWebApiUnknown_I4Myxk_plus_roNI);
int APS5_VABI sceNpCppWebApiUnknown_I4Myxk_plus_roNI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("I5ECYW2vgfQ", sceNpCppWebApiUnknown_I5ECYW2vgfQ);
int APS5_VABI sceNpCppWebApiUnknown_I5ECYW2vgfQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("I6tYhuj7HMI", sceNpCppWebApiUnknown_I6tYhuj7HMI);
int APS5_VABI sceNpCppWebApiUnknown_I6tYhuj7HMI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("I8xwUdauOcY", sceNpCppWebApiUnknown_I8xwUdauOcY);
int APS5_VABI sceNpCppWebApiUnknown_I8xwUdauOcY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IAhVdJnk8FU", sceNpCppWebApiUnknown_IAhVdJnk8FU);
int APS5_VABI sceNpCppWebApiUnknown_IAhVdJnk8FU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IAkGC4cv3zs", sceNpCppWebApiUnknown_IAkGC4cv3zs);
int APS5_VABI sceNpCppWebApiUnknown_IAkGC4cv3zs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IApJAV73lSI", sceNpCppWebApiUnknown_IApJAV73lSI);
int APS5_VABI sceNpCppWebApiUnknown_IApJAV73lSI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IBHTBzd-VAg", sceNpCppWebApiUnknown_IBHTBzd_minus_VAg);
int APS5_VABI sceNpCppWebApiUnknown_IBHTBzd_minus_VAg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IBNzGcdOddw", sceNpCppWebApiUnknown_IBNzGcdOddw);
int APS5_VABI sceNpCppWebApiUnknown_IBNzGcdOddw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IBjqt0oLxeg", sceNpCppWebApiUnknown_IBjqt0oLxeg);
int APS5_VABI sceNpCppWebApiUnknown_IBjqt0oLxeg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IE4yA08WagA", sceNpCppWebApiUnknown_IE4yA08WagA);
int APS5_VABI sceNpCppWebApiUnknown_IE4yA08WagA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IJPH53bfhlo", sceNpCppWebApiUnknown_IJPH53bfhlo);
int APS5_VABI sceNpCppWebApiUnknown_IJPH53bfhlo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IL4RffHDATI", sceNpCppWebApiUnknown_IL4RffHDATI);
int APS5_VABI sceNpCppWebApiUnknown_IL4RffHDATI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("INgczqD0H+w", sceNpCppWebApiUnknown_INgczqD0H_plus_w);
int APS5_VABI sceNpCppWebApiUnknown_INgczqD0H_plus_w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IOzq33jNat8", sceNpCppWebApiUnknown_IOzq33jNat8);
int APS5_VABI sceNpCppWebApiUnknown_IOzq33jNat8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IPkyP6ig4sc", sceNpCppWebApiUnknown_IPkyP6ig4sc);
int APS5_VABI sceNpCppWebApiUnknown_IPkyP6ig4sc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IQOtjPPUJZM", sceNpCppWebApiUnknown_IQOtjPPUJZM);
int APS5_VABI sceNpCppWebApiUnknown_IQOtjPPUJZM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IRRGlO7Zjs8", sceNpCppWebApiUnknown_IRRGlO7Zjs8);
int APS5_VABI sceNpCppWebApiUnknown_IRRGlO7Zjs8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ISHK9QjN3R8", sceNpCppWebApiUnknown_ISHK9QjN3R8);
int APS5_VABI sceNpCppWebApiUnknown_ISHK9QjN3R8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IT4DyqEmct0", sceNpCppWebApiUnknown_IT4DyqEmct0);
int APS5_VABI sceNpCppWebApiUnknown_IT4DyqEmct0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IXW-z8pggfg", sceNpCppWebApiUnknown_IXW_minus_z8pggfg);
int APS5_VABI sceNpCppWebApiUnknown_IXW_minus_z8pggfg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IXXc85fjOzA", sceNpCppWebApiUnknown_IXXc85fjOzA);
int APS5_VABI sceNpCppWebApiUnknown_IXXc85fjOzA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IdPdE-du09Y", sceNpCppWebApiUnknown_IdPdE_minus_du09Y);
int APS5_VABI sceNpCppWebApiUnknown_IdPdE_minus_du09Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IezD5dF6NbQ", sceNpCppWebApiUnknown_IezD5dF6NbQ);
int APS5_VABI sceNpCppWebApiUnknown_IezD5dF6NbQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ih1gA1ynWjk", sceNpCppWebApiUnknown_Ih1gA1ynWjk);
int APS5_VABI sceNpCppWebApiUnknown_Ih1gA1ynWjk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IhRgrFc69cU", sceNpCppWebApiUnknown_IhRgrFc69cU);
int APS5_VABI sceNpCppWebApiUnknown_IhRgrFc69cU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ii7ADHLAYg4", sceNpCppWebApiUnknown_Ii7ADHLAYg4);
int APS5_VABI sceNpCppWebApiUnknown_Ii7ADHLAYg4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IjjVp9K7RsI", sceNpCppWebApiUnknown_IjjVp9K7RsI);
int APS5_VABI sceNpCppWebApiUnknown_IjjVp9K7RsI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ikt1f83OovA", sceNpCppWebApiUnknown_Ikt1f83OovA);
int APS5_VABI sceNpCppWebApiUnknown_Ikt1f83OovA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IlMqEkBLuBE", sceNpCppWebApiUnknown_IlMqEkBLuBE);
int APS5_VABI sceNpCppWebApiUnknown_IlMqEkBLuBE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IonqsnFkdJY", sceNpCppWebApiUnknown_IonqsnFkdJY);
int APS5_VABI sceNpCppWebApiUnknown_IonqsnFkdJY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IpCH3U9wFW0", sceNpCppWebApiUnknown_IpCH3U9wFW0);
int APS5_VABI sceNpCppWebApiUnknown_IpCH3U9wFW0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Is+nI7Hq9jU", sceNpCppWebApiUnknown_Is_plus_nI7Hq9jU);
int APS5_VABI sceNpCppWebApiUnknown_Is_plus_nI7Hq9jU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("IyUcglm6m4s", sceNpCppWebApiUnknown_IyUcglm6m4s);
int APS5_VABI sceNpCppWebApiUnknown_IyUcglm6m4s(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Iz0z3ATi4aE", sceNpCppWebApiUnknown_Iz0z3ATi4aE);
int APS5_VABI sceNpCppWebApiUnknown_Iz0z3ATi4aE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("J-9+igQy8us", sceNpCppWebApiUnknown_J_minus_9_plus_igQy8us);
int APS5_VABI sceNpCppWebApiUnknown_J_minus_9_plus_igQy8us(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("J-GJSI9lqgU", sceNpCppWebApiUnknown_J_minus_GJSI9lqgU);
int APS5_VABI sceNpCppWebApiUnknown_J_minus_GJSI9lqgU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("J192ZqTgPjM", sceNpCppWebApiUnknown_J192ZqTgPjM);
int APS5_VABI sceNpCppWebApiUnknown_J192ZqTgPjM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("J2CB-hl+VCY", sceNpCppWebApiUnknown_J2CB_minus_hl_plus_VCY);
int APS5_VABI sceNpCppWebApiUnknown_J2CB_minus_hl_plus_VCY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("J2Ve3PDUt2A", sceNpCppWebApiUnknown_J2Ve3PDUt2A);
int APS5_VABI sceNpCppWebApiUnknown_J2Ve3PDUt2A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("J3iduJTcpvQ", sceNpCppWebApiUnknown_J3iduJTcpvQ);
int APS5_VABI sceNpCppWebApiUnknown_J3iduJTcpvQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("J4vXbYykW8M", sceNpCppWebApiUnknown_J4vXbYykW8M);
int APS5_VABI sceNpCppWebApiUnknown_J4vXbYykW8M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("J7O30c5+anQ", sceNpCppWebApiUnknown_J7O30c5_plus_anQ);
int APS5_VABI sceNpCppWebApiUnknown_J7O30c5_plus_anQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("J9klOgvbE2A", sceNpCppWebApiUnknown_J9klOgvbE2A);
int APS5_VABI sceNpCppWebApiUnknown_J9klOgvbE2A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JAAxVx89vNc", sceNpCppWebApiUnknown_JAAxVx89vNc);
int APS5_VABI sceNpCppWebApiUnknown_JAAxVx89vNc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JDaQLsO6--U", sceNpCppWebApiUnknown_JDaQLsO6_minus__minus_U);
int APS5_VABI sceNpCppWebApiUnknown_JDaQLsO6_minus__minus_U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JFkvxXYRVS0", sceNpCppWebApiUnknown_JFkvxXYRVS0);
int APS5_VABI sceNpCppWebApiUnknown_JFkvxXYRVS0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JK6AmGTXYlI", sceNpCppWebApiUnknown_JK6AmGTXYlI);
int APS5_VABI sceNpCppWebApiUnknown_JK6AmGTXYlI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JL3q4exb1R4", sceNpCppWebApiUnknown_JL3q4exb1R4);
int APS5_VABI sceNpCppWebApiUnknown_JL3q4exb1R4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JP9y7f7e7QQ", sceNpCppWebApiUnknown_JP9y7f7e7QQ);
int APS5_VABI sceNpCppWebApiUnknown_JP9y7f7e7QQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JQ419YwNuc8", sceNpCppWebApiUnknown_JQ419YwNuc8);
int APS5_VABI sceNpCppWebApiUnknown_JQ419YwNuc8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JRPQayxpW9I", sceNpCppWebApiUnknown_JRPQayxpW9I);
int APS5_VABI sceNpCppWebApiUnknown_JRPQayxpW9I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JVV9P5rGzk8", sceNpCppWebApiUnknown_JVV9P5rGzk8);
int APS5_VABI sceNpCppWebApiUnknown_JVV9P5rGzk8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JYa01KuMqhA", sceNpCppWebApiUnknown_JYa01KuMqhA);
int APS5_VABI sceNpCppWebApiUnknown_JYa01KuMqhA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JYcDoHDeq-E", sceNpCppWebApiUnknown_JYcDoHDeq_minus_E);
int APS5_VABI sceNpCppWebApiUnknown_JYcDoHDeq_minus_E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JZeWjTYQwd4", sceNpCppWebApiUnknown_JZeWjTYQwd4);
int APS5_VABI sceNpCppWebApiUnknown_JZeWjTYQwd4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JZrd12xKQZw", sceNpCppWebApiUnknown_JZrd12xKQZw);
int APS5_VABI sceNpCppWebApiUnknown_JZrd12xKQZw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JaE6NtYASYE", sceNpCppWebApiUnknown_JaE6NtYASYE);
int APS5_VABI sceNpCppWebApiUnknown_JaE6NtYASYE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JakrtOQM6k4", sceNpCppWebApiUnknown_JakrtOQM6k4);
int APS5_VABI sceNpCppWebApiUnknown_JakrtOQM6k4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Je0-LmoRXYM", sceNpCppWebApiUnknown_Je0_minus_LmoRXYM);
int APS5_VABI sceNpCppWebApiUnknown_Je0_minus_LmoRXYM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JgLeJhw9Co8", sceNpCppWebApiUnknown_JgLeJhw9Co8);
int APS5_VABI sceNpCppWebApiUnknown_JgLeJhw9Co8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JhKjP9Krde0", sceNpCppWebApiUnknown_JhKjP9Krde0);
int APS5_VABI sceNpCppWebApiUnknown_JhKjP9Krde0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JiY4sfHkV34", sceNpCppWebApiUnknown_JiY4sfHkV34);
int APS5_VABI sceNpCppWebApiUnknown_JiY4sfHkV34(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Jlkdu--4aTg", sceNpCppWebApiUnknown_Jlkdu_minus__minus_4aTg);
int APS5_VABI sceNpCppWebApiUnknown_Jlkdu_minus__minus_4aTg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Jn4PaFyN-uE", sceNpCppWebApiUnknown_Jn4PaFyN_minus_uE);
int APS5_VABI sceNpCppWebApiUnknown_Jn4PaFyN_minus_uE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JnhpgrdCugM", sceNpCppWebApiUnknown_JnhpgrdCugM);
int APS5_VABI sceNpCppWebApiUnknown_JnhpgrdCugM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Jt9bJVyqe7A", sceNpCppWebApiUnknown_Jt9bJVyqe7A);
int APS5_VABI sceNpCppWebApiUnknown_Jt9bJVyqe7A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JuhregJdYZQ", sceNpCppWebApiUnknown_JuhregJdYZQ);
int APS5_VABI sceNpCppWebApiUnknown_JuhregJdYZQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Jv4JETvYzWg", sceNpCppWebApiUnknown_Jv4JETvYzWg);
int APS5_VABI sceNpCppWebApiUnknown_Jv4JETvYzWg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Jxct84SbIMg", sceNpCppWebApiUnknown_Jxct84SbIMg);
int APS5_VABI sceNpCppWebApiUnknown_Jxct84SbIMg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("JzqS+6C+2F4", sceNpCppWebApiUnknown_JzqS_plus_6C_plus_2F4);
int APS5_VABI sceNpCppWebApiUnknown_JzqS_plus_6C_plus_2F4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("K+AkNLmYoRo", sceNpCppWebApiUnknown_K_plus_AkNLmYoRo);
int APS5_VABI sceNpCppWebApiUnknown_K_plus_AkNLmYoRo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("K0knta-WVjs", sceNpCppWebApiUnknown_K0knta_minus_WVjs);
int APS5_VABI sceNpCppWebApiUnknown_K0knta_minus_WVjs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("K1c9NM1WP8Q", sceNpCppWebApiUnknown_K1c9NM1WP8Q);
int APS5_VABI sceNpCppWebApiUnknown_K1c9NM1WP8Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("K2Au2-6Agmw", sceNpCppWebApiUnknown_K2Au2_minus_6Agmw);
int APS5_VABI sceNpCppWebApiUnknown_K2Au2_minus_6Agmw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("K2C3Nc9TW5c", sceNpCppWebApiUnknown_K2C3Nc9TW5c);
int APS5_VABI sceNpCppWebApiUnknown_K2C3Nc9TW5c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("K5jUzaoyT60", sceNpCppWebApiUnknown_K5jUzaoyT60);
int APS5_VABI sceNpCppWebApiUnknown_K5jUzaoyT60(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("K7iNFNfeBN4", sceNpCppWebApiUnknown_K7iNFNfeBN4);
int APS5_VABI sceNpCppWebApiUnknown_K7iNFNfeBN4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("K81QQXNxZuk", sceNpCppWebApiUnknown_K81QQXNxZuk);
int APS5_VABI sceNpCppWebApiUnknown_K81QQXNxZuk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("K89tpB2jJ4g", sceNpCppWebApiUnknown_K89tpB2jJ4g);
int APS5_VABI sceNpCppWebApiUnknown_K89tpB2jJ4g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("K8zhm2vYK+o", sceNpCppWebApiUnknown_K8zhm2vYK_plus_o);
int APS5_VABI sceNpCppWebApiUnknown_K8zhm2vYK_plus_o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KD0NkSaIi1M", sceNpCppWebApiUnknown_KD0NkSaIi1M);
int APS5_VABI sceNpCppWebApiUnknown_KD0NkSaIi1M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KEDRSc45lqQ", sceNpCppWebApiUnknown_KEDRSc45lqQ);
int APS5_VABI sceNpCppWebApiUnknown_KEDRSc45lqQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KFXojgE-mpQ", sceNpCppWebApiUnknown_KFXojgE_minus_mpQ);
int APS5_VABI sceNpCppWebApiUnknown_KFXojgE_minus_mpQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KFwOdpIqVYU", sceNpCppWebApiUnknown_KFwOdpIqVYU);
int APS5_VABI sceNpCppWebApiUnknown_KFwOdpIqVYU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KHyHMSavpD0", sceNpCppWebApiUnknown_KHyHMSavpD0);
int APS5_VABI sceNpCppWebApiUnknown_KHyHMSavpD0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KJLvDqmFiKk", sceNpCppWebApiUnknown_KJLvDqmFiKk);
int APS5_VABI sceNpCppWebApiUnknown_KJLvDqmFiKk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KJTzMXmYY+U", sceNpCppWebApiUnknown_KJTzMXmYY_plus_U);
int APS5_VABI sceNpCppWebApiUnknown_KJTzMXmYY_plus_U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KJdPcOGmK58", sceNpCppWebApiUnknown_KJdPcOGmK58);
int APS5_VABI sceNpCppWebApiUnknown_KJdPcOGmK58(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KKAsA9YWAsM", sceNpCppWebApiUnknown_KKAsA9YWAsM);
int APS5_VABI sceNpCppWebApiUnknown_KKAsA9YWAsM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KNvvXG6k7dE", sceNpCppWebApiUnknown_KNvvXG6k7dE);
int APS5_VABI sceNpCppWebApiUnknown_KNvvXG6k7dE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KWEMMPMT56w", sceNpCppWebApiUnknown_KWEMMPMT56w);
int APS5_VABI sceNpCppWebApiUnknown_KWEMMPMT56w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KWwIUfLC-7k", sceNpCppWebApiUnknown_KWwIUfLC_minus_7k);
int APS5_VABI sceNpCppWebApiUnknown_KWwIUfLC_minus_7k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KXllpv1tR48", sceNpCppWebApiUnknown_KXllpv1tR48);
int APS5_VABI sceNpCppWebApiUnknown_KXllpv1tR48(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KZM7A2krDiM", sceNpCppWebApiUnknown_KZM7A2krDiM);
int APS5_VABI sceNpCppWebApiUnknown_KZM7A2krDiM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KadPb+xSU-8", sceNpCppWebApiUnknown_KadPb_plus_xSU_minus_8);
int APS5_VABI sceNpCppWebApiUnknown_KadPb_plus_xSU_minus_8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KanXmz-cpzE", sceNpCppWebApiUnknown_KanXmz_minus_cpzE);
int APS5_VABI sceNpCppWebApiUnknown_KanXmz_minus_cpzE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KhN0dL9tnTM", sceNpCppWebApiUnknown_KhN0dL9tnTM);
int APS5_VABI sceNpCppWebApiUnknown_KhN0dL9tnTM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ki0MvF8XNCk", sceNpCppWebApiUnknown_Ki0MvF8XNCk);
int APS5_VABI sceNpCppWebApiUnknown_Ki0MvF8XNCk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KitQxBdKz2M", sceNpCppWebApiUnknown_KitQxBdKz2M);
int APS5_VABI sceNpCppWebApiUnknown_KitQxBdKz2M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KlEYv40LnDY", sceNpCppWebApiUnknown_KlEYv40LnDY);
int APS5_VABI sceNpCppWebApiUnknown_KlEYv40LnDY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KnCMTTJWLdg", sceNpCppWebApiUnknown_KnCMTTJWLdg);
int APS5_VABI sceNpCppWebApiUnknown_KnCMTTJWLdg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KncijCmcEsk", sceNpCppWebApiUnknown_KncijCmcEsk);
int APS5_VABI sceNpCppWebApiUnknown_KncijCmcEsk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KpTVnt-DHo8", sceNpCppWebApiUnknown_KpTVnt_minus_DHo8);
int APS5_VABI sceNpCppWebApiUnknown_KpTVnt_minus_DHo8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Kvcr4zxrBSo", sceNpCppWebApiUnknown_Kvcr4zxrBSo);
int APS5_VABI sceNpCppWebApiUnknown_Kvcr4zxrBSo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("KxT0LzeguEc", sceNpCppWebApiUnknown_KxT0LzeguEc);
int APS5_VABI sceNpCppWebApiUnknown_KxT0LzeguEc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("L0aobPyYBRw", sceNpCppWebApiUnknown_L0aobPyYBRw);
int APS5_VABI sceNpCppWebApiUnknown_L0aobPyYBRw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("L1KAkYWml-M", sceNpCppWebApiUnknown_L1KAkYWml_minus_M);
int APS5_VABI sceNpCppWebApiUnknown_L1KAkYWml_minus_M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("L2P6Bg+kwG0", sceNpCppWebApiUnknown_L2P6Bg_plus_kwG0);
int APS5_VABI sceNpCppWebApiUnknown_L2P6Bg_plus_kwG0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("L3Nj0biFj4w", sceNpCppWebApiUnknown_L3Nj0biFj4w);
int APS5_VABI sceNpCppWebApiUnknown_L3Nj0biFj4w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("L6K9Z5jPLN0", sceNpCppWebApiUnknown_L6K9Z5jPLN0);
int APS5_VABI sceNpCppWebApiUnknown_L6K9Z5jPLN0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("L77IShz1gVM", sceNpCppWebApiUnknown_L77IShz1gVM);
int APS5_VABI sceNpCppWebApiUnknown_L77IShz1gVM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("L8RB0NbNLPI", sceNpCppWebApiUnknown_L8RB0NbNLPI);
int APS5_VABI sceNpCppWebApiUnknown_L8RB0NbNLPI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LAtHDO4YPVc", sceNpCppWebApiUnknown_LAtHDO4YPVc);
int APS5_VABI sceNpCppWebApiUnknown_LAtHDO4YPVc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LF70KOoRmM4", sceNpCppWebApiUnknown_LF70KOoRmM4);
int APS5_VABI sceNpCppWebApiUnknown_LF70KOoRmM4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LFCfPxGTF9w", sceNpCppWebApiUnknown_LFCfPxGTF9w);
int APS5_VABI sceNpCppWebApiUnknown_LFCfPxGTF9w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LFNbGrxObcY", sceNpCppWebApiUnknown_LFNbGrxObcY);
int APS5_VABI sceNpCppWebApiUnknown_LFNbGrxObcY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LGkB+6CseZk", sceNpCppWebApiUnknown_LGkB_plus_6CseZk);
int APS5_VABI sceNpCppWebApiUnknown_LGkB_plus_6CseZk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LKD-Mgz9RDY", sceNpCppWebApiUnknown_LKD_minus_Mgz9RDY);
int APS5_VABI sceNpCppWebApiUnknown_LKD_minus_Mgz9RDY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LMpRWV5XyrQ", sceNpCppWebApiUnknown_LMpRWV5XyrQ);
int APS5_VABI sceNpCppWebApiUnknown_LMpRWV5XyrQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LO70m1BiJPM", sceNpCppWebApiUnknown_LO70m1BiJPM);
int APS5_VABI sceNpCppWebApiUnknown_LO70m1BiJPM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LOPoNwieUwA", sceNpCppWebApiUnknown_LOPoNwieUwA);
int APS5_VABI sceNpCppWebApiUnknown_LOPoNwieUwA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LOoCMH7rHLE", sceNpCppWebApiUnknown_LOoCMH7rHLE);
int APS5_VABI sceNpCppWebApiUnknown_LOoCMH7rHLE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LP64B-0dbUs", sceNpCppWebApiUnknown_LP64B_minus_0dbUs);
int APS5_VABI sceNpCppWebApiUnknown_LP64B_minus_0dbUs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LQ3kaw4CTPw", sceNpCppWebApiUnknown_LQ3kaw4CTPw);
int APS5_VABI sceNpCppWebApiUnknown_LQ3kaw4CTPw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LQvamxROa-U", sceNpCppWebApiUnknown_LQvamxROa_minus_U);
int APS5_VABI sceNpCppWebApiUnknown_LQvamxROa_minus_U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LRP+Y9NHGfw", sceNpCppWebApiUnknown_LRP_plus_Y9NHGfw);
int APS5_VABI sceNpCppWebApiUnknown_LRP_plus_Y9NHGfw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LTXWL-kuxGM", sceNpCppWebApiUnknown_LTXWL_minus_kuxGM);
int APS5_VABI sceNpCppWebApiUnknown_LTXWL_minus_kuxGM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LTov9gMEqCU", sceNpCppWebApiUnknown_LTov9gMEqCU);
int APS5_VABI sceNpCppWebApiUnknown_LTov9gMEqCU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LUvjGNEPg2Y", sceNpCppWebApiUnknown_LUvjGNEPg2Y);
int APS5_VABI sceNpCppWebApiUnknown_LUvjGNEPg2Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LWjH6SZSrtg", sceNpCppWebApiUnknown_LWjH6SZSrtg);
int APS5_VABI sceNpCppWebApiUnknown_LWjH6SZSrtg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LXo-OIaiUIo", sceNpCppWebApiUnknown_LXo_minus_OIaiUIo);
int APS5_VABI sceNpCppWebApiUnknown_LXo_minus_OIaiUIo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LY-D6umDD70", sceNpCppWebApiUnknown_LY_minus_D6umDD70);
int APS5_VABI sceNpCppWebApiUnknown_LY_minus_D6umDD70(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LYn6kxpm6SY", sceNpCppWebApiUnknown_LYn6kxpm6SY);
int APS5_VABI sceNpCppWebApiUnknown_LYn6kxpm6SY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LbfXIdP82pI", sceNpCppWebApiUnknown_LbfXIdP82pI);
int APS5_VABI sceNpCppWebApiUnknown_LbfXIdP82pI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LbhH9NeB93I", sceNpCppWebApiUnknown_LbhH9NeB93I);
int APS5_VABI sceNpCppWebApiUnknown_LbhH9NeB93I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Le0QS6QTxds", sceNpCppWebApiUnknown_Le0QS6QTxds);
int APS5_VABI sceNpCppWebApiUnknown_Le0QS6QTxds(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LeqII4J8VMQ", sceNpCppWebApiUnknown_LeqII4J8VMQ);
int APS5_VABI sceNpCppWebApiUnknown_LeqII4J8VMQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LkZ-XYvmwqE", sceNpCppWebApiUnknown_LkZ_minus_XYvmwqE);
int APS5_VABI sceNpCppWebApiUnknown_LkZ_minus_XYvmwqE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Lkx5W5Yh+4s", sceNpCppWebApiUnknown_Lkx5W5Yh_plus_4s);
int APS5_VABI sceNpCppWebApiUnknown_Lkx5W5Yh_plus_4s(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Llx16-4OS+0", sceNpCppWebApiUnknown_Llx16_minus_4OS_plus_0);
int APS5_VABI sceNpCppWebApiUnknown_Llx16_minus_4OS_plus_0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LoAw3zk58OM", sceNpCppWebApiUnknown_LoAw3zk58OM);
int APS5_VABI sceNpCppWebApiUnknown_LoAw3zk58OM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Lohpoj0CgRU", sceNpCppWebApiUnknown_Lohpoj0CgRU);
int APS5_VABI sceNpCppWebApiUnknown_Lohpoj0CgRU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LotMMhPqWp0", sceNpCppWebApiUnknown_LotMMhPqWp0);
int APS5_VABI sceNpCppWebApiUnknown_LotMMhPqWp0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LpAj7UOsLsE", sceNpCppWebApiUnknown_LpAj7UOsLsE);
int APS5_VABI sceNpCppWebApiUnknown_LpAj7UOsLsE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LsUk5LsyhDs", sceNpCppWebApiUnknown_LsUk5LsyhDs);
int APS5_VABI sceNpCppWebApiUnknown_LsUk5LsyhDs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Lt-pvpkUkvQ", sceNpCppWebApiUnknown_Lt_minus_pvpkUkvQ);
int APS5_VABI sceNpCppWebApiUnknown_Lt_minus_pvpkUkvQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Lw3y-bVEGLA", sceNpCppWebApiUnknown_Lw3y_minus_bVEGLA);
int APS5_VABI sceNpCppWebApiUnknown_Lw3y_minus_bVEGLA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LwGrNk25QbE", sceNpCppWebApiUnknown_LwGrNk25QbE);
int APS5_VABI sceNpCppWebApiUnknown_LwGrNk25QbE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("LxkJXQ1xY0M", sceNpCppWebApiUnknown_LxkJXQ1xY0M);
int APS5_VABI sceNpCppWebApiUnknown_LxkJXQ1xY0M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Lz4PitW6VhE", sceNpCppWebApiUnknown_Lz4PitW6VhE);
int APS5_VABI sceNpCppWebApiUnknown_Lz4PitW6VhE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("M-j+UAPQzEQ", sceNpCppWebApiUnknown_M_minus_j_plus_UAPQzEQ);
int APS5_VABI sceNpCppWebApiUnknown_M_minus_j_plus_UAPQzEQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("M3Qe4kEI9Sc", sceNpCppWebApiUnknown_M3Qe4kEI9Sc);
int APS5_VABI sceNpCppWebApiUnknown_M3Qe4kEI9Sc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("M3wFXbYQtAA", sceNpCppWebApiUnknown_M3wFXbYQtAA);
int APS5_VABI sceNpCppWebApiUnknown_M3wFXbYQtAA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("M5s0JTah4qM", sceNpCppWebApiUnknown_M5s0JTah4qM);
int APS5_VABI sceNpCppWebApiUnknown_M5s0JTah4qM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MA1voKbveBw", sceNpCppWebApiUnknown_MA1voKbveBw);
int APS5_VABI sceNpCppWebApiUnknown_MA1voKbveBw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MCAsOwSWmdk", sceNpCppWebApiUnknown_MCAsOwSWmdk);
int APS5_VABI sceNpCppWebApiUnknown_MCAsOwSWmdk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MDS4M264Evk", sceNpCppWebApiUnknown_MDS4M264Evk);
int APS5_VABI sceNpCppWebApiUnknown_MDS4M264Evk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MHVO5QnXNWM", sceNpCppWebApiUnknown_MHVO5QnXNWM);
int APS5_VABI sceNpCppWebApiUnknown_MHVO5QnXNWM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MMz3pWjeeqA", sceNpCppWebApiUnknown_MMz3pWjeeqA);
int APS5_VABI sceNpCppWebApiUnknown_MMz3pWjeeqA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MObuWNuwKTo", sceNpCppWebApiUnknown_MObuWNuwKTo);
int APS5_VABI sceNpCppWebApiUnknown_MObuWNuwKTo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MPQDL+np6mI", sceNpCppWebApiUnknown_MPQDL_plus_np6mI);
int APS5_VABI sceNpCppWebApiUnknown_MPQDL_plus_np6mI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MRwiBFUbp1A", sceNpCppWebApiUnknown_MRwiBFUbp1A);
int APS5_VABI sceNpCppWebApiUnknown_MRwiBFUbp1A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MSJw0mUj+lU", sceNpCppWebApiUnknown_MSJw0mUj_plus_lU);
int APS5_VABI sceNpCppWebApiUnknown_MSJw0mUj_plus_lU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MSxsktnjRgA", sceNpCppWebApiUnknown_MSxsktnjRgA);
int APS5_VABI sceNpCppWebApiUnknown_MSxsktnjRgA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MVExpBYTEow", sceNpCppWebApiUnknown_MVExpBYTEow);
int APS5_VABI sceNpCppWebApiUnknown_MVExpBYTEow(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MXOk+ACfs0I", sceNpCppWebApiUnknown_MXOk_plus_ACfs0I);
int APS5_VABI sceNpCppWebApiUnknown_MXOk_plus_ACfs0I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MXakKx9ktts", sceNpCppWebApiUnknown_MXakKx9ktts);
int APS5_VABI sceNpCppWebApiUnknown_MXakKx9ktts(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MYhbfW1IWEM", sceNpCppWebApiUnknown_MYhbfW1IWEM);
int APS5_VABI sceNpCppWebApiUnknown_MYhbfW1IWEM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ma7+aWJsXow", sceNpCppWebApiUnknown_Ma7_plus_aWJsXow);
int APS5_VABI sceNpCppWebApiUnknown_Ma7_plus_aWJsXow(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MbWCMf0K0CM", sceNpCppWebApiUnknown_MbWCMf0K0CM);
int APS5_VABI sceNpCppWebApiUnknown_MbWCMf0K0CM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("McaGKOZBuHw", sceNpCppWebApiUnknown_McaGKOZBuHw);
int APS5_VABI sceNpCppWebApiUnknown_McaGKOZBuHw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MgkNIcmCFyI", sceNpCppWebApiUnknown_MgkNIcmCFyI);
int APS5_VABI sceNpCppWebApiUnknown_MgkNIcmCFyI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MkGUMQt7MBE", sceNpCppWebApiUnknown_MkGUMQt7MBE);
int APS5_VABI sceNpCppWebApiUnknown_MkGUMQt7MBE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MkSmI8WCSvM", sceNpCppWebApiUnknown_MkSmI8WCSvM);
int APS5_VABI sceNpCppWebApiUnknown_MkSmI8WCSvM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("MnNXyyktogo", sceNpCppWebApiUnknown_MnNXyyktogo);
int APS5_VABI sceNpCppWebApiUnknown_MnNXyyktogo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Mnc3Os+mgYM", sceNpCppWebApiUnknown_Mnc3Os_plus_mgYM);
int APS5_VABI sceNpCppWebApiUnknown_Mnc3Os_plus_mgYM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Mnp3qDir4NU", sceNpCppWebApiUnknown_Mnp3qDir4NU);
int APS5_VABI sceNpCppWebApiUnknown_Mnp3qDir4NU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Mp+8pHkZWkc", sceNpCppWebApiUnknown_Mp_plus_8pHkZWkc);
int APS5_VABI sceNpCppWebApiUnknown_Mp_plus_8pHkZWkc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Myp3FtCMlpU", sceNpCppWebApiUnknown_Myp3FtCMlpU);
int APS5_VABI sceNpCppWebApiUnknown_Myp3FtCMlpU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("N+uuxyqdvFo", sceNpCppWebApiUnknown_N_plus_uuxyqdvFo);
int APS5_VABI sceNpCppWebApiUnknown_N_plus_uuxyqdvFo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("N0V2-p-0hAM", sceNpCppWebApiUnknown_N0V2_minus_p_minus_0hAM);
int APS5_VABI sceNpCppWebApiUnknown_N0V2_minus_p_minus_0hAM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("N1VZUoOe-sc", sceNpCppWebApiUnknown_N1VZUoOe_minus_sc);
int APS5_VABI sceNpCppWebApiUnknown_N1VZUoOe_minus_sc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("N2xzUyENlCU", sceNpCppWebApiUnknown_N2xzUyENlCU);
int APS5_VABI sceNpCppWebApiUnknown_N2xzUyENlCU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("N78k-Vryooc", sceNpCppWebApiUnknown_N78k_minus_Vryooc);
int APS5_VABI sceNpCppWebApiUnknown_N78k_minus_Vryooc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("N9jUpAxISno", sceNpCppWebApiUnknown_N9jUpAxISno);
int APS5_VABI sceNpCppWebApiUnknown_N9jUpAxISno(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NCe1Kgd6Jlo", sceNpCppWebApiUnknown_NCe1Kgd6Jlo);
int APS5_VABI sceNpCppWebApiUnknown_NCe1Kgd6Jlo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NFreX0aMUyY", sceNpCppWebApiUnknown_NFreX0aMUyY);
int APS5_VABI sceNpCppWebApiUnknown_NFreX0aMUyY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NG-lbm2BAKU", sceNpCppWebApiUnknown_NG_minus_lbm2BAKU);
int APS5_VABI sceNpCppWebApiUnknown_NG_minus_lbm2BAKU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NGaR2vt2da4", sceNpCppWebApiUnknown_NGaR2vt2da4);
int APS5_VABI sceNpCppWebApiUnknown_NGaR2vt2da4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NJw4sqe8Ci0", sceNpCppWebApiUnknown_NJw4sqe8Ci0);
int APS5_VABI sceNpCppWebApiUnknown_NJw4sqe8Ci0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NKNYJptJYtY", sceNpCppWebApiUnknown_NKNYJptJYtY);
int APS5_VABI sceNpCppWebApiUnknown_NKNYJptJYtY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NMMIJ31zSo8", sceNpCppWebApiUnknown_NMMIJ31zSo8);
int APS5_VABI sceNpCppWebApiUnknown_NMMIJ31zSo8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NNVf18SlbT8", sceNpCppWebApiUnknown_NNVf18SlbT8);
int APS5_VABI sceNpCppWebApiUnknown_NNVf18SlbT8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NPr7arR8X0k", sceNpCppWebApiUnknown_NPr7arR8X0k);
int APS5_VABI sceNpCppWebApiUnknown_NPr7arR8X0k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NRX8fDa7344", sceNpCppWebApiUnknown_NRX8fDa7344);
int APS5_VABI sceNpCppWebApiUnknown_NRX8fDa7344(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NSgnzcPIWYY", sceNpCppWebApiUnknown_NSgnzcPIWYY);
int APS5_VABI sceNpCppWebApiUnknown_NSgnzcPIWYY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NTELx+nQzhw", sceNpCppWebApiUnknown_NTELx_plus_nQzhw);
int APS5_VABI sceNpCppWebApiUnknown_NTELx_plus_nQzhw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NUKnIPTIUWs", sceNpCppWebApiUnknown_NUKnIPTIUWs);
int APS5_VABI sceNpCppWebApiUnknown_NUKnIPTIUWs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NYIdgQKy4fs", sceNpCppWebApiUnknown_NYIdgQKy4fs);
int APS5_VABI sceNpCppWebApiUnknown_NYIdgQKy4fs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NZ2HFbENJYM", sceNpCppWebApiUnknown_NZ2HFbENJYM);
int APS5_VABI sceNpCppWebApiUnknown_NZ2HFbENJYM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NaCmgKItIgo", sceNpCppWebApiUnknown_NaCmgKItIgo);
int APS5_VABI sceNpCppWebApiUnknown_NaCmgKItIgo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NdsHawcRBNk", sceNpCppWebApiUnknown_NdsHawcRBNk);
int APS5_VABI sceNpCppWebApiUnknown_NdsHawcRBNk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NeMAgSIPdzI", sceNpCppWebApiUnknown_NeMAgSIPdzI);
int APS5_VABI sceNpCppWebApiUnknown_NeMAgSIPdzI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NfSNgSBygmI", sceNpCppWebApiUnknown_NfSNgSBygmI);
int APS5_VABI sceNpCppWebApiUnknown_NfSNgSBygmI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Nip+FRfvDHU", sceNpCppWebApiUnknown_Nip_plus_FRfvDHU);
int APS5_VABI sceNpCppWebApiUnknown_Nip_plus_FRfvDHU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NjDbsTHlHRM", sceNpCppWebApiUnknown_NjDbsTHlHRM);
int APS5_VABI sceNpCppWebApiUnknown_NjDbsTHlHRM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Nk5eLqHjvZU", sceNpCppWebApiUnknown_Nk5eLqHjvZU);
int APS5_VABI sceNpCppWebApiUnknown_Nk5eLqHjvZU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NlQkxEcVfRU", sceNpCppWebApiUnknown_NlQkxEcVfRU);
int APS5_VABI sceNpCppWebApiUnknown_NlQkxEcVfRU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NnxzMmD8XJo", sceNpCppWebApiUnknown_NnxzMmD8XJo);
int APS5_VABI sceNpCppWebApiUnknown_NnxzMmD8XJo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NqoGDM7dSPs", sceNpCppWebApiUnknown_NqoGDM7dSPs);
int APS5_VABI sceNpCppWebApiUnknown_NqoGDM7dSPs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NvV--345al4", sceNpCppWebApiUnknown_NvV_minus__minus_345al4);
int APS5_VABI sceNpCppWebApiUnknown_NvV_minus__minus_345al4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("NzUX4yf7ccQ", sceNpCppWebApiUnknown_NzUX4yf7ccQ);
int APS5_VABI sceNpCppWebApiUnknown_NzUX4yf7ccQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("O+VoB8m5MXA", sceNpCppWebApiUnknown_O_plus_VoB8m5MXA);
int APS5_VABI sceNpCppWebApiUnknown_O_plus_VoB8m5MXA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("O-fW3bUQ3Ao", sceNpCppWebApiUnknown_O_minus_fW3bUQ3Ao);
int APS5_VABI sceNpCppWebApiUnknown_O_minus_fW3bUQ3Ao(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("O2W8UiBdLWk", sceNpCppWebApiUnknown_O2W8UiBdLWk);
int APS5_VABI sceNpCppWebApiUnknown_O2W8UiBdLWk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("O4L+0oCN9zA", sceNpCppWebApiUnknown_O4L_plus_0oCN9zA);
int APS5_VABI sceNpCppWebApiUnknown_O4L_plus_0oCN9zA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OCkMuKlsBy0", sceNpCppWebApiUnknown_OCkMuKlsBy0);
int APS5_VABI sceNpCppWebApiUnknown_OCkMuKlsBy0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OFUPL4Enbf8", sceNpCppWebApiUnknown_OFUPL4Enbf8);
int APS5_VABI sceNpCppWebApiUnknown_OFUPL4Enbf8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OIvAnAUdbQ8", sceNpCppWebApiUnknown_OIvAnAUdbQ8);
int APS5_VABI sceNpCppWebApiUnknown_OIvAnAUdbQ8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OJPTonqdg0I", sceNpCppWebApiUnknown_OJPTonqdg0I);
int APS5_VABI sceNpCppWebApiUnknown_OJPTonqdg0I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OJuFV3IDHZI", sceNpCppWebApiUnknown_OJuFV3IDHZI);
int APS5_VABI sceNpCppWebApiUnknown_OJuFV3IDHZI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OLgCzPhsacA", sceNpCppWebApiUnknown_OLgCzPhsacA);
int APS5_VABI sceNpCppWebApiUnknown_OLgCzPhsacA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OO5r3tJuV1I", sceNpCppWebApiUnknown_OO5r3tJuV1I);
int APS5_VABI sceNpCppWebApiUnknown_OO5r3tJuV1I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OTQRxO5GP0o", sceNpCppWebApiUnknown_OTQRxO5GP0o);
int APS5_VABI sceNpCppWebApiUnknown_OTQRxO5GP0o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OTilStjd9L8", sceNpCppWebApiUnknown_OTilStjd9L8);
int APS5_VABI sceNpCppWebApiUnknown_OTilStjd9L8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OV+MKKnbEYM", sceNpCppWebApiUnknown_OV_plus_MKKnbEYM);
int APS5_VABI sceNpCppWebApiUnknown_OV_plus_MKKnbEYM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OVsvmF7lYPQ", sceNpCppWebApiUnknown_OVsvmF7lYPQ);
int APS5_VABI sceNpCppWebApiUnknown_OVsvmF7lYPQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OW0SLrs6odM", sceNpCppWebApiUnknown_OW0SLrs6odM);
int APS5_VABI sceNpCppWebApiUnknown_OW0SLrs6odM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ObpPwWnpM8I", sceNpCppWebApiUnknown_ObpPwWnpM8I);
int APS5_VABI sceNpCppWebApiUnknown_ObpPwWnpM8I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Oc4wsNJNskY", sceNpCppWebApiUnknown_Oc4wsNJNskY);
int APS5_VABI sceNpCppWebApiUnknown_Oc4wsNJNskY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OcAgPxcq5Vk", sceNpCppWebApiUnknown_OcAgPxcq5Vk);
int APS5_VABI sceNpCppWebApiUnknown_OcAgPxcq5Vk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OdIsrLAknok", sceNpCppWebApiUnknown_OdIsrLAknok);
int APS5_VABI sceNpCppWebApiUnknown_OdIsrLAknok(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OdnfwPg6zUo", sceNpCppWebApiUnknown_OdnfwPg6zUo);
int APS5_VABI sceNpCppWebApiUnknown_OdnfwPg6zUo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Oe1W+jARzBQ", sceNpCppWebApiUnknown_Oe1W_plus_jARzBQ);
int APS5_VABI sceNpCppWebApiUnknown_Oe1W_plus_jARzBQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OeZaNLCgxdA", sceNpCppWebApiUnknown_OeZaNLCgxdA);
int APS5_VABI sceNpCppWebApiUnknown_OeZaNLCgxdA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Of4xukikvEk", sceNpCppWebApiUnknown_Of4xukikvEk);
int APS5_VABI sceNpCppWebApiUnknown_Of4xukikvEk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OfE+cft2qCg", sceNpCppWebApiUnknown_OfE_plus_cft2qCg);
int APS5_VABI sceNpCppWebApiUnknown_OfE_plus_cft2qCg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OfSm8N8i+XA", sceNpCppWebApiUnknown_OfSm8N8i_plus_XA);
int APS5_VABI sceNpCppWebApiUnknown_OfSm8N8i_plus_XA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OlMxxnDBfEg", sceNpCppWebApiUnknown_OlMxxnDBfEg);
int APS5_VABI sceNpCppWebApiUnknown_OlMxxnDBfEg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OmJ5HIMLuNg", sceNpCppWebApiUnknown_OmJ5HIMLuNg);
int APS5_VABI sceNpCppWebApiUnknown_OmJ5HIMLuNg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OokInMhyhn8", sceNpCppWebApiUnknown_OokInMhyhn8);
int APS5_VABI sceNpCppWebApiUnknown_OokInMhyhn8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Oqfphi8E-Hc", sceNpCppWebApiUnknown_Oqfphi8E_minus_Hc);
int APS5_VABI sceNpCppWebApiUnknown_Oqfphi8E_minus_Hc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OsAmT5z22q8", sceNpCppWebApiUnknown_OsAmT5z22q8);
int APS5_VABI sceNpCppWebApiUnknown_OsAmT5z22q8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("OvgA9cAjXns", sceNpCppWebApiUnknown_OvgA9cAjXns);
int APS5_VABI sceNpCppWebApiUnknown_OvgA9cAjXns(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Ovuffw32ExY", sceNpCppWebApiUnknown_Ovuffw32ExY);
int APS5_VABI sceNpCppWebApiUnknown_Ovuffw32ExY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("P3gUyz0kWxw", sceNpCppWebApiUnknown_P3gUyz0kWxw);
int APS5_VABI sceNpCppWebApiUnknown_P3gUyz0kWxw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("P4OkrF+7hsM", sceNpCppWebApiUnknown_P4OkrF_plus_7hsM);
int APS5_VABI sceNpCppWebApiUnknown_P4OkrF_plus_7hsM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("P4VhCJbgaOw", sceNpCppWebApiUnknown_P4VhCJbgaOw);
int APS5_VABI sceNpCppWebApiUnknown_P4VhCJbgaOw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("P4sWQuI4WLQ", sceNpCppWebApiUnknown_P4sWQuI4WLQ);
int APS5_VABI sceNpCppWebApiUnknown_P4sWQuI4WLQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("P5mDlGPLSUU", sceNpCppWebApiUnknown_P5mDlGPLSUU);
int APS5_VABI sceNpCppWebApiUnknown_P5mDlGPLSUU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("P6piso307SE", sceNpCppWebApiUnknown_P6piso307SE);
int APS5_VABI sceNpCppWebApiUnknown_P6piso307SE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PBrdYjoh-nU", sceNpCppWebApiUnknown_PBrdYjoh_minus_nU);
int APS5_VABI sceNpCppWebApiUnknown_PBrdYjoh_minus_nU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PDasby6lhDo", sceNpCppWebApiUnknown_PDasby6lhDo);
int APS5_VABI sceNpCppWebApiUnknown_PDasby6lhDo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PERc71DuaAs", sceNpCppWebApiUnknown_PERc71DuaAs);
int APS5_VABI sceNpCppWebApiUnknown_PERc71DuaAs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PGRYlJHDzro", sceNpCppWebApiUnknown_PGRYlJHDzro);
int APS5_VABI sceNpCppWebApiUnknown_PGRYlJHDzro(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PGbdeeOVRRU", sceNpCppWebApiUnknown_PGbdeeOVRRU);
int APS5_VABI sceNpCppWebApiUnknown_PGbdeeOVRRU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PI1X7RZGbYo", sceNpCppWebApiUnknown_PI1X7RZGbYo);
int APS5_VABI sceNpCppWebApiUnknown_PI1X7RZGbYo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PJ3AspBM8Mo", sceNpCppWebApiUnknown_PJ3AspBM8Mo);
int APS5_VABI sceNpCppWebApiUnknown_PJ3AspBM8Mo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PJPIXtFqKBM", sceNpCppWebApiUnknown_PJPIXtFqKBM);
int APS5_VABI sceNpCppWebApiUnknown_PJPIXtFqKBM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PJYDjH8b5Hg", sceNpCppWebApiUnknown_PJYDjH8b5Hg);
int APS5_VABI sceNpCppWebApiUnknown_PJYDjH8b5Hg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PKZOKLLg6oo", sceNpCppWebApiUnknown_PKZOKLLg6oo);
int APS5_VABI sceNpCppWebApiUnknown_PKZOKLLg6oo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PL5gk1ieSJk", sceNpCppWebApiUnknown_PL5gk1ieSJk);
int APS5_VABI sceNpCppWebApiUnknown_PL5gk1ieSJk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PP8lun2rCEU", sceNpCppWebApiUnknown_PP8lun2rCEU);
int APS5_VABI sceNpCppWebApiUnknown_PP8lun2rCEU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PR8r-TCBz0U", sceNpCppWebApiUnknown_PR8r_minus_TCBz0U);
int APS5_VABI sceNpCppWebApiUnknown_PR8r_minus_TCBz0U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PWBfEpSHJHo", sceNpCppWebApiUnknown_PWBfEpSHJHo);
int APS5_VABI sceNpCppWebApiUnknown_PWBfEpSHJHo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Pb8eru9c+lw", sceNpCppWebApiUnknown_Pb8eru9c_plus_lw);
int APS5_VABI sceNpCppWebApiUnknown_Pb8eru9c_plus_lw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PfBVKm1Magc", sceNpCppWebApiUnknown_PfBVKm1Magc);
int APS5_VABI sceNpCppWebApiUnknown_PfBVKm1Magc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PhsQLxwcDaI", sceNpCppWebApiUnknown_PhsQLxwcDaI);
int APS5_VABI sceNpCppWebApiUnknown_PhsQLxwcDaI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PiTXx78dkbo", sceNpCppWebApiUnknown_PiTXx78dkbo);
int APS5_VABI sceNpCppWebApiUnknown_PiTXx78dkbo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PjWWA4MpFFM", sceNpCppWebApiUnknown_PjWWA4MpFFM);
int APS5_VABI sceNpCppWebApiUnknown_PjWWA4MpFFM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PmMi+EcpbpM", sceNpCppWebApiUnknown_PmMi_plus_EcpbpM);
int APS5_VABI sceNpCppWebApiUnknown_PmMi_plus_EcpbpM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PmQt+EUQlmA", sceNpCppWebApiUnknown_PmQt_plus_EUQlmA);
int APS5_VABI sceNpCppWebApiUnknown_PmQt_plus_EUQlmA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Pmzfl-YOkdU", sceNpCppWebApiUnknown_Pmzfl_minus_YOkdU);
int APS5_VABI sceNpCppWebApiUnknown_Pmzfl_minus_YOkdU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PqRoVXyPIWA", sceNpCppWebApiUnknown_PqRoVXyPIWA);
int APS5_VABI sceNpCppWebApiUnknown_PqRoVXyPIWA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PqSY4P0AdQk", sceNpCppWebApiUnknown_PqSY4P0AdQk);
int APS5_VABI sceNpCppWebApiUnknown_PqSY4P0AdQk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PsX657reZeo", sceNpCppWebApiUnknown_PsX657reZeo);
int APS5_VABI sceNpCppWebApiUnknown_PsX657reZeo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PsjIb1R2yXg", sceNpCppWebApiUnknown_PsjIb1R2yXg);
int APS5_VABI sceNpCppWebApiUnknown_PsjIb1R2yXg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("PzLUwQXc7VM", sceNpCppWebApiUnknown_PzLUwQXc7VM);
int APS5_VABI sceNpCppWebApiUnknown_PzLUwQXc7VM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Q+mysfonf+A", sceNpCppWebApiUnknown_Q_plus_mysfonf_plus_A);
int APS5_VABI sceNpCppWebApiUnknown_Q_plus_mysfonf_plus_A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Q-u6dljYZ9o", sceNpCppWebApiUnknown_Q_minus_u6dljYZ9o);
int APS5_VABI sceNpCppWebApiUnknown_Q_minus_u6dljYZ9o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Q0fKqfQeXnI", sceNpCppWebApiUnknown_Q0fKqfQeXnI);
int APS5_VABI sceNpCppWebApiUnknown_Q0fKqfQeXnI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Q1WNakfGV08", sceNpCppWebApiUnknown_Q1WNakfGV08);
int APS5_VABI sceNpCppWebApiUnknown_Q1WNakfGV08(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Q64RmYv-DJM", sceNpCppWebApiUnknown_Q64RmYv_minus_DJM);
int APS5_VABI sceNpCppWebApiUnknown_Q64RmYv_minus_DJM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Q6vUUPnen-M", sceNpCppWebApiUnknown_Q6vUUPnen_minus_M);
int APS5_VABI sceNpCppWebApiUnknown_Q6vUUPnen_minus_M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QAXlroxEPKk", sceNpCppWebApiUnknown_QAXlroxEPKk);
int APS5_VABI sceNpCppWebApiUnknown_QAXlroxEPKk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QAZtKuy2wIg", sceNpCppWebApiUnknown_QAZtKuy2wIg);
int APS5_VABI sceNpCppWebApiUnknown_QAZtKuy2wIg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QBW1-h74Tfk", sceNpCppWebApiUnknown_QBW1_minus_h74Tfk);
int APS5_VABI sceNpCppWebApiUnknown_QBW1_minus_h74Tfk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QBzq0D46oqg", sceNpCppWebApiUnknown_QBzq0D46oqg);
int APS5_VABI sceNpCppWebApiUnknown_QBzq0D46oqg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QFE3c-4xZNU", sceNpCppWebApiUnknown_QFE3c_minus_4xZNU);
int APS5_VABI sceNpCppWebApiUnknown_QFE3c_minus_4xZNU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QGxBoGah9yU", sceNpCppWebApiUnknown_QGxBoGah9yU);
int APS5_VABI sceNpCppWebApiUnknown_QGxBoGah9yU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QKrNgVo170o", sceNpCppWebApiUnknown_QKrNgVo170o);
int APS5_VABI sceNpCppWebApiUnknown_QKrNgVo170o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QLeIK5tpVUU", sceNpCppWebApiUnknown_QLeIK5tpVUU);
int APS5_VABI sceNpCppWebApiUnknown_QLeIK5tpVUU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QNgg1huXSW8", sceNpCppWebApiUnknown_QNgg1huXSW8);
int APS5_VABI sceNpCppWebApiUnknown_QNgg1huXSW8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QNpeGOb3qzY", sceNpCppWebApiUnknown_QNpeGOb3qzY);
int APS5_VABI sceNpCppWebApiUnknown_QNpeGOb3qzY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QQdPV66dHBg", sceNpCppWebApiUnknown_QQdPV66dHBg);
int APS5_VABI sceNpCppWebApiUnknown_QQdPV66dHBg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QU4gBqAEVtM", sceNpCppWebApiUnknown_QU4gBqAEVtM);
int APS5_VABI sceNpCppWebApiUnknown_QU4gBqAEVtM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QWSiZJDtguI", sceNpCppWebApiUnknown_QWSiZJDtguI);
int APS5_VABI sceNpCppWebApiUnknown_QWSiZJDtguI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QXLG4UDWLEI", sceNpCppWebApiUnknown_QXLG4UDWLEI);
int APS5_VABI sceNpCppWebApiUnknown_QXLG4UDWLEI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QXah3pw5xfc", sceNpCppWebApiUnknown_QXah3pw5xfc);
int APS5_VABI sceNpCppWebApiUnknown_QXah3pw5xfc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QXmqerVwWZM", sceNpCppWebApiUnknown_QXmqerVwWZM);
int APS5_VABI sceNpCppWebApiUnknown_QXmqerVwWZM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QYwFL46iKSA", sceNpCppWebApiUnknown_QYwFL46iKSA);
int APS5_VABI sceNpCppWebApiUnknown_QYwFL46iKSA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QaynXlxYhQ8", sceNpCppWebApiUnknown_QaynXlxYhQ8);
int APS5_VABI sceNpCppWebApiUnknown_QaynXlxYhQ8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QcImFaI26pw", sceNpCppWebApiUnknown_QcImFaI26pw);
int APS5_VABI sceNpCppWebApiUnknown_QcImFaI26pw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QdU3cirVIdE", sceNpCppWebApiUnknown_QdU3cirVIdE);
int APS5_VABI sceNpCppWebApiUnknown_QdU3cirVIdE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QdjltHtjuQo", sceNpCppWebApiUnknown_QdjltHtjuQo);
int APS5_VABI sceNpCppWebApiUnknown_QdjltHtjuQo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QeO7GQrfXnU", sceNpCppWebApiUnknown_QeO7GQrfXnU);
int APS5_VABI sceNpCppWebApiUnknown_QeO7GQrfXnU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QgNyrC5sOsM", sceNpCppWebApiUnknown_QgNyrC5sOsM);
int APS5_VABI sceNpCppWebApiUnknown_QgNyrC5sOsM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QkKeZCHCFBg", sceNpCppWebApiUnknown_QkKeZCHCFBg);
int APS5_VABI sceNpCppWebApiUnknown_QkKeZCHCFBg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Qltt2qSBU1I", sceNpCppWebApiUnknown_Qltt2qSBU1I);
int APS5_VABI sceNpCppWebApiUnknown_Qltt2qSBU1I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Qn83xIhFHK8", sceNpCppWebApiUnknown_Qn83xIhFHK8);
int APS5_VABI sceNpCppWebApiUnknown_Qn83xIhFHK8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QnG7AKNnigk", sceNpCppWebApiUnknown_QnG7AKNnigk);
int APS5_VABI sceNpCppWebApiUnknown_QnG7AKNnigk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QwwfhtPOTWA", sceNpCppWebApiUnknown_QwwfhtPOTWA);
int APS5_VABI sceNpCppWebApiUnknown_QwwfhtPOTWA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QxB6IX2hOYg", sceNpCppWebApiUnknown_QxB6IX2hOYg);
int APS5_VABI sceNpCppWebApiUnknown_QxB6IX2hOYg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QxatxkNYNls", sceNpCppWebApiUnknown_QxatxkNYNls);
int APS5_VABI sceNpCppWebApiUnknown_QxatxkNYNls(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QxtL3miUkVQ", sceNpCppWebApiUnknown_QxtL3miUkVQ);
int APS5_VABI sceNpCppWebApiUnknown_QxtL3miUkVQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QzKntnYEOXc", sceNpCppWebApiUnknown_QzKntnYEOXc);
int APS5_VABI sceNpCppWebApiUnknown_QzKntnYEOXc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("QzaDg6FyTQU", sceNpCppWebApiUnknown_QzaDg6FyTQU);
int APS5_VABI sceNpCppWebApiUnknown_QzaDg6FyTQU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("R0KW845D-BE", sceNpCppWebApiUnknown_R0KW845D_minus_BE);
int APS5_VABI sceNpCppWebApiUnknown_R0KW845D_minus_BE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("R0Q4Gb2MOpI", sceNpCppWebApiUnknown_R0Q4Gb2MOpI);
int APS5_VABI sceNpCppWebApiUnknown_R0Q4Gb2MOpI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("R5QhFOlpzjo", sceNpCppWebApiUnknown_R5QhFOlpzjo);
int APS5_VABI sceNpCppWebApiUnknown_R5QhFOlpzjo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("R5UMy7LMszo", sceNpCppWebApiUnknown_R5UMy7LMszo);
int APS5_VABI sceNpCppWebApiUnknown_R5UMy7LMszo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("R6ZajxSYd8E", sceNpCppWebApiUnknown_R6ZajxSYd8E);
int APS5_VABI sceNpCppWebApiUnknown_R6ZajxSYd8E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("R9uJB8bxFPM", sceNpCppWebApiUnknown_R9uJB8bxFPM);
int APS5_VABI sceNpCppWebApiUnknown_R9uJB8bxFPM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RB5LlxlbLPI", sceNpCppWebApiUnknown_RB5LlxlbLPI);
int APS5_VABI sceNpCppWebApiUnknown_RB5LlxlbLPI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RM35a8uFre4", sceNpCppWebApiUnknown_RM35a8uFre4);
int APS5_VABI sceNpCppWebApiUnknown_RM35a8uFre4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RMcwsFqcuXU", sceNpCppWebApiUnknown_RMcwsFqcuXU);
int APS5_VABI sceNpCppWebApiUnknown_RMcwsFqcuXU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RQfqM1BZuoc", sceNpCppWebApiUnknown_RQfqM1BZuoc);
int APS5_VABI sceNpCppWebApiUnknown_RQfqM1BZuoc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RTzesATSziM", sceNpCppWebApiUnknown_RTzesATSziM);
int APS5_VABI sceNpCppWebApiUnknown_RTzesATSziM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RUZA5-5hfvg", sceNpCppWebApiUnknown_RUZA5_minus_5hfvg);
int APS5_VABI sceNpCppWebApiUnknown_RUZA5_minus_5hfvg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RXqt13UAFeY", sceNpCppWebApiUnknown_RXqt13UAFeY);
int APS5_VABI sceNpCppWebApiUnknown_RXqt13UAFeY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RZYvTMuqmhw", sceNpCppWebApiUnknown_RZYvTMuqmhw);
int APS5_VABI sceNpCppWebApiUnknown_RZYvTMuqmhw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RbGsR3uLRKg", sceNpCppWebApiUnknown_RbGsR3uLRKg);
int APS5_VABI sceNpCppWebApiUnknown_RbGsR3uLRKg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RcqsPn2xXI4", sceNpCppWebApiUnknown_RcqsPn2xXI4);
int APS5_VABI sceNpCppWebApiUnknown_RcqsPn2xXI4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RdRtBInu8gw", sceNpCppWebApiUnknown_RdRtBInu8gw);
int APS5_VABI sceNpCppWebApiUnknown_RdRtBInu8gw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ReJOJwhpsLU", sceNpCppWebApiUnknown_ReJOJwhpsLU);
int APS5_VABI sceNpCppWebApiUnknown_ReJOJwhpsLU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Rfm9T4Fhns0", sceNpCppWebApiUnknown_Rfm9T4Fhns0);
int APS5_VABI sceNpCppWebApiUnknown_Rfm9T4Fhns0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Rg9KCjkElx4", sceNpCppWebApiUnknown_Rg9KCjkElx4);
int APS5_VABI sceNpCppWebApiUnknown_Rg9KCjkElx4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RiA-QbWEAbo", sceNpCppWebApiUnknown_RiA_minus_QbWEAbo);
int APS5_VABI sceNpCppWebApiUnknown_RiA_minus_QbWEAbo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RiMz3Wlmt8Q", sceNpCppWebApiUnknown_RiMz3Wlmt8Q);
int APS5_VABI sceNpCppWebApiUnknown_RiMz3Wlmt8Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Rj1cdK6XqPY", sceNpCppWebApiUnknown_Rj1cdK6XqPY);
int APS5_VABI sceNpCppWebApiUnknown_Rj1cdK6XqPY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RjMlsR8EXrw", sceNpCppWebApiUnknown_RjMlsR8EXrw);
int APS5_VABI sceNpCppWebApiUnknown_RjMlsR8EXrw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RjP6EdmFhuM", sceNpCppWebApiUnknown_RjP6EdmFhuM);
int APS5_VABI sceNpCppWebApiUnknown_RjP6EdmFhuM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Rk-LaLv8rzI", sceNpCppWebApiUnknown_Rk_minus_LaLv8rzI);
int APS5_VABI sceNpCppWebApiUnknown_Rk_minus_LaLv8rzI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RmJKkzZFNFA", sceNpCppWebApiUnknown_RmJKkzZFNFA);
int APS5_VABI sceNpCppWebApiUnknown_RmJKkzZFNFA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RolXukwAUco", sceNpCppWebApiUnknown_RolXukwAUco);
int APS5_VABI sceNpCppWebApiUnknown_RolXukwAUco(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RqoJBNCaLyo", sceNpCppWebApiUnknown_RqoJBNCaLyo);
int APS5_VABI sceNpCppWebApiUnknown_RqoJBNCaLyo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Rr1p24Dyq+w", sceNpCppWebApiUnknown_Rr1p24Dyq_plus_w);
int APS5_VABI sceNpCppWebApiUnknown_Rr1p24Dyq_plus_w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RujUxbr3haM", sceNpCppWebApiUnknown_RujUxbr3haM);
int APS5_VABI sceNpCppWebApiUnknown_RujUxbr3haM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RvH9AZ5rCkQ", sceNpCppWebApiUnknown_RvH9AZ5rCkQ);
int APS5_VABI sceNpCppWebApiUnknown_RvH9AZ5rCkQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RvSjdfZQRMQ", sceNpCppWebApiUnknown_RvSjdfZQRMQ);
int APS5_VABI sceNpCppWebApiUnknown_RvSjdfZQRMQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("RzJr+LF13gI", sceNpCppWebApiUnknown_RzJr_plus_LF13gI);
int APS5_VABI sceNpCppWebApiUnknown_RzJr_plus_LF13gI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("S-Gdd1eIiKU", sceNpCppWebApiUnknown_S_minus_Gdd1eIiKU);
int APS5_VABI sceNpCppWebApiUnknown_S_minus_Gdd1eIiKU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("S-tuNnMKjfQ", sceNpCppWebApiUnknown_S_minus_tuNnMKjfQ);
int APS5_VABI sceNpCppWebApiUnknown_S_minus_tuNnMKjfQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("S3JxD633sV4", sceNpCppWebApiUnknown_S3JxD633sV4);
int APS5_VABI sceNpCppWebApiUnknown_S3JxD633sV4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("S4txuoFSiu8", sceNpCppWebApiUnknown_S4txuoFSiu8);
int APS5_VABI sceNpCppWebApiUnknown_S4txuoFSiu8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("S7r7-hGiFO8", sceNpCppWebApiUnknown_S7r7_minus_hGiFO8);
int APS5_VABI sceNpCppWebApiUnknown_S7r7_minus_hGiFO8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SBBILMusZlc", sceNpCppWebApiUnknown_SBBILMusZlc);
int APS5_VABI sceNpCppWebApiUnknown_SBBILMusZlc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SBIme60FhnY", sceNpCppWebApiUnknown_SBIme60FhnY);
int APS5_VABI sceNpCppWebApiUnknown_SBIme60FhnY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SBQ0uFoUymc", sceNpCppWebApiUnknown_SBQ0uFoUymc);
int APS5_VABI sceNpCppWebApiUnknown_SBQ0uFoUymc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SCSNVsQRpSw", sceNpCppWebApiUnknown_SCSNVsQRpSw);
int APS5_VABI sceNpCppWebApiUnknown_SCSNVsQRpSw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SJBKLQrHqFw", sceNpCppWebApiUnknown_SJBKLQrHqFw);
int APS5_VABI sceNpCppWebApiUnknown_SJBKLQrHqFw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SK2UF3FYqM4", sceNpCppWebApiUnknown_SK2UF3FYqM4);
int APS5_VABI sceNpCppWebApiUnknown_SK2UF3FYqM4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SL98pRGwFko", sceNpCppWebApiUnknown_SL98pRGwFko);
int APS5_VABI sceNpCppWebApiUnknown_SL98pRGwFko(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SNwqnx6r3c8", sceNpCppWebApiUnknown_SNwqnx6r3c8);
int APS5_VABI sceNpCppWebApiUnknown_SNwqnx6r3c8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SOCxwiW2uqw", sceNpCppWebApiUnknown_SOCxwiW2uqw);
int APS5_VABI sceNpCppWebApiUnknown_SOCxwiW2uqw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SQMjAiGtsiI", sceNpCppWebApiUnknown_SQMjAiGtsiI);
int APS5_VABI sceNpCppWebApiUnknown_SQMjAiGtsiI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SR3BKonD8yk", sceNpCppWebApiUnknown_SR3BKonD8yk);
int APS5_VABI sceNpCppWebApiUnknown_SR3BKonD8yk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SR76Quk0L9Y", sceNpCppWebApiUnknown_SR76Quk0L9Y);
int APS5_VABI sceNpCppWebApiUnknown_SR76Quk0L9Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("STSh215IhuM", sceNpCppWebApiUnknown_STSh215IhuM);
int APS5_VABI sceNpCppWebApiUnknown_STSh215IhuM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SUOUmSmPy5E", sceNpCppWebApiUnknown_SUOUmSmPy5E);
int APS5_VABI sceNpCppWebApiUnknown_SUOUmSmPy5E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SWVe+Akobhk", sceNpCppWebApiUnknown_SWVe_plus_Akobhk);
int APS5_VABI sceNpCppWebApiUnknown_SWVe_plus_Akobhk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SY+ctdf-emY", sceNpCppWebApiUnknown_SY_plus_ctdf_minus_emY);
int APS5_VABI sceNpCppWebApiUnknown_SY_plus_ctdf_minus_emY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SYo0cly-xio", sceNpCppWebApiUnknown_SYo0cly_minus_xio);
int APS5_VABI sceNpCppWebApiUnknown_SYo0cly_minus_xio(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SZpEJeI60T0", sceNpCppWebApiUnknown_SZpEJeI60T0);
int APS5_VABI sceNpCppWebApiUnknown_SZpEJeI60T0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Sa8gEQuXm1w", sceNpCppWebApiUnknown_Sa8gEQuXm1w);
int APS5_VABI sceNpCppWebApiUnknown_Sa8gEQuXm1w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SdZ9dpNQ5lA", sceNpCppWebApiUnknown_SdZ9dpNQ5lA);
int APS5_VABI sceNpCppWebApiUnknown_SdZ9dpNQ5lA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Sdfm7eE3q80", sceNpCppWebApiUnknown_Sdfm7eE3q80);
int APS5_VABI sceNpCppWebApiUnknown_Sdfm7eE3q80(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SeccwfyNaAQ", sceNpCppWebApiUnknown_SeccwfyNaAQ);
int APS5_VABI sceNpCppWebApiUnknown_SeccwfyNaAQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Shl1yWD1LfE", sceNpCppWebApiUnknown_Shl1yWD1LfE);
int APS5_VABI sceNpCppWebApiUnknown_Shl1yWD1LfE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Smy8175FjUE", sceNpCppWebApiUnknown_Smy8175FjUE);
int APS5_VABI sceNpCppWebApiUnknown_Smy8175FjUE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Sol9Dxan-7I", sceNpCppWebApiUnknown_Sol9Dxan_minus_7I);
int APS5_VABI sceNpCppWebApiUnknown_Sol9Dxan_minus_7I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SrvkW9SlLuk", sceNpCppWebApiUnknown_SrvkW9SlLuk);
int APS5_VABI sceNpCppWebApiUnknown_SrvkW9SlLuk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SvkCnFdkw64", sceNpCppWebApiUnknown_SvkCnFdkw64);
int APS5_VABI sceNpCppWebApiUnknown_SvkCnFdkw64(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("SyilEZ5n3wU", sceNpCppWebApiUnknown_SyilEZ5n3wU);
int APS5_VABI sceNpCppWebApiUnknown_SyilEZ5n3wU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("T+B9qAqbrw4", sceNpCppWebApiUnknown_T_plus_B9qAqbrw4);
int APS5_VABI sceNpCppWebApiUnknown_T_plus_B9qAqbrw4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("T0CzqR5tLXE", sceNpCppWebApiUnknown_T0CzqR5tLXE);
int APS5_VABI sceNpCppWebApiUnknown_T0CzqR5tLXE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("T23YLQIlyFY", sceNpCppWebApiUnknown_T23YLQIlyFY);
int APS5_VABI sceNpCppWebApiUnknown_T23YLQIlyFY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("T7EoauKK+cM", sceNpCppWebApiUnknown_T7EoauKK_plus_cM);
int APS5_VABI sceNpCppWebApiUnknown_T7EoauKK_plus_cM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("T9OhZwk3U8o", sceNpCppWebApiUnknown_T9OhZwk3U8o);
int APS5_VABI sceNpCppWebApiUnknown_T9OhZwk3U8o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TCJpUloY2bw", sceNpCppWebApiUnknown_TCJpUloY2bw);
int APS5_VABI sceNpCppWebApiUnknown_TCJpUloY2bw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TDZC+Ww30ys", sceNpCppWebApiUnknown_TDZC_plus_Ww30ys);
int APS5_VABI sceNpCppWebApiUnknown_TDZC_plus_Ww30ys(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TGYjQWpJ2Z8", sceNpCppWebApiUnknown_TGYjQWpJ2Z8);
int APS5_VABI sceNpCppWebApiUnknown_TGYjQWpJ2Z8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("THFniQXBq9E", sceNpCppWebApiUnknown_THFniQXBq9E);
int APS5_VABI sceNpCppWebApiUnknown_THFniQXBq9E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TIdDZeVUTJ4", sceNpCppWebApiUnknown_TIdDZeVUTJ4);
int APS5_VABI sceNpCppWebApiUnknown_TIdDZeVUTJ4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TJIV4zGjz8o", sceNpCppWebApiUnknown_TJIV4zGjz8o);
int APS5_VABI sceNpCppWebApiUnknown_TJIV4zGjz8o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TJz8sDUa6n4", sceNpCppWebApiUnknown_TJz8sDUa6n4);
int APS5_VABI sceNpCppWebApiUnknown_TJz8sDUa6n4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TK-C+fGnZik", sceNpCppWebApiUnknown_TK_minus_C_plus_fGnZik);
int APS5_VABI sceNpCppWebApiUnknown_TK_minus_C_plus_fGnZik(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TRnNxoUlj70", sceNpCppWebApiUnknown_TRnNxoUlj70);
int APS5_VABI sceNpCppWebApiUnknown_TRnNxoUlj70(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TSrREisrFnA", sceNpCppWebApiUnknown_TSrREisrFnA);
int APS5_VABI sceNpCppWebApiUnknown_TSrREisrFnA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TVfbf1sXt0A", sceNpCppWebApiUnknown_TVfbf1sXt0A);
int APS5_VABI sceNpCppWebApiUnknown_TVfbf1sXt0A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TbZhQodHqx4", sceNpCppWebApiUnknown_TbZhQodHqx4);
int APS5_VABI sceNpCppWebApiUnknown_TbZhQodHqx4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TdjUv1NVR2I", sceNpCppWebApiUnknown_TdjUv1NVR2I);
int APS5_VABI sceNpCppWebApiUnknown_TdjUv1NVR2I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TldoClSl8Ek", sceNpCppWebApiUnknown_TldoClSl8Ek);
int APS5_VABI sceNpCppWebApiUnknown_TldoClSl8Ek(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TmJpVfEDPo4", sceNpCppWebApiUnknown_TmJpVfEDPo4);
int APS5_VABI sceNpCppWebApiUnknown_TmJpVfEDPo4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Tn3enK0KXAw", sceNpCppWebApiUnknown_Tn3enK0KXAw);
int APS5_VABI sceNpCppWebApiUnknown_Tn3enK0KXAw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TnC+MS5Fmtg", sceNpCppWebApiUnknown_TnC_plus_MS5Fmtg);
int APS5_VABI sceNpCppWebApiUnknown_TnC_plus_MS5Fmtg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TnEmXni08qk", sceNpCppWebApiUnknown_TnEmXni08qk);
int APS5_VABI sceNpCppWebApiUnknown_TnEmXni08qk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TnqhMyvzjHk", sceNpCppWebApiUnknown_TnqhMyvzjHk);
int APS5_VABI sceNpCppWebApiUnknown_TnqhMyvzjHk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Tp7ptaLjJBE", sceNpCppWebApiUnknown_Tp7ptaLjJBE);
int APS5_VABI sceNpCppWebApiUnknown_Tp7ptaLjJBE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TpeZWm8upgg", sceNpCppWebApiUnknown_TpeZWm8upgg);
int APS5_VABI sceNpCppWebApiUnknown_TpeZWm8upgg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TrRrJp+BxS4", sceNpCppWebApiUnknown_TrRrJp_plus_BxS4);
int APS5_VABI sceNpCppWebApiUnknown_TrRrJp_plus_BxS4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("TvpCfaZLFBw", sceNpCppWebApiUnknown_TvpCfaZLFBw);
int APS5_VABI sceNpCppWebApiUnknown_TvpCfaZLFBw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("U2aZV5ZkjyI", sceNpCppWebApiUnknown_U2aZV5ZkjyI);
int APS5_VABI sceNpCppWebApiUnknown_U2aZV5ZkjyI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("U3xnMqYSrnA", sceNpCppWebApiUnknown_U3xnMqYSrnA);
int APS5_VABI sceNpCppWebApiUnknown_U3xnMqYSrnA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("U5JdnyRtuWc", sceNpCppWebApiUnknown_U5JdnyRtuWc);
int APS5_VABI sceNpCppWebApiUnknown_U5JdnyRtuWc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("U9QiJMy8SFA", sceNpCppWebApiUnknown_U9QiJMy8SFA);
int APS5_VABI sceNpCppWebApiUnknown_U9QiJMy8SFA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UAH9GuC-H8w", sceNpCppWebApiUnknown_UAH9GuC_minus_H8w);
int APS5_VABI sceNpCppWebApiUnknown_UAH9GuC_minus_H8w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UD3voE5uO3k", sceNpCppWebApiUnknown_UD3voE5uO3k);
int APS5_VABI sceNpCppWebApiUnknown_UD3voE5uO3k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UEc2r2t5mEU", sceNpCppWebApiUnknown_UEc2r2t5mEU);
int APS5_VABI sceNpCppWebApiUnknown_UEc2r2t5mEU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UGittJii7dI", sceNpCppWebApiUnknown_UGittJii7dI);
int APS5_VABI sceNpCppWebApiUnknown_UGittJii7dI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UM5sJwwMUuI", sceNpCppWebApiUnknown_UM5sJwwMUuI);
int APS5_VABI sceNpCppWebApiUnknown_UM5sJwwMUuI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UNKRtjBCvUE", sceNpCppWebApiUnknown_UNKRtjBCvUE);
int APS5_VABI sceNpCppWebApiUnknown_UNKRtjBCvUE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UPhP7g1EWsI", sceNpCppWebApiUnknown_UPhP7g1EWsI);
int APS5_VABI sceNpCppWebApiUnknown_UPhP7g1EWsI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UQzg1PX27Q8", sceNpCppWebApiUnknown_UQzg1PX27Q8);
int APS5_VABI sceNpCppWebApiUnknown_UQzg1PX27Q8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("URZOfbC4fzU", sceNpCppWebApiUnknown_URZOfbC4fzU);
int APS5_VABI sceNpCppWebApiUnknown_URZOfbC4fzU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UReOIxlkMjo", sceNpCppWebApiUnknown_UReOIxlkMjo);
int APS5_VABI sceNpCppWebApiUnknown_UReOIxlkMjo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("USUuOd-dcYQ", sceNpCppWebApiUnknown_USUuOd_minus_dcYQ);
int APS5_VABI sceNpCppWebApiUnknown_USUuOd_minus_dcYQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UWAbPAGUbT4", sceNpCppWebApiUnknown_UWAbPAGUbT4);
int APS5_VABI sceNpCppWebApiUnknown_UWAbPAGUbT4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UWIQyDcz1hs", sceNpCppWebApiUnknown_UWIQyDcz1hs);
int APS5_VABI sceNpCppWebApiUnknown_UWIQyDcz1hs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UWQwOo1drgk", sceNpCppWebApiUnknown_UWQwOo1drgk);
int APS5_VABI sceNpCppWebApiUnknown_UWQwOo1drgk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UXZhDlHMhQo", sceNpCppWebApiUnknown_UXZhDlHMhQo);
int APS5_VABI sceNpCppWebApiUnknown_UXZhDlHMhQo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UYPxv8MIzGo", sceNpCppWebApiUnknown_UYPxv8MIzGo);
int APS5_VABI sceNpCppWebApiUnknown_UYPxv8MIzGo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UbjNnuuOgCc", sceNpCppWebApiUnknown_UbjNnuuOgCc);
int APS5_VABI sceNpCppWebApiUnknown_UbjNnuuOgCc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Uc5eSnk3dvE", sceNpCppWebApiUnknown_Uc5eSnk3dvE);
int APS5_VABI sceNpCppWebApiUnknown_Uc5eSnk3dvE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UfnL9ZaxWd4", sceNpCppWebApiUnknown_UfnL9ZaxWd4);
int APS5_VABI sceNpCppWebApiUnknown_UfnL9ZaxWd4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Uh3L-l186q8", sceNpCppWebApiUnknown_Uh3L_minus_l186q8);
int APS5_VABI sceNpCppWebApiUnknown_Uh3L_minus_l186q8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Uk5LZyI6uuA", sceNpCppWebApiUnknown_Uk5LZyI6uuA);
int APS5_VABI sceNpCppWebApiUnknown_Uk5LZyI6uuA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Un-Nm66asII", sceNpCppWebApiUnknown_Un_minus_Nm66asII);
int APS5_VABI sceNpCppWebApiUnknown_Un_minus_Nm66asII(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UqbbaKxqxd4", sceNpCppWebApiUnknown_UqbbaKxqxd4);
int APS5_VABI sceNpCppWebApiUnknown_UqbbaKxqxd4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UsoNZ5gxEuo", sceNpCppWebApiUnknown_UsoNZ5gxEuo);
int APS5_VABI sceNpCppWebApiUnknown_UsoNZ5gxEuo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UtTetxvQ8Zg", sceNpCppWebApiUnknown_UtTetxvQ8Zg);
int APS5_VABI sceNpCppWebApiUnknown_UtTetxvQ8Zg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UtXl-tmi7iw", sceNpCppWebApiUnknown_UtXl_minus_tmi7iw);
int APS5_VABI sceNpCppWebApiUnknown_UtXl_minus_tmi7iw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UtvF5urnmHc", sceNpCppWebApiUnknown_UtvF5urnmHc);
int APS5_VABI sceNpCppWebApiUnknown_UtvF5urnmHc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Uy+spyud5AY", sceNpCppWebApiUnknown_Uy_plus_spyud5AY);
int APS5_VABI sceNpCppWebApiUnknown_Uy_plus_spyud5AY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UzT-6N3lHz8", sceNpCppWebApiUnknown_UzT_minus_6N3lHz8);
int APS5_VABI sceNpCppWebApiUnknown_UzT_minus_6N3lHz8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("UzU2Exfq1M8", sceNpCppWebApiUnknown_UzU2Exfq1M8);
int APS5_VABI sceNpCppWebApiUnknown_UzU2Exfq1M8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("V+F2j7pxUPw", sceNpCppWebApiUnknown_V_plus_F2j7pxUPw);
int APS5_VABI sceNpCppWebApiUnknown_V_plus_F2j7pxUPw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("V0mB3kD5rJ8", sceNpCppWebApiUnknown_V0mB3kD5rJ8);
int APS5_VABI sceNpCppWebApiUnknown_V0mB3kD5rJ8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("V0rY+DfuHcc", sceNpCppWebApiUnknown_V0rY_plus_DfuHcc);
int APS5_VABI sceNpCppWebApiUnknown_V0rY_plus_DfuHcc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("V1XNGDVRh9M", sceNpCppWebApiUnknown_V1XNGDVRh9M);
int APS5_VABI sceNpCppWebApiUnknown_V1XNGDVRh9M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("V1jgl1b2Qog", sceNpCppWebApiUnknown_V1jgl1b2Qog);
int APS5_VABI sceNpCppWebApiUnknown_V1jgl1b2Qog(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("V27QHoEy6MY", sceNpCppWebApiUnknown_V27QHoEy6MY);
int APS5_VABI sceNpCppWebApiUnknown_V27QHoEy6MY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("V2NelZNoJwI", sceNpCppWebApiUnknown_V2NelZNoJwI);
int APS5_VABI sceNpCppWebApiUnknown_V2NelZNoJwI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("V7YhUQ+eU3Q", sceNpCppWebApiUnknown_V7YhUQ_plus_eU3Q);
int APS5_VABI sceNpCppWebApiUnknown_V7YhUQ_plus_eU3Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("V86XwpBrzzk", sceNpCppWebApiUnknown_V86XwpBrzzk);
int APS5_VABI sceNpCppWebApiUnknown_V86XwpBrzzk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("V9yO0FsYFno", sceNpCppWebApiUnknown_V9yO0FsYFno);
int APS5_VABI sceNpCppWebApiUnknown_V9yO0FsYFno(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VDv4UrXnxF4", sceNpCppWebApiUnknown_VDv4UrXnxF4);
int APS5_VABI sceNpCppWebApiUnknown_VDv4UrXnxF4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VFr1+v+P8+c", sceNpCppWebApiUnknown_VFr1_plus_v_plus_P8_plus_c);
int APS5_VABI sceNpCppWebApiUnknown_VFr1_plus_v_plus_P8_plus_c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VKaRucCPRyU", sceNpCppWebApiUnknown_VKaRucCPRyU);
int APS5_VABI sceNpCppWebApiUnknown_VKaRucCPRyU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VPbJwTCgME0", sceNpCppWebApiUnknown_VPbJwTCgME0);
int APS5_VABI sceNpCppWebApiUnknown_VPbJwTCgME0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VTZ+4rVcoJM", sceNpCppWebApiUnknown_VTZ_plus_4rVcoJM);
int APS5_VABI sceNpCppWebApiUnknown_VTZ_plus_4rVcoJM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VVi2+kwIbCc", sceNpCppWebApiUnknown_VVi2_plus_kwIbCc);
int APS5_VABI sceNpCppWebApiUnknown_VVi2_plus_kwIbCc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VW+Fo-JVvOk", sceNpCppWebApiUnknown_VW_plus_Fo_minus_JVvOk);
int APS5_VABI sceNpCppWebApiUnknown_VW_plus_Fo_minus_JVvOk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VXm5RXt+uJ0", sceNpCppWebApiUnknown_VXm5RXt_plus_uJ0);
int APS5_VABI sceNpCppWebApiUnknown_VXm5RXt_plus_uJ0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Vas2eInFOZM", sceNpCppWebApiUnknown_Vas2eInFOZM);
int APS5_VABI sceNpCppWebApiUnknown_Vas2eInFOZM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Vb3Xkb4A1mw", sceNpCppWebApiUnknown_Vb3Xkb4A1mw);
int APS5_VABI sceNpCppWebApiUnknown_Vb3Xkb4A1mw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VbuMfjgZqNY", sceNpCppWebApiUnknown_VbuMfjgZqNY);
int APS5_VABI sceNpCppWebApiUnknown_VbuMfjgZqNY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VdjJFagyPDA", sceNpCppWebApiUnknown_VdjJFagyPDA);
int APS5_VABI sceNpCppWebApiUnknown_VdjJFagyPDA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VekcO4txY-Q", sceNpCppWebApiUnknown_VekcO4txY_minus_Q);
int APS5_VABI sceNpCppWebApiUnknown_VekcO4txY_minus_Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VhKnWhtDDEg", sceNpCppWebApiUnknown_VhKnWhtDDEg);
int APS5_VABI sceNpCppWebApiUnknown_VhKnWhtDDEg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VhWD7ylSQu8", sceNpCppWebApiUnknown_VhWD7ylSQu8);
int APS5_VABI sceNpCppWebApiUnknown_VhWD7ylSQu8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VhcO5ZV4APE", sceNpCppWebApiUnknown_VhcO5ZV4APE);
int APS5_VABI sceNpCppWebApiUnknown_VhcO5ZV4APE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VjU9XzKj5H0", sceNpCppWebApiUnknown_VjU9XzKj5H0);
int APS5_VABI sceNpCppWebApiUnknown_VjU9XzKj5H0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Vjv+I-aNWFU", sceNpCppWebApiUnknown_Vjv_plus_I_minus_aNWFU);
int APS5_VABI sceNpCppWebApiUnknown_Vjv_plus_I_minus_aNWFU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Vk-9HMEErLw", sceNpCppWebApiUnknown_Vk_minus_9HMEErLw);
int APS5_VABI sceNpCppWebApiUnknown_Vk_minus_9HMEErLw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VkqilTtSq7U", sceNpCppWebApiUnknown_VkqilTtSq7U);
int APS5_VABI sceNpCppWebApiUnknown_VkqilTtSq7U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Vn4-DjtH6XY", sceNpCppWebApiUnknown_Vn4_minus_DjtH6XY);
int APS5_VABI sceNpCppWebApiUnknown_Vn4_minus_DjtH6XY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Vtl5U83F8Ak", sceNpCppWebApiUnknown_Vtl5U83F8Ak);
int APS5_VABI sceNpCppWebApiUnknown_Vtl5U83F8Ak(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VuKbx6zlEG4", sceNpCppWebApiUnknown_VuKbx6zlEG4);
int APS5_VABI sceNpCppWebApiUnknown_VuKbx6zlEG4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("VwtsvzMN7Jk", sceNpCppWebApiUnknown_VwtsvzMN7Jk);
int APS5_VABI sceNpCppWebApiUnknown_VwtsvzMN7Jk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("W+-RA2Vn-cc", sceNpCppWebApiUnknown_W_plus__minus_RA2Vn_minus_cc);
int APS5_VABI sceNpCppWebApiUnknown_W_plus__minus_RA2Vn_minus_cc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("W+uipM8JBWk", sceNpCppWebApiUnknown_W_plus_uipM8JBWk);
int APS5_VABI sceNpCppWebApiUnknown_W_plus_uipM8JBWk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("W0j6vCxh9Pc", sceNpCppWebApiUnknown_W0j6vCxh9Pc);
int APS5_VABI sceNpCppWebApiUnknown_W0j6vCxh9Pc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("W12G2P+s074", sceNpCppWebApiUnknown_W12G2P_plus_s074);
int APS5_VABI sceNpCppWebApiUnknown_W12G2P_plus_s074(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WAKEa6sKXJM", sceNpCppWebApiUnknown_WAKEa6sKXJM);
int APS5_VABI sceNpCppWebApiUnknown_WAKEa6sKXJM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WCGKU7ZOFU4", sceNpCppWebApiUnknown_WCGKU7ZOFU4);
int APS5_VABI sceNpCppWebApiUnknown_WCGKU7ZOFU4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WDXPNP2syeA", sceNpCppWebApiUnknown_WDXPNP2syeA);
int APS5_VABI sceNpCppWebApiUnknown_WDXPNP2syeA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WDjBY4VN+Pk", sceNpCppWebApiUnknown_WDjBY4VN_plus_Pk);
int APS5_VABI sceNpCppWebApiUnknown_WDjBY4VN_plus_Pk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WEEilwQPEtQ", sceNpCppWebApiUnknown_WEEilwQPEtQ);
int APS5_VABI sceNpCppWebApiUnknown_WEEilwQPEtQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WGKQ359T744", sceNpCppWebApiUnknown_WGKQ359T744);
int APS5_VABI sceNpCppWebApiUnknown_WGKQ359T744(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WGQ1H+AetkE", sceNpCppWebApiUnknown_WGQ1H_plus_AetkE);
int APS5_VABI sceNpCppWebApiUnknown_WGQ1H_plus_AetkE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WH6fUUj9cAQ", sceNpCppWebApiUnknown_WH6fUUj9cAQ);
int APS5_VABI sceNpCppWebApiUnknown_WH6fUUj9cAQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WJZ8vQtZvGs", sceNpCppWebApiUnknown_WJZ8vQtZvGs);
int APS5_VABI sceNpCppWebApiUnknown_WJZ8vQtZvGs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WN8MUUVljFU", sceNpCppWebApiUnknown_WN8MUUVljFU);
int APS5_VABI sceNpCppWebApiUnknown_WN8MUUVljFU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WNK0UpGK8Ws", sceNpCppWebApiUnknown_WNK0UpGK8Ws);
int APS5_VABI sceNpCppWebApiUnknown_WNK0UpGK8Ws(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WOacvEO6fhk", sceNpCppWebApiUnknown_WOacvEO6fhk);
int APS5_VABI sceNpCppWebApiUnknown_WOacvEO6fhk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WPvvEKLEX2c", sceNpCppWebApiUnknown_WPvvEKLEX2c);
int APS5_VABI sceNpCppWebApiUnknown_WPvvEKLEX2c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WRVY82uPSvU", sceNpCppWebApiUnknown_WRVY82uPSvU);
int APS5_VABI sceNpCppWebApiUnknown_WRVY82uPSvU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WSOuge5IsCg", sceNpCppWebApiUnknown_WSOuge5IsCg);
int APS5_VABI sceNpCppWebApiUnknown_WSOuge5IsCg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WTtYf+cNnXI", sceNpCppWebApiUnknown_WTtYf_plus_cNnXI);
int APS5_VABI sceNpCppWebApiUnknown_WTtYf_plus_cNnXI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WU4eHoqbTKg", sceNpCppWebApiUnknown_WU4eHoqbTKg);
int APS5_VABI sceNpCppWebApiUnknown_WU4eHoqbTKg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WV9y653hEmc", sceNpCppWebApiUnknown_WV9y653hEmc);
int APS5_VABI sceNpCppWebApiUnknown_WV9y653hEmc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WX0zzRD99Y4", sceNpCppWebApiUnknown_WX0zzRD99Y4);
int APS5_VABI sceNpCppWebApiUnknown_WX0zzRD99Y4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WYdsQyln9-g", sceNpCppWebApiUnknown_WYdsQyln9_minus_g);
int APS5_VABI sceNpCppWebApiUnknown_WYdsQyln9_minus_g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Wd-pB5LIO+k", sceNpCppWebApiUnknown_Wd_minus_pB5LIO_plus_k);
int APS5_VABI sceNpCppWebApiUnknown_Wd_minus_pB5LIO_plus_k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WdG0bfPQ7Zo", sceNpCppWebApiUnknown_WdG0bfPQ7Zo);
int APS5_VABI sceNpCppWebApiUnknown_WdG0bfPQ7Zo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("We01BOPaiBk", sceNpCppWebApiUnknown_We01BOPaiBk);
int APS5_VABI sceNpCppWebApiUnknown_We01BOPaiBk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Wf8UPh4tq1g", sceNpCppWebApiUnknown_Wf8UPh4tq1g);
int APS5_VABI sceNpCppWebApiUnknown_Wf8UPh4tq1g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Wgw2LeWBk4A", sceNpCppWebApiUnknown_Wgw2LeWBk4A);
int APS5_VABI sceNpCppWebApiUnknown_Wgw2LeWBk4A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Wh2-BPMYsmE", sceNpCppWebApiUnknown_Wh2_minus_BPMYsmE);
int APS5_VABI sceNpCppWebApiUnknown_Wh2_minus_BPMYsmE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WiLSvVuUcpE", sceNpCppWebApiUnknown_WiLSvVuUcpE);
int APS5_VABI sceNpCppWebApiUnknown_WiLSvVuUcpE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WiLcZIhhlio", sceNpCppWebApiUnknown_WiLcZIhhlio);
int APS5_VABI sceNpCppWebApiUnknown_WiLcZIhhlio(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WlfyBqgFeQs", sceNpCppWebApiUnknown_WlfyBqgFeQs);
int APS5_VABI sceNpCppWebApiUnknown_WlfyBqgFeQs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WmWLkS27FxE", sceNpCppWebApiUnknown_WmWLkS27FxE);
int APS5_VABI sceNpCppWebApiUnknown_WmWLkS27FxE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WpAliLBlSKU", sceNpCppWebApiUnknown_WpAliLBlSKU);
int APS5_VABI sceNpCppWebApiUnknown_WpAliLBlSKU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WuQAWT8kYek", sceNpCppWebApiUnknown_WuQAWT8kYek);
int APS5_VABI sceNpCppWebApiUnknown_WuQAWT8kYek(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Wv1Rz8Hw1Iw", sceNpCppWebApiUnknown_Wv1Rz8Hw1Iw);
int APS5_VABI sceNpCppWebApiUnknown_Wv1Rz8Hw1Iw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("WvzIMvjCp0g", sceNpCppWebApiUnknown_WvzIMvjCp0g);
int APS5_VABI sceNpCppWebApiUnknown_WvzIMvjCp0g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Wz7FeYPE0UQ", sceNpCppWebApiUnknown_Wz7FeYPE0UQ);
int APS5_VABI sceNpCppWebApiUnknown_Wz7FeYPE0UQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("X1JE3HkJST8", sceNpCppWebApiUnknown_X1JE3HkJST8);
int APS5_VABI sceNpCppWebApiUnknown_X1JE3HkJST8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("X1SYe8TIGOc", sceNpCppWebApiUnknown_X1SYe8TIGOc);
int APS5_VABI sceNpCppWebApiUnknown_X1SYe8TIGOc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("X2lVKLlddME", sceNpCppWebApiUnknown_X2lVKLlddME);
int APS5_VABI sceNpCppWebApiUnknown_X2lVKLlddME(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("X4MYzukPc3g", sceNpCppWebApiUnknown_X4MYzukPc3g);
int APS5_VABI sceNpCppWebApiUnknown_X4MYzukPc3g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("X7294x828J0", sceNpCppWebApiUnknown_X7294x828J0);
int APS5_VABI sceNpCppWebApiUnknown_X7294x828J0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XCtf+23QT0k", sceNpCppWebApiUnknown_XCtf_plus_23QT0k);
int APS5_VABI sceNpCppWebApiUnknown_XCtf_plus_23QT0k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XFQNeE+EwJU", sceNpCppWebApiUnknown_XFQNeE_plus_EwJU);
int APS5_VABI sceNpCppWebApiUnknown_XFQNeE_plus_EwJU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XHnm54FAso4", sceNpCppWebApiUnknown_XHnm54FAso4);
int APS5_VABI sceNpCppWebApiUnknown_XHnm54FAso4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XJWSy+HoWws", sceNpCppWebApiUnknown_XJWSy_plus_HoWws);
int APS5_VABI sceNpCppWebApiUnknown_XJWSy_plus_HoWws(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XLDdP3+ELyM", sceNpCppWebApiUnknown_XLDdP3_plus_ELyM);
int APS5_VABI sceNpCppWebApiUnknown_XLDdP3_plus_ELyM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XN2v+nFqQLk", sceNpCppWebApiUnknown_XN2v_plus_nFqQLk);
int APS5_VABI sceNpCppWebApiUnknown_XN2v_plus_nFqQLk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XPkpjN985E0", sceNpCppWebApiUnknown_XPkpjN985E0);
int APS5_VABI sceNpCppWebApiUnknown_XPkpjN985E0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XPxNYsOPUkc", sceNpCppWebApiUnknown_XPxNYsOPUkc);
int APS5_VABI sceNpCppWebApiUnknown_XPxNYsOPUkc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XRWDGSuaBfs", sceNpCppWebApiUnknown_XRWDGSuaBfs);
int APS5_VABI sceNpCppWebApiUnknown_XRWDGSuaBfs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XUeWyaje50Q", sceNpCppWebApiUnknown_XUeWyaje50Q);
int APS5_VABI sceNpCppWebApiUnknown_XUeWyaje50Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XWJpJef97fM", sceNpCppWebApiUnknown_XWJpJef97fM);
int APS5_VABI sceNpCppWebApiUnknown_XWJpJef97fM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Xa2lDjDv3L4", sceNpCppWebApiUnknown_Xa2lDjDv3L4);
int APS5_VABI sceNpCppWebApiUnknown_Xa2lDjDv3L4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XarIgT72mQQ", sceNpCppWebApiUnknown_XarIgT72mQQ);
int APS5_VABI sceNpCppWebApiUnknown_XarIgT72mQQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Xb+6t+80cjc", sceNpCppWebApiUnknown_Xb_plus_6t_plus_80cjc);
int APS5_VABI sceNpCppWebApiUnknown_Xb_plus_6t_plus_80cjc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Xc-SooizcFA", sceNpCppWebApiUnknown_Xc_minus_SooizcFA);
int APS5_VABI sceNpCppWebApiUnknown_Xc_minus_SooizcFA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Xd6iEm8Qc84", sceNpCppWebApiUnknown_Xd6iEm8Qc84);
int APS5_VABI sceNpCppWebApiUnknown_Xd6iEm8Qc84(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Xd9wjes-HxU", sceNpCppWebApiUnknown_Xd9wjes_minus_HxU);
int APS5_VABI sceNpCppWebApiUnknown_Xd9wjes_minus_HxU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XdbxA93DS90", sceNpCppWebApiUnknown_XdbxA93DS90);
int APS5_VABI sceNpCppWebApiUnknown_XdbxA93DS90(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XfjRijeQEDo", sceNpCppWebApiUnknown_XfjRijeQEDo);
int APS5_VABI sceNpCppWebApiUnknown_XfjRijeQEDo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Xh5o-FMSmeU", sceNpCppWebApiUnknown_Xh5o_minus_FMSmeU);
int APS5_VABI sceNpCppWebApiUnknown_Xh5o_minus_FMSmeU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XhIcun3hgAo", sceNpCppWebApiUnknown_XhIcun3hgAo);
int APS5_VABI sceNpCppWebApiUnknown_XhIcun3hgAo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Xjm04fAF6Kw", sceNpCppWebApiUnknown_Xjm04fAF6Kw);
int APS5_VABI sceNpCppWebApiUnknown_Xjm04fAF6Kw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XkdGinZ2HtA", sceNpCppWebApiUnknown_XkdGinZ2HtA);
int APS5_VABI sceNpCppWebApiUnknown_XkdGinZ2HtA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Xq-kDNoE6tQ", sceNpCppWebApiUnknown_Xq_minus_kDNoE6tQ);
int APS5_VABI sceNpCppWebApiUnknown_Xq_minus_kDNoE6tQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XtFyTKi22Sc", sceNpCppWebApiUnknown_XtFyTKi22Sc);
int APS5_VABI sceNpCppWebApiUnknown_XtFyTKi22Sc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XtOl9KAhjh0", sceNpCppWebApiUnknown_XtOl9KAhjh0);
int APS5_VABI sceNpCppWebApiUnknown_XtOl9KAhjh0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XuVY56j7GM0", sceNpCppWebApiUnknown_XuVY56j7GM0);
int APS5_VABI sceNpCppWebApiUnknown_XuVY56j7GM0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XvfHZol5M80", sceNpCppWebApiUnknown_XvfHZol5M80);
int APS5_VABI sceNpCppWebApiUnknown_XvfHZol5M80(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("XypLQD+W1HM", sceNpCppWebApiUnknown_XypLQD_plus_W1HM);
int APS5_VABI sceNpCppWebApiUnknown_XypLQD_plus_W1HM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Xzo2Nq9GflA", sceNpCppWebApiUnknown_Xzo2Nq9GflA);
int APS5_VABI sceNpCppWebApiUnknown_Xzo2Nq9GflA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Xztq5XmEJeA", sceNpCppWebApiUnknown_Xztq5XmEJeA);
int APS5_VABI sceNpCppWebApiUnknown_Xztq5XmEJeA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y+pTVRiUcHo", sceNpCppWebApiUnknown_Y_plus_pTVRiUcHo);
int APS5_VABI sceNpCppWebApiUnknown_Y_plus_pTVRiUcHo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y+u-l+LxGIs", sceNpCppWebApiUnknown_Y_plus_u_minus_l_plus_LxGIs);
int APS5_VABI sceNpCppWebApiUnknown_Y_plus_u_minus_l_plus_LxGIs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y-BeRt6hfUg", sceNpCppWebApiUnknown_Y_minus_BeRt6hfUg);
int APS5_VABI sceNpCppWebApiUnknown_Y_minus_BeRt6hfUg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y0LQ0UrxiEs", sceNpCppWebApiUnknown_Y0LQ0UrxiEs);
int APS5_VABI sceNpCppWebApiUnknown_Y0LQ0UrxiEs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y295ygEccqk", sceNpCppWebApiUnknown_Y295ygEccqk);
int APS5_VABI sceNpCppWebApiUnknown_Y295ygEccqk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y2AJknsUg1g", sceNpCppWebApiUnknown_Y2AJknsUg1g);
int APS5_VABI sceNpCppWebApiUnknown_Y2AJknsUg1g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y2onZmaIziI", sceNpCppWebApiUnknown_Y2onZmaIziI);
int APS5_VABI sceNpCppWebApiUnknown_Y2onZmaIziI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y3CFlALyKU8", sceNpCppWebApiUnknown_Y3CFlALyKU8);
int APS5_VABI sceNpCppWebApiUnknown_Y3CFlALyKU8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y3EHXCmjgAE", sceNpCppWebApiUnknown_Y3EHXCmjgAE);
int APS5_VABI sceNpCppWebApiUnknown_Y3EHXCmjgAE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y3GaShDncwM", sceNpCppWebApiUnknown_Y3GaShDncwM);
int APS5_VABI sceNpCppWebApiUnknown_Y3GaShDncwM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y4DduTtrY60", sceNpCppWebApiUnknown_Y4DduTtrY60);
int APS5_VABI sceNpCppWebApiUnknown_Y4DduTtrY60(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y4p7dZLQgDQ", sceNpCppWebApiUnknown_Y4p7dZLQgDQ);
int APS5_VABI sceNpCppWebApiUnknown_Y4p7dZLQgDQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Y5+ET6w8iVg", sceNpCppWebApiUnknown_Y5_plus_ET6w8iVg);
int APS5_VABI sceNpCppWebApiUnknown_Y5_plus_ET6w8iVg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YAtVGPtThx8", sceNpCppWebApiUnknown_YAtVGPtThx8);
int APS5_VABI sceNpCppWebApiUnknown_YAtVGPtThx8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YBGAMvRfRHM", sceNpCppWebApiUnknown_YBGAMvRfRHM);
int APS5_VABI sceNpCppWebApiUnknown_YBGAMvRfRHM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YBdB17og2MY", sceNpCppWebApiUnknown_YBdB17og2MY);
int APS5_VABI sceNpCppWebApiUnknown_YBdB17og2MY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YDpM-vQ8XDI", sceNpCppWebApiUnknown_YDpM_minus_vQ8XDI);
int APS5_VABI sceNpCppWebApiUnknown_YDpM_minus_vQ8XDI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YHYfyQXdOLc", sceNpCppWebApiUnknown_YHYfyQXdOLc);
int APS5_VABI sceNpCppWebApiUnknown_YHYfyQXdOLc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YKLLU7aElnk", sceNpCppWebApiUnknown_YKLLU7aElnk);
int APS5_VABI sceNpCppWebApiUnknown_YKLLU7aElnk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YKVixVJQoXU", sceNpCppWebApiUnknown_YKVixVJQoXU);
int APS5_VABI sceNpCppWebApiUnknown_YKVixVJQoXU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YL917dn7tuI", sceNpCppWebApiUnknown_YL917dn7tuI);
int APS5_VABI sceNpCppWebApiUnknown_YL917dn7tuI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YNjmVsr47so", sceNpCppWebApiUnknown_YNjmVsr47so);
int APS5_VABI sceNpCppWebApiUnknown_YNjmVsr47so(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YRdnLdRUBbY", sceNpCppWebApiUnknown_YRdnLdRUBbY);
int APS5_VABI sceNpCppWebApiUnknown_YRdnLdRUBbY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YS045R-CDdU", sceNpCppWebApiUnknown_YS045R_minus_CDdU);
int APS5_VABI sceNpCppWebApiUnknown_YS045R_minus_CDdU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YXcNoxF0DcA", sceNpCppWebApiUnknown_YXcNoxF0DcA);
int APS5_VABI sceNpCppWebApiUnknown_YXcNoxF0DcA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YZ-6im6M-qw", sceNpCppWebApiUnknown_YZ_minus_6im6M_minus_qw);
int APS5_VABI sceNpCppWebApiUnknown_YZ_minus_6im6M_minus_qw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YZlVMCFs-Dw", sceNpCppWebApiUnknown_YZlVMCFs_minus_Dw);
int APS5_VABI sceNpCppWebApiUnknown_YZlVMCFs_minus_Dw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Yc96yUZp-sU", sceNpCppWebApiUnknown_Yc96yUZp_minus_sU);
int APS5_VABI sceNpCppWebApiUnknown_Yc96yUZp_minus_sU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YdOv0ED76ik", sceNpCppWebApiUnknown_YdOv0ED76ik);
int APS5_VABI sceNpCppWebApiUnknown_YdOv0ED76ik(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YsJoje1pipY", sceNpCppWebApiUnknown_YsJoje1pipY);
int APS5_VABI sceNpCppWebApiUnknown_YsJoje1pipY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Yt01YIntxQY", sceNpCppWebApiUnknown_Yt01YIntxQY);
int APS5_VABI sceNpCppWebApiUnknown_Yt01YIntxQY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YvmY5Jf0VYU", sceNpCppWebApiUnknown_YvmY5Jf0VYU);
int APS5_VABI sceNpCppWebApiUnknown_YvmY5Jf0VYU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Yy6rsbLELYw", sceNpCppWebApiUnknown_Yy6rsbLELYw);
int APS5_VABI sceNpCppWebApiUnknown_Yy6rsbLELYw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YzMVqTigiyc", sceNpCppWebApiUnknown_YzMVqTigiyc);
int APS5_VABI sceNpCppWebApiUnknown_YzMVqTigiyc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("YzNkW8mlLgY", sceNpCppWebApiUnknown_YzNkW8mlLgY);
int APS5_VABI sceNpCppWebApiUnknown_YzNkW8mlLgY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Z+Vy+A8-Opw", sceNpCppWebApiUnknown_Z_plus_Vy_plus_A8_minus_Opw);
int APS5_VABI sceNpCppWebApiUnknown_Z_plus_Vy_plus_A8_minus_Opw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Z+zAL-Z6eQk", sceNpCppWebApiUnknown_Z_plus_zAL_minus_Z6eQk);
int APS5_VABI sceNpCppWebApiUnknown_Z_plus_zAL_minus_Z6eQk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Z-IQIsCmGnQ", sceNpCppWebApiUnknown_Z_minus_IQIsCmGnQ);
int APS5_VABI sceNpCppWebApiUnknown_Z_minus_IQIsCmGnQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Z3YhIxkXvF0", sceNpCppWebApiUnknown_Z3YhIxkXvF0);
int APS5_VABI sceNpCppWebApiUnknown_Z3YhIxkXvF0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Z4nTIsAp+QM", sceNpCppWebApiUnknown_Z4nTIsAp_plus_QM);
int APS5_VABI sceNpCppWebApiUnknown_Z4nTIsAp_plus_QM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Z9Q9LzQDXf0", sceNpCppWebApiUnknown_Z9Q9LzQDXf0);
int APS5_VABI sceNpCppWebApiUnknown_Z9Q9LzQDXf0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Z9oXE1jp2n4", sceNpCppWebApiUnknown_Z9oXE1jp2n4);
int APS5_VABI sceNpCppWebApiUnknown_Z9oXE1jp2n4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZAnCRHLGXyI", sceNpCppWebApiUnknown_ZAnCRHLGXyI);
int APS5_VABI sceNpCppWebApiUnknown_ZAnCRHLGXyI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZBgCNsO6GDc", sceNpCppWebApiUnknown_ZBgCNsO6GDc);
int APS5_VABI sceNpCppWebApiUnknown_ZBgCNsO6GDc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZBkm0cyqmsU", sceNpCppWebApiUnknown_ZBkm0cyqmsU);
int APS5_VABI sceNpCppWebApiUnknown_ZBkm0cyqmsU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZCaq-0g5qJQ", sceNpCppWebApiUnknown_ZCaq_minus_0g5qJQ);
int APS5_VABI sceNpCppWebApiUnknown_ZCaq_minus_0g5qJQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZCd6IYoD3Bc", sceNpCppWebApiUnknown_ZCd6IYoD3Bc);
int APS5_VABI sceNpCppWebApiUnknown_ZCd6IYoD3Bc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZDwprZa5o10", sceNpCppWebApiUnknown_ZDwprZa5o10);
int APS5_VABI sceNpCppWebApiUnknown_ZDwprZa5o10(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZF+vvm66HTU", sceNpCppWebApiUnknown_ZF_plus_vvm66HTU);
int APS5_VABI sceNpCppWebApiUnknown_ZF_plus_vvm66HTU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZJSITmcqRH4", sceNpCppWebApiUnknown_ZJSITmcqRH4);
int APS5_VABI sceNpCppWebApiUnknown_ZJSITmcqRH4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZL5Azk2U+XI", sceNpCppWebApiUnknown_ZL5Azk2U_plus_XI);
int APS5_VABI sceNpCppWebApiUnknown_ZL5Azk2U_plus_XI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZL5Znq6YsMY", sceNpCppWebApiUnknown_ZL5Znq6YsMY);
int APS5_VABI sceNpCppWebApiUnknown_ZL5Znq6YsMY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZPUDWmgk5Jo", sceNpCppWebApiUnknown_ZPUDWmgk5Jo);
int APS5_VABI sceNpCppWebApiUnknown_ZPUDWmgk5Jo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZSi7Ar-PFnk", sceNpCppWebApiUnknown_ZSi7Ar_minus_PFnk);
int APS5_VABI sceNpCppWebApiUnknown_ZSi7Ar_minus_PFnk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZTcvy6ei9jA", sceNpCppWebApiUnknown_ZTcvy6ei9jA);
int APS5_VABI sceNpCppWebApiUnknown_ZTcvy6ei9jA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZUn0ZQBDyVQ", sceNpCppWebApiUnknown_ZUn0ZQBDyVQ);
int APS5_VABI sceNpCppWebApiUnknown_ZUn0ZQBDyVQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZVoxbZlKv7M", sceNpCppWebApiUnknown_ZVoxbZlKv7M);
int APS5_VABI sceNpCppWebApiUnknown_ZVoxbZlKv7M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZXKMi2AM4t8", sceNpCppWebApiUnknown_ZXKMi2AM4t8);
int APS5_VABI sceNpCppWebApiUnknown_ZXKMi2AM4t8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZYTehDq4VkA", sceNpCppWebApiUnknown_ZYTehDq4VkA);
int APS5_VABI sceNpCppWebApiUnknown_ZYTehDq4VkA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZYpuc8benBo", sceNpCppWebApiUnknown_ZYpuc8benBo);
int APS5_VABI sceNpCppWebApiUnknown_ZYpuc8benBo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZZpDSZob2Bs", sceNpCppWebApiUnknown_ZZpDSZob2Bs);
int APS5_VABI sceNpCppWebApiUnknown_ZZpDSZob2Bs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZdVlyQI3f-c", sceNpCppWebApiUnknown_ZdVlyQI3f_minus_c);
int APS5_VABI sceNpCppWebApiUnknown_ZdVlyQI3f_minus_c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Zdcq951Fgg0", sceNpCppWebApiUnknown_Zdcq951Fgg0);
int APS5_VABI sceNpCppWebApiUnknown_Zdcq951Fgg0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Zf59eCCIiY0", sceNpCppWebApiUnknown_Zf59eCCIiY0);
int APS5_VABI sceNpCppWebApiUnknown_Zf59eCCIiY0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Zi8G4d9xcDg", sceNpCppWebApiUnknown_Zi8G4d9xcDg);
int APS5_VABI sceNpCppWebApiUnknown_Zi8G4d9xcDg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZiJqTZVDuPM", sceNpCppWebApiUnknown_ZiJqTZVDuPM);
int APS5_VABI sceNpCppWebApiUnknown_ZiJqTZVDuPM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Zj-yNSTs8bE", sceNpCppWebApiUnknown_Zj_minus_yNSTs8bE);
int APS5_VABI sceNpCppWebApiUnknown_Zj_minus_yNSTs8bE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZkSM1nh5zM8", sceNpCppWebApiUnknown_ZkSM1nh5zM8);
int APS5_VABI sceNpCppWebApiUnknown_ZkSM1nh5zM8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Zkxpy0zdodA", sceNpCppWebApiUnknown_Zkxpy0zdodA);
int APS5_VABI sceNpCppWebApiUnknown_Zkxpy0zdodA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZlCkpiN0Zdg", sceNpCppWebApiUnknown_ZlCkpiN0Zdg);
int APS5_VABI sceNpCppWebApiUnknown_ZlCkpiN0Zdg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZmBHJxoLzQQ", sceNpCppWebApiUnknown_ZmBHJxoLzQQ);
int APS5_VABI sceNpCppWebApiUnknown_ZmBHJxoLzQQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Zp0CMnAK+jc", sceNpCppWebApiUnknown_Zp0CMnAK_plus_jc);
int APS5_VABI sceNpCppWebApiUnknown_Zp0CMnAK_plus_jc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("Zpt2xfyeRMU", sceNpCppWebApiUnknown_Zpt2xfyeRMU);
int APS5_VABI sceNpCppWebApiUnknown_Zpt2xfyeRMU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZttIjRBl3Rc", sceNpCppWebApiUnknown_ZttIjRBl3Rc);
int APS5_VABI sceNpCppWebApiUnknown_ZttIjRBl3Rc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZuRinzzVx1M", sceNpCppWebApiUnknown_ZuRinzzVx1M);
int APS5_VABI sceNpCppWebApiUnknown_ZuRinzzVx1M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ZwjmbSr4Cxc", sceNpCppWebApiUnknown_ZwjmbSr4Cxc);
int APS5_VABI sceNpCppWebApiUnknown_ZwjmbSr4Cxc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("a+W7HHlwpBs", sceNpCppWebApiUnknown_a_plus_W7HHlwpBs);
int APS5_VABI sceNpCppWebApiUnknown_a_plus_W7HHlwpBs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("a-DLKQIJzr4", sceNpCppWebApiUnknown_a_minus_DLKQIJzr4);
int APS5_VABI sceNpCppWebApiUnknown_a_minus_DLKQIJzr4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("a-z7wxuYO2E", sceNpCppWebApiUnknown_a_minus_z7wxuYO2E);
int APS5_VABI sceNpCppWebApiUnknown_a_minus_z7wxuYO2E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("a1wn2EnEF8U", sceNpCppWebApiUnknown_a1wn2EnEF8U);
int APS5_VABI sceNpCppWebApiUnknown_a1wn2EnEF8U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("a473OMzUcj8", sceNpCppWebApiUnknown_a473OMzUcj8);
int APS5_VABI sceNpCppWebApiUnknown_a473OMzUcj8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("a4q15LI1a4E", sceNpCppWebApiUnknown_a4q15LI1a4E);
int APS5_VABI sceNpCppWebApiUnknown_a4q15LI1a4E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("a58v7b1bGpg", sceNpCppWebApiUnknown_a58v7b1bGpg);
int APS5_VABI sceNpCppWebApiUnknown_a58v7b1bGpg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("a7sI39vUIu0", sceNpCppWebApiUnknown_a7sI39vUIu0);
int APS5_VABI sceNpCppWebApiUnknown_a7sI39vUIu0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aBuX0PX-T7I", sceNpCppWebApiUnknown_aBuX0PX_minus_T7I);
int APS5_VABI sceNpCppWebApiUnknown_aBuX0PX_minus_T7I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aER-LBHbVbw", sceNpCppWebApiUnknown_aER_minus_LBHbVbw);
int APS5_VABI sceNpCppWebApiUnknown_aER_minus_LBHbVbw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aEdk8vxoGZg", sceNpCppWebApiUnknown_aEdk8vxoGZg);
int APS5_VABI sceNpCppWebApiUnknown_aEdk8vxoGZg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aEt4aNpeLwQ", sceNpCppWebApiUnknown_aEt4aNpeLwQ);
int APS5_VABI sceNpCppWebApiUnknown_aEt4aNpeLwQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aHP5OyUPuvM", sceNpCppWebApiUnknown_aHP5OyUPuvM);
int APS5_VABI sceNpCppWebApiUnknown_aHP5OyUPuvM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aJJy9D41U-4", sceNpCppWebApiUnknown_aJJy9D41U_minus_4);
int APS5_VABI sceNpCppWebApiUnknown_aJJy9D41U_minus_4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aLtk6y5ryVQ", sceNpCppWebApiUnknown_aLtk6y5ryVQ);
int APS5_VABI sceNpCppWebApiUnknown_aLtk6y5ryVQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aMu9uihcJ3w", sceNpCppWebApiUnknown_aMu9uihcJ3w);
int APS5_VABI sceNpCppWebApiUnknown_aMu9uihcJ3w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aMurPhX5fKQ", sceNpCppWebApiUnknown_aMurPhX5fKQ);
int APS5_VABI sceNpCppWebApiUnknown_aMurPhX5fKQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aRjr2VRv2tM", sceNpCppWebApiUnknown_aRjr2VRv2tM);
int APS5_VABI sceNpCppWebApiUnknown_aRjr2VRv2tM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aWLNTDtVUxY", sceNpCppWebApiUnknown_aWLNTDtVUxY);
int APS5_VABI sceNpCppWebApiUnknown_aWLNTDtVUxY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("abngJhUiYJ4", sceNpCppWebApiUnknown_abngJhUiYJ4);
int APS5_VABI sceNpCppWebApiUnknown_abngJhUiYJ4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("adkIhIybMOU", sceNpCppWebApiUnknown_adkIhIybMOU);
int APS5_VABI sceNpCppWebApiUnknown_adkIhIybMOU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aeee7nTH-0U", sceNpCppWebApiUnknown_aeee7nTH_minus_0U);
int APS5_VABI sceNpCppWebApiUnknown_aeee7nTH_minus_0U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aezUahhkO0A", sceNpCppWebApiUnknown_aezUahhkO0A);
int APS5_VABI sceNpCppWebApiUnknown_aezUahhkO0A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ahhmHImMlbs", sceNpCppWebApiUnknown_ahhmHImMlbs);
int APS5_VABI sceNpCppWebApiUnknown_ahhmHImMlbs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("aixppP6tkFs", sceNpCppWebApiUnknown_aixppP6tkFs);
int APS5_VABI sceNpCppWebApiUnknown_aixppP6tkFs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("alTlXracq1k", sceNpCppWebApiUnknown_alTlXracq1k);
int APS5_VABI sceNpCppWebApiUnknown_alTlXracq1k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("anOM+-Ryi+o", sceNpCppWebApiUnknown_anOM_plus__minus_Ryi_plus_o);
int APS5_VABI sceNpCppWebApiUnknown_anOM_plus__minus_Ryi_plus_o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("anSJcjPKG6M", sceNpCppWebApiUnknown_anSJcjPKG6M);
int APS5_VABI sceNpCppWebApiUnknown_anSJcjPKG6M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("apHKv46QaCw", sceNpCppWebApiUnknown_apHKv46QaCw);
int APS5_VABI sceNpCppWebApiUnknown_apHKv46QaCw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("as7GCfRIzhQ", sceNpCppWebApiUnknown_as7GCfRIzhQ);
int APS5_VABI sceNpCppWebApiUnknown_as7GCfRIzhQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("atu1X5r1lok", sceNpCppWebApiUnknown_atu1X5r1lok);
int APS5_VABI sceNpCppWebApiUnknown_atu1X5r1lok(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("auWuyA-JeSQ", sceNpCppWebApiUnknown_auWuyA_minus_JeSQ);
int APS5_VABI sceNpCppWebApiUnknown_auWuyA_minus_JeSQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("avBV-pprLUo", sceNpCppWebApiUnknown_avBV_minus_pprLUo);
int APS5_VABI sceNpCppWebApiUnknown_avBV_minus_pprLUo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("avsgLCaJz0w", sceNpCppWebApiUnknown_avsgLCaJz0w);
int APS5_VABI sceNpCppWebApiUnknown_avsgLCaJz0w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("awTPhlC4368", sceNpCppWebApiUnknown_awTPhlC4368);
int APS5_VABI sceNpCppWebApiUnknown_awTPhlC4368(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("b+v7BB12y5I", sceNpCppWebApiUnknown_b_plus_v7BB12y5I);
int APS5_VABI sceNpCppWebApiUnknown_b_plus_v7BB12y5I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("b-FP2Br9oeE", sceNpCppWebApiUnknown_b_minus_FP2Br9oeE);
int APS5_VABI sceNpCppWebApiUnknown_b_minus_FP2Br9oeE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("b0o51X8BVw4", sceNpCppWebApiUnknown_b0o51X8BVw4);
int APS5_VABI sceNpCppWebApiUnknown_b0o51X8BVw4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("b1MSFaAfkYs", sceNpCppWebApiUnknown_b1MSFaAfkYs);
int APS5_VABI sceNpCppWebApiUnknown_b1MSFaAfkYs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("b4bvSzAJv+o", sceNpCppWebApiUnknown_b4bvSzAJv_plus_o);
int APS5_VABI sceNpCppWebApiUnknown_b4bvSzAJv_plus_o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("b604iIfa1Lc", sceNpCppWebApiUnknown_b604iIfa1Lc);
int APS5_VABI sceNpCppWebApiUnknown_b604iIfa1Lc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bCCKHTeKetw", sceNpCppWebApiUnknown_bCCKHTeKetw);
int APS5_VABI sceNpCppWebApiUnknown_bCCKHTeKetw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bEwJsewIngA", sceNpCppWebApiUnknown_bEwJsewIngA);
int APS5_VABI sceNpCppWebApiUnknown_bEwJsewIngA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bFVvlb12HRo", sceNpCppWebApiUnknown_bFVvlb12HRo);
int APS5_VABI sceNpCppWebApiUnknown_bFVvlb12HRo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bKzqp+Lj3YQ", sceNpCppWebApiUnknown_bKzqp_plus_Lj3YQ);
int APS5_VABI sceNpCppWebApiUnknown_bKzqp_plus_Lj3YQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bLiiGezq8wo", sceNpCppWebApiUnknown_bLiiGezq8wo);
int APS5_VABI sceNpCppWebApiUnknown_bLiiGezq8wo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bOrGTnL22O4", sceNpCppWebApiUnknown_bOrGTnL22O4);
int APS5_VABI sceNpCppWebApiUnknown_bOrGTnL22O4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bPNJIPCmGqI", sceNpCppWebApiUnknown_bPNJIPCmGqI);
int APS5_VABI sceNpCppWebApiUnknown_bPNJIPCmGqI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bPtMdnjinVM", sceNpCppWebApiUnknown_bPtMdnjinVM);
int APS5_VABI sceNpCppWebApiUnknown_bPtMdnjinVM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bTKeeOGemO8", sceNpCppWebApiUnknown_bTKeeOGemO8);
int APS5_VABI sceNpCppWebApiUnknown_bTKeeOGemO8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bTZjr816ME4", sceNpCppWebApiUnknown_bTZjr816ME4);
int APS5_VABI sceNpCppWebApiUnknown_bTZjr816ME4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bTaEyd6uzzE", sceNpCppWebApiUnknown_bTaEyd6uzzE);
int APS5_VABI sceNpCppWebApiUnknown_bTaEyd6uzzE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bVJ-1ExjYco", sceNpCppWebApiUnknown_bVJ_minus_1ExjYco);
int APS5_VABI sceNpCppWebApiUnknown_bVJ_minus_1ExjYco(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bZGh0ikos7A", sceNpCppWebApiUnknown_bZGh0ikos7A);
int APS5_VABI sceNpCppWebApiUnknown_bZGh0ikos7A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("baD+O944dgk", sceNpCppWebApiUnknown_baD_plus_O944dgk);
int APS5_VABI sceNpCppWebApiUnknown_baD_plus_O944dgk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bbc9x3jc5OM", sceNpCppWebApiUnknown_bbc9x3jc5OM);
int APS5_VABI sceNpCppWebApiUnknown_bbc9x3jc5OM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("biWNVs-42is", sceNpCppWebApiUnknown_biWNVs_minus_42is);
int APS5_VABI sceNpCppWebApiUnknown_biWNVs_minus_42is(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("biZCCyH6Iro", sceNpCppWebApiUnknown_biZCCyH6Iro);
int APS5_VABI sceNpCppWebApiUnknown_biZCCyH6Iro(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bmYoW--WCkc", sceNpCppWebApiUnknown_bmYoW_minus__minus_WCkc);
int APS5_VABI sceNpCppWebApiUnknown_bmYoW_minus__minus_WCkc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bmaxU2oOHeg", sceNpCppWebApiUnknown_bmaxU2oOHeg);
int APS5_VABI sceNpCppWebApiUnknown_bmaxU2oOHeg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("boO7DKyXql4", sceNpCppWebApiUnknown_boO7DKyXql4);
int APS5_VABI sceNpCppWebApiUnknown_boO7DKyXql4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bobGynGpRjk", sceNpCppWebApiUnknown_bobGynGpRjk);
int APS5_VABI sceNpCppWebApiUnknown_bobGynGpRjk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bpXMsrvbIho", sceNpCppWebApiUnknown_bpXMsrvbIho);
int APS5_VABI sceNpCppWebApiUnknown_bpXMsrvbIho(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("br2jlJMs1lQ", sceNpCppWebApiUnknown_br2jlJMs1lQ);
int APS5_VABI sceNpCppWebApiUnknown_br2jlJMs1lQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("br6GL-f3CQg", sceNpCppWebApiUnknown_br6GL_minus_f3CQg);
int APS5_VABI sceNpCppWebApiUnknown_br6GL_minus_f3CQg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("bvfjdByaA6Q", sceNpCppWebApiUnknown_bvfjdByaA6Q);
int APS5_VABI sceNpCppWebApiUnknown_bvfjdByaA6Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("c-lHWCnJjrM", sceNpCppWebApiUnknown_c_minus_lHWCnJjrM);
int APS5_VABI sceNpCppWebApiUnknown_c_minus_lHWCnJjrM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("c00bHjdQxuc", sceNpCppWebApiUnknown_c00bHjdQxuc);
int APS5_VABI sceNpCppWebApiUnknown_c00bHjdQxuc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("c3GtMIue0b4", sceNpCppWebApiUnknown_c3GtMIue0b4);
int APS5_VABI sceNpCppWebApiUnknown_c3GtMIue0b4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("c3J9K9XbQqE", sceNpCppWebApiUnknown_c3J9K9XbQqE);
int APS5_VABI sceNpCppWebApiUnknown_c3J9K9XbQqE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("c3rtINcNDaw", sceNpCppWebApiUnknown_c3rtINcNDaw);
int APS5_VABI sceNpCppWebApiUnknown_c3rtINcNDaw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cAckbuvTh9s", sceNpCppWebApiUnknown_cAckbuvTh9s);
int APS5_VABI sceNpCppWebApiUnknown_cAckbuvTh9s(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cEyJk+5DR7g", sceNpCppWebApiUnknown_cEyJk_plus_5DR7g);
int APS5_VABI sceNpCppWebApiUnknown_cEyJk_plus_5DR7g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cG1VE2HMl6c", sceNpCppWebApiUnknown_cG1VE2HMl6c);
int APS5_VABI sceNpCppWebApiUnknown_cG1VE2HMl6c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cG2OXrkSQMQ", sceNpCppWebApiUnknown_cG2OXrkSQMQ);
int APS5_VABI sceNpCppWebApiUnknown_cG2OXrkSQMQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cGzgsNULjbg", sceNpCppWebApiUnknown_cGzgsNULjbg);
int APS5_VABI sceNpCppWebApiUnknown_cGzgsNULjbg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cIGZSzXyQSA", sceNpCppWebApiUnknown_cIGZSzXyQSA);
int APS5_VABI sceNpCppWebApiUnknown_cIGZSzXyQSA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cJ8md8EbvrI", sceNpCppWebApiUnknown_cJ8md8EbvrI);
int APS5_VABI sceNpCppWebApiUnknown_cJ8md8EbvrI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cK6bYHf-Q5E", sceNpCppWebApiUnknown_cK6bYHf_minus_Q5E);
int APS5_VABI sceNpCppWebApiUnknown_cK6bYHf_minus_Q5E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cKqnSGpeRUE", sceNpCppWebApiUnknown_cKqnSGpeRUE);
int APS5_VABI sceNpCppWebApiUnknown_cKqnSGpeRUE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cLn1tFTtp38", sceNpCppWebApiUnknown_cLn1tFTtp38);
int APS5_VABI sceNpCppWebApiUnknown_cLn1tFTtp38(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cMtWfMUFIY8", sceNpCppWebApiUnknown_cMtWfMUFIY8);
int APS5_VABI sceNpCppWebApiUnknown_cMtWfMUFIY8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cPcej3xetd0", sceNpCppWebApiUnknown_cPcej3xetd0);
int APS5_VABI sceNpCppWebApiUnknown_cPcej3xetd0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cQaY7Qre6CA", sceNpCppWebApiUnknown_cQaY7Qre6CA);
int APS5_VABI sceNpCppWebApiUnknown_cQaY7Qre6CA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cQkBH-pXhF0", sceNpCppWebApiUnknown_cQkBH_minus_pXhF0);
int APS5_VABI sceNpCppWebApiUnknown_cQkBH_minus_pXhF0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cRILAEvn+9M", sceNpCppWebApiUnknown_cRILAEvn_plus_9M);
int APS5_VABI sceNpCppWebApiUnknown_cRILAEvn_plus_9M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cTLUQmCfWNQ", sceNpCppWebApiUnknown_cTLUQmCfWNQ);
int APS5_VABI sceNpCppWebApiUnknown_cTLUQmCfWNQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cUw-+DZEy+U", sceNpCppWebApiUnknown_cUw_minus__plus_DZEy_plus_U);
int APS5_VABI sceNpCppWebApiUnknown_cUw_minus__plus_DZEy_plus_U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cVSUblwHP90", sceNpCppWebApiUnknown_cVSUblwHP90);
int APS5_VABI sceNpCppWebApiUnknown_cVSUblwHP90(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cbMKWC77kiE", sceNpCppWebApiUnknown_cbMKWC77kiE);
int APS5_VABI sceNpCppWebApiUnknown_cbMKWC77kiE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cfu8UjoKktY", sceNpCppWebApiUnknown_cfu8UjoKktY);
int APS5_VABI sceNpCppWebApiUnknown_cfu8UjoKktY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ch-tDZCG1+k", sceNpCppWebApiUnknown_ch_minus_tDZCG1_plus_k);
int APS5_VABI sceNpCppWebApiUnknown_ch_minus_tDZCG1_plus_k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ci7j9YWIWNo", sceNpCppWebApiUnknown_ci7j9YWIWNo);
int APS5_VABI sceNpCppWebApiUnknown_ci7j9YWIWNo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cjZEuzHkgng", sceNpCppWebApiUnknown_cjZEuzHkgng);
int APS5_VABI sceNpCppWebApiUnknown_cjZEuzHkgng(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ckpI53I9ZWw", sceNpCppWebApiUnknown_ckpI53I9ZWw);
int APS5_VABI sceNpCppWebApiUnknown_ckpI53I9ZWw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cltvY2tDiVM", sceNpCppWebApiUnknown_cltvY2tDiVM);
int APS5_VABI sceNpCppWebApiUnknown_cltvY2tDiVM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cmVpl3RY18M", sceNpCppWebApiUnknown_cmVpl3RY18M);
int APS5_VABI sceNpCppWebApiUnknown_cmVpl3RY18M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cokIsGCANB0", sceNpCppWebApiUnknown_cokIsGCANB0);
int APS5_VABI sceNpCppWebApiUnknown_cokIsGCANB0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cp82JRS8smw", sceNpCppWebApiUnknown_cp82JRS8smw);
int APS5_VABI sceNpCppWebApiUnknown_cp82JRS8smw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cpCOXWMgha0", sceNpCppWebApiUnknown_cpCOXWMgha0);
int APS5_VABI sceNpCppWebApiUnknown_cpCOXWMgha0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cq65J5rCEwE", sceNpCppWebApiUnknown_cq65J5rCEwE);
int APS5_VABI sceNpCppWebApiUnknown_cq65J5rCEwE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("crYcq1HKdq8", sceNpCppWebApiUnknown_crYcq1HKdq8);
int APS5_VABI sceNpCppWebApiUnknown_crYcq1HKdq8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("crZ66tvCbo8", sceNpCppWebApiUnknown_crZ66tvCbo8);
int APS5_VABI sceNpCppWebApiUnknown_crZ66tvCbo8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ctghP7Sf6+c", sceNpCppWebApiUnknown_ctghP7Sf6_plus_c);
int APS5_VABI sceNpCppWebApiUnknown_ctghP7Sf6_plus_c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cwj+Px7cp0g", sceNpCppWebApiUnknown_cwj_plus_Px7cp0g);
int APS5_VABI sceNpCppWebApiUnknown_cwj_plus_Px7cp0g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cxaFAuLyJcU", sceNpCppWebApiUnknown_cxaFAuLyJcU);
int APS5_VABI sceNpCppWebApiUnknown_cxaFAuLyJcU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cxh9Tu8zj+0", sceNpCppWebApiUnknown_cxh9Tu8zj_plus_0);
int APS5_VABI sceNpCppWebApiUnknown_cxh9Tu8zj_plus_0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cy2kWghrw-I", sceNpCppWebApiUnknown_cy2kWghrw_minus_I);
int APS5_VABI sceNpCppWebApiUnknown_cy2kWghrw_minus_I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("cyWEjTH6na8", sceNpCppWebApiUnknown_cyWEjTH6na8);
int APS5_VABI sceNpCppWebApiUnknown_cyWEjTH6na8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("czReUjajRN4", sceNpCppWebApiUnknown_czReUjajRN4);
int APS5_VABI sceNpCppWebApiUnknown_czReUjajRN4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("d0Dclzb-DnI", sceNpCppWebApiUnknown_d0Dclzb_minus_DnI);
int APS5_VABI sceNpCppWebApiUnknown_d0Dclzb_minus_DnI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("d1eEjWR60wk", sceNpCppWebApiUnknown_d1eEjWR60wk);
int APS5_VABI sceNpCppWebApiUnknown_d1eEjWR60wk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("d2JuGu+UcU8", sceNpCppWebApiUnknown_d2JuGu_plus_UcU8);
int APS5_VABI sceNpCppWebApiUnknown_d2JuGu_plus_UcU8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("d3BbxfGmStE", sceNpCppWebApiUnknown_d3BbxfGmStE);
int APS5_VABI sceNpCppWebApiUnknown_d3BbxfGmStE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("d3DH-QgjgR8", sceNpCppWebApiUnknown_d3DH_minus_QgjgR8);
int APS5_VABI sceNpCppWebApiUnknown_d3DH_minus_QgjgR8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("d5xkh7fa3mU", sceNpCppWebApiUnknown_d5xkh7fa3mU);
int APS5_VABI sceNpCppWebApiUnknown_d5xkh7fa3mU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("d7cla40ORx0", sceNpCppWebApiUnknown_d7cla40ORx0);
int APS5_VABI sceNpCppWebApiUnknown_d7cla40ORx0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("d9N3khVxERQ", sceNpCppWebApiUnknown_d9N3khVxERQ);
int APS5_VABI sceNpCppWebApiUnknown_d9N3khVxERQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dA0BfgUC88k", sceNpCppWebApiUnknown_dA0BfgUC88k);
int APS5_VABI sceNpCppWebApiUnknown_dA0BfgUC88k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dAGTHaOuY20", sceNpCppWebApiUnknown_dAGTHaOuY20);
int APS5_VABI sceNpCppWebApiUnknown_dAGTHaOuY20(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dASQMnaMR1E", sceNpCppWebApiUnknown_dASQMnaMR1E);
int APS5_VABI sceNpCppWebApiUnknown_dASQMnaMR1E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dE2PoJBF6Bg", sceNpCppWebApiUnknown_dE2PoJBF6Bg);
int APS5_VABI sceNpCppWebApiUnknown_dE2PoJBF6Bg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dEdd7MSM2Mc", sceNpCppWebApiUnknown_dEdd7MSM2Mc);
int APS5_VABI sceNpCppWebApiUnknown_dEdd7MSM2Mc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dGYo9mE8K2A", sceNpCppWebApiUnknown_dGYo9mE8K2A);
int APS5_VABI sceNpCppWebApiUnknown_dGYo9mE8K2A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dIFd5OXQYtU", sceNpCppWebApiUnknown_dIFd5OXQYtU);
int APS5_VABI sceNpCppWebApiUnknown_dIFd5OXQYtU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dKUBHYQlhMw", sceNpCppWebApiUnknown_dKUBHYQlhMw);
int APS5_VABI sceNpCppWebApiUnknown_dKUBHYQlhMw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dPdi6E-j9mE", sceNpCppWebApiUnknown_dPdi6E_minus_j9mE);
int APS5_VABI sceNpCppWebApiUnknown_dPdi6E_minus_j9mE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dQFPWRSreYc", sceNpCppWebApiUnknown_dQFPWRSreYc);
int APS5_VABI sceNpCppWebApiUnknown_dQFPWRSreYc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dU+6TbijWus", sceNpCppWebApiUnknown_dU_plus_6TbijWus);
int APS5_VABI sceNpCppWebApiUnknown_dU_plus_6TbijWus(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dVCh5JiPsSY", sceNpCppWebApiUnknown_dVCh5JiPsSY);
int APS5_VABI sceNpCppWebApiUnknown_dVCh5JiPsSY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dY6vQe8+YfA", sceNpCppWebApiUnknown_dY6vQe8_plus_YfA);
int APS5_VABI sceNpCppWebApiUnknown_dY6vQe8_plus_YfA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dYh6MqCHi0w", sceNpCppWebApiUnknown_dYh6MqCHi0w);
int APS5_VABI sceNpCppWebApiUnknown_dYh6MqCHi0w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dZAozwzECsU", sceNpCppWebApiUnknown_dZAozwzECsU);
int APS5_VABI sceNpCppWebApiUnknown_dZAozwzECsU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dcbbhKIZCDA", sceNpCppWebApiUnknown_dcbbhKIZCDA);
int APS5_VABI sceNpCppWebApiUnknown_dcbbhKIZCDA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("deJgtP9hduc", sceNpCppWebApiUnknown_deJgtP9hduc);
int APS5_VABI sceNpCppWebApiUnknown_deJgtP9hduc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dgLPhYb+SDc", sceNpCppWebApiUnknown_dgLPhYb_plus_SDc);
int APS5_VABI sceNpCppWebApiUnknown_dgLPhYb_plus_SDc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dhWQF5yRpr8", sceNpCppWebApiUnknown_dhWQF5yRpr8);
int APS5_VABI sceNpCppWebApiUnknown_dhWQF5yRpr8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("djbNspswbKc", sceNpCppWebApiUnknown_djbNspswbKc);
int APS5_VABI sceNpCppWebApiUnknown_djbNspswbKc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dpPikaYGTPg", sceNpCppWebApiUnknown_dpPikaYGTPg);
int APS5_VABI sceNpCppWebApiUnknown_dpPikaYGTPg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dqGIvYiZAso", sceNpCppWebApiUnknown_dqGIvYiZAso);
int APS5_VABI sceNpCppWebApiUnknown_dqGIvYiZAso(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dtNkfThj-uk", sceNpCppWebApiUnknown_dtNkfThj_minus_uk);
int APS5_VABI sceNpCppWebApiUnknown_dtNkfThj_minus_uk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("du6QAD7G1Uc", sceNpCppWebApiUnknown_du6QAD7G1Uc);
int APS5_VABI sceNpCppWebApiUnknown_du6QAD7G1Uc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("dv8KUvfjc8c", sceNpCppWebApiUnknown_dv8KUvfjc8c);
int APS5_VABI sceNpCppWebApiUnknown_dv8KUvfjc8c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("e0n+XvGVWjY", sceNpCppWebApiUnknown_e0n_plus_XvGVWjY);
int APS5_VABI sceNpCppWebApiUnknown_e0n_plus_XvGVWjY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("e0oVUnvqg38", sceNpCppWebApiUnknown_e0oVUnvqg38);
int APS5_VABI sceNpCppWebApiUnknown_e0oVUnvqg38(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("e7CbETQO+bo", sceNpCppWebApiUnknown_e7CbETQO_plus_bo);
int APS5_VABI sceNpCppWebApiUnknown_e7CbETQO_plus_bo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eARplY-I3B8", sceNpCppWebApiUnknown_eARplY_minus_I3B8);
int APS5_VABI sceNpCppWebApiUnknown_eARplY_minus_I3B8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eAg1RVhw5wA", sceNpCppWebApiUnknown_eAg1RVhw5wA);
int APS5_VABI sceNpCppWebApiUnknown_eAg1RVhw5wA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eBuNgAm7zgY", sceNpCppWebApiUnknown_eBuNgAm7zgY);
int APS5_VABI sceNpCppWebApiUnknown_eBuNgAm7zgY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eD0Heap9FkI", sceNpCppWebApiUnknown_eD0Heap9FkI);
int APS5_VABI sceNpCppWebApiUnknown_eD0Heap9FkI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eFEGeSrXMjE", sceNpCppWebApiUnknown_eFEGeSrXMjE);
int APS5_VABI sceNpCppWebApiUnknown_eFEGeSrXMjE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eGRZTvsrK1Q", sceNpCppWebApiUnknown_eGRZTvsrK1Q);
int APS5_VABI sceNpCppWebApiUnknown_eGRZTvsrK1Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eHckgiQ4UwY", sceNpCppWebApiUnknown_eHckgiQ4UwY);
int APS5_VABI sceNpCppWebApiUnknown_eHckgiQ4UwY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eN81KmJlGOE", sceNpCppWebApiUnknown_eN81KmJlGOE);
int APS5_VABI sceNpCppWebApiUnknown_eN81KmJlGOE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eNxbfBk88VI", sceNpCppWebApiUnknown_eNxbfBk88VI);
int APS5_VABI sceNpCppWebApiUnknown_eNxbfBk88VI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eQCNHjia30c", sceNpCppWebApiUnknown_eQCNHjia30c);
int APS5_VABI sceNpCppWebApiUnknown_eQCNHjia30c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eT4TQB7OsLA", sceNpCppWebApiUnknown_eT4TQB7OsLA);
int APS5_VABI sceNpCppWebApiUnknown_eT4TQB7OsLA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eVU3cXIJz0w", sceNpCppWebApiUnknown_eVU3cXIJz0w);
int APS5_VABI sceNpCppWebApiUnknown_eVU3cXIJz0w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("edOjjRsxL1o", sceNpCppWebApiUnknown_edOjjRsxL1o);
int APS5_VABI sceNpCppWebApiUnknown_edOjjRsxL1o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("egm8uuznnW4", sceNpCppWebApiUnknown_egm8uuznnW4);
int APS5_VABI sceNpCppWebApiUnknown_egm8uuznnW4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ehMuSPAqlVQ", sceNpCppWebApiUnknown_ehMuSPAqlVQ);
int APS5_VABI sceNpCppWebApiUnknown_ehMuSPAqlVQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ehhooxUKFJ8", sceNpCppWebApiUnknown_ehhooxUKFJ8);
int APS5_VABI sceNpCppWebApiUnknown_ehhooxUKFJ8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eiWoUXnutss", sceNpCppWebApiUnknown_eiWoUXnutss);
int APS5_VABI sceNpCppWebApiUnknown_eiWoUXnutss(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("enuMll33T24", sceNpCppWebApiUnknown_enuMll33T24);
int APS5_VABI sceNpCppWebApiUnknown_enuMll33T24(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("epJ6x2LV0kU", sceNpCppWebApiUnknown_epJ6x2LV0kU);
int APS5_VABI sceNpCppWebApiUnknown_epJ6x2LV0kU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("epwr+cBCIFs", sceNpCppWebApiUnknown_epwr_plus_cBCIFs);
int APS5_VABI sceNpCppWebApiUnknown_epwr_plus_cBCIFs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eu3WBBxzdp0", sceNpCppWebApiUnknown_eu3WBBxzdp0);
int APS5_VABI sceNpCppWebApiUnknown_eu3WBBxzdp0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("euQ2PLd-LNQ", sceNpCppWebApiUnknown_euQ2PLd_minus_LNQ);
int APS5_VABI sceNpCppWebApiUnknown_euQ2PLd_minus_LNQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("exkGQSIrF+U", sceNpCppWebApiUnknown_exkGQSIrF_plus_U);
int APS5_VABI sceNpCppWebApiUnknown_exkGQSIrF_plus_U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("eyiQGHPBIKI", sceNpCppWebApiUnknown_eyiQGHPBIKI);
int APS5_VABI sceNpCppWebApiUnknown_eyiQGHPBIKI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("f-oj+Yb47R8", sceNpCppWebApiUnknown_f_minus_oj_plus_Yb47R8);
int APS5_VABI sceNpCppWebApiUnknown_f_minus_oj_plus_Yb47R8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("f-ssShWqQnM", sceNpCppWebApiUnknown_f_minus_ssShWqQnM);
int APS5_VABI sceNpCppWebApiUnknown_f_minus_ssShWqQnM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("f2OmNBGuemI", sceNpCppWebApiUnknown_f2OmNBGuemI);
int APS5_VABI sceNpCppWebApiUnknown_f2OmNBGuemI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("f492Zxg0008", sceNpCppWebApiUnknown_f492Zxg0008);
int APS5_VABI sceNpCppWebApiUnknown_f492Zxg0008(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("f4gEI1xHQvE", sceNpCppWebApiUnknown_f4gEI1xHQvE);
int APS5_VABI sceNpCppWebApiUnknown_f4gEI1xHQvE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("f5Dl1OQppW0", sceNpCppWebApiUnknown_f5Dl1OQppW0);
int APS5_VABI sceNpCppWebApiUnknown_f5Dl1OQppW0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("f69FYKsbeMI", sceNpCppWebApiUnknown_f69FYKsbeMI);
int APS5_VABI sceNpCppWebApiUnknown_f69FYKsbeMI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("f8+4s7hHlCo", sceNpCppWebApiUnknown_f8_plus_4s7hHlCo);
int APS5_VABI sceNpCppWebApiUnknown_f8_plus_4s7hHlCo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("f9jTmCiIUVA", sceNpCppWebApiUnknown_f9jTmCiIUVA);
int APS5_VABI sceNpCppWebApiUnknown_f9jTmCiIUVA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("f9yKvvilry4", sceNpCppWebApiUnknown_f9yKvvilry4);
int APS5_VABI sceNpCppWebApiUnknown_f9yKvvilry4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fDex2lyDL+I", sceNpCppWebApiUnknown_fDex2lyDL_plus_I);
int APS5_VABI sceNpCppWebApiUnknown_fDex2lyDL_plus_I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fFgQN3-ZyrE", sceNpCppWebApiUnknown_fFgQN3_minus_ZyrE);
int APS5_VABI sceNpCppWebApiUnknown_fFgQN3_minus_ZyrE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fG06a38iao8", sceNpCppWebApiUnknown_fG06a38iao8);
int APS5_VABI sceNpCppWebApiUnknown_fG06a38iao8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fGZkGHlpjeI", sceNpCppWebApiUnknown_fGZkGHlpjeI);
int APS5_VABI sceNpCppWebApiUnknown_fGZkGHlpjeI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fI-bJo6WlXU", sceNpCppWebApiUnknown_fI_minus_bJo6WlXU);
int APS5_VABI sceNpCppWebApiUnknown_fI_minus_bJo6WlXU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fIzsqjyD5oY", sceNpCppWebApiUnknown_fIzsqjyD5oY);
int APS5_VABI sceNpCppWebApiUnknown_fIzsqjyD5oY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fJ9329XKPPA", sceNpCppWebApiUnknown_fJ9329XKPPA);
int APS5_VABI sceNpCppWebApiUnknown_fJ9329XKPPA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fJuX5gEyZwU", sceNpCppWebApiUnknown_fJuX5gEyZwU);
int APS5_VABI sceNpCppWebApiUnknown_fJuX5gEyZwU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fPqHpzI5B90", sceNpCppWebApiUnknown_fPqHpzI5B90);
int APS5_VABI sceNpCppWebApiUnknown_fPqHpzI5B90(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fSGHm9RjN5U", sceNpCppWebApiUnknown_fSGHm9RjN5U);
int APS5_VABI sceNpCppWebApiUnknown_fSGHm9RjN5U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fSHQFh1yyzw", sceNpCppWebApiUnknown_fSHQFh1yyzw);
int APS5_VABI sceNpCppWebApiUnknown_fSHQFh1yyzw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fT2haGTnwWE", sceNpCppWebApiUnknown_fT2haGTnwWE);
int APS5_VABI sceNpCppWebApiUnknown_fT2haGTnwWE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fTmhvzraShY", sceNpCppWebApiUnknown_fTmhvzraShY);
int APS5_VABI sceNpCppWebApiUnknown_fTmhvzraShY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fTxXvCVeTZU", sceNpCppWebApiUnknown_fTxXvCVeTZU);
int APS5_VABI sceNpCppWebApiUnknown_fTxXvCVeTZU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fVUuJBP8CTs", sceNpCppWebApiUnknown_fVUuJBP8CTs);
int APS5_VABI sceNpCppWebApiUnknown_fVUuJBP8CTs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fXY2I1W0WwI", sceNpCppWebApiUnknown_fXY2I1W0WwI);
int APS5_VABI sceNpCppWebApiUnknown_fXY2I1W0WwI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("faLX3UugvMA", sceNpCppWebApiUnknown_faLX3UugvMA);
int APS5_VABI sceNpCppWebApiUnknown_faLX3UugvMA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fblfzylXZPo", sceNpCppWebApiUnknown_fblfzylXZPo);
int APS5_VABI sceNpCppWebApiUnknown_fblfzylXZPo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fcnM7N9hbY8", sceNpCppWebApiUnknown_fcnM7N9hbY8);
int APS5_VABI sceNpCppWebApiUnknown_fcnM7N9hbY8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fdwMUgaSJ7A", sceNpCppWebApiUnknown_fdwMUgaSJ7A);
int APS5_VABI sceNpCppWebApiUnknown_fdwMUgaSJ7A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("feck3i1FOo0", sceNpCppWebApiUnknown_feck3i1FOo0);
int APS5_VABI sceNpCppWebApiUnknown_feck3i1FOo0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fef0ftqQSs4", sceNpCppWebApiUnknown_fef0ftqQSs4);
int APS5_VABI sceNpCppWebApiUnknown_fef0ftqQSs4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ff2u4mro4Zg", sceNpCppWebApiUnknown_ff2u4mro4Zg);
int APS5_VABI sceNpCppWebApiUnknown_ff2u4mro4Zg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fhRcPQVoEqM", sceNpCppWebApiUnknown_fhRcPQVoEqM);
int APS5_VABI sceNpCppWebApiUnknown_fhRcPQVoEqM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fhT6Ff0uR-w", sceNpCppWebApiUnknown_fhT6Ff0uR_minus_w);
int APS5_VABI sceNpCppWebApiUnknown_fhT6Ff0uR_minus_w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fkUKKs-QrJ8", sceNpCppWebApiUnknown_fkUKKs_minus_QrJ8);
int APS5_VABI sceNpCppWebApiUnknown_fkUKKs_minus_QrJ8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fmtiiQmMB7k", sceNpCppWebApiUnknown_fmtiiQmMB7k);
int APS5_VABI sceNpCppWebApiUnknown_fmtiiQmMB7k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fqF2T3M8uxk", sceNpCppWebApiUnknown_fqF2T3M8uxk);
int APS5_VABI sceNpCppWebApiUnknown_fqF2T3M8uxk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fsdh0JMoYYk", sceNpCppWebApiUnknown_fsdh0JMoYYk);
int APS5_VABI sceNpCppWebApiUnknown_fsdh0JMoYYk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fsvOBG-Ufug", sceNpCppWebApiUnknown_fsvOBG_minus_Ufug);
int APS5_VABI sceNpCppWebApiUnknown_fsvOBG_minus_Ufug(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fwRtYVa9Sr8", sceNpCppWebApiUnknown_fwRtYVa9Sr8);
int APS5_VABI sceNpCppWebApiUnknown_fwRtYVa9Sr8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fwYdaeuRgrM", sceNpCppWebApiUnknown_fwYdaeuRgrM);
int APS5_VABI sceNpCppWebApiUnknown_fwYdaeuRgrM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("fxlzW3SSdGc", sceNpCppWebApiUnknown_fxlzW3SSdGc);
int APS5_VABI sceNpCppWebApiUnknown_fxlzW3SSdGc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("g+ZmPEUfu8I", sceNpCppWebApiUnknown_g_plus_ZmPEUfu8I);
int APS5_VABI sceNpCppWebApiUnknown_g_plus_ZmPEUfu8I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("g1BAuEQJiRQ", sceNpCppWebApiUnknown_g1BAuEQJiRQ);
int APS5_VABI sceNpCppWebApiUnknown_g1BAuEQJiRQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("g3Dr-rOS+ZY", sceNpCppWebApiUnknown_g3Dr_minus_rOS_plus_ZY);
int APS5_VABI sceNpCppWebApiUnknown_g3Dr_minus_rOS_plus_ZY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("g6Fi1iVk3Kc", sceNpCppWebApiUnknown_g6Fi1iVk3Kc);
int APS5_VABI sceNpCppWebApiUnknown_g6Fi1iVk3Kc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("g8VFzqnRHX8", sceNpCppWebApiUnknown_g8VFzqnRHX8);
int APS5_VABI sceNpCppWebApiUnknown_g8VFzqnRHX8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("g96i5kYXrKk", sceNpCppWebApiUnknown_g96i5kYXrKk);
int APS5_VABI sceNpCppWebApiUnknown_g96i5kYXrKk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("g9FmpQw2M-U", sceNpCppWebApiUnknown_g9FmpQw2M_minus_U);
int APS5_VABI sceNpCppWebApiUnknown_g9FmpQw2M_minus_U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gA7zC2wtgxo", sceNpCppWebApiUnknown_gA7zC2wtgxo);
int APS5_VABI sceNpCppWebApiUnknown_gA7zC2wtgxo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gAaHJ9b4U90", sceNpCppWebApiUnknown_gAaHJ9b4U90);
int APS5_VABI sceNpCppWebApiUnknown_gAaHJ9b4U90(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gC8BoQoW8Eo", sceNpCppWebApiUnknown_gC8BoQoW8Eo);
int APS5_VABI sceNpCppWebApiUnknown_gC8BoQoW8Eo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gFEZ+WDc2GY", sceNpCppWebApiUnknown_gFEZ_plus_WDc2GY);
int APS5_VABI sceNpCppWebApiUnknown_gFEZ_plus_WDc2GY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gPe3KXjSgig", sceNpCppWebApiUnknown_gPe3KXjSgig);
int APS5_VABI sceNpCppWebApiUnknown_gPe3KXjSgig(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gRGCRBt1o0Q", sceNpCppWebApiUnknown_gRGCRBt1o0Q);
int APS5_VABI sceNpCppWebApiUnknown_gRGCRBt1o0Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gTiHp+M4wFM", sceNpCppWebApiUnknown_gTiHp_plus_M4wFM);
int APS5_VABI sceNpCppWebApiUnknown_gTiHp_plus_M4wFM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gVfZ1j2bhPM", sceNpCppWebApiUnknown_gVfZ1j2bhPM);
int APS5_VABI sceNpCppWebApiUnknown_gVfZ1j2bhPM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gaoBc6o3cXs", sceNpCppWebApiUnknown_gaoBc6o3cXs);
int APS5_VABI sceNpCppWebApiUnknown_gaoBc6o3cXs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ge6xTrDDgOs", sceNpCppWebApiUnknown_ge6xTrDDgOs);
int APS5_VABI sceNpCppWebApiUnknown_ge6xTrDDgOs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("geae-6xMaag", sceNpCppWebApiUnknown_geae_minus_6xMaag);
int APS5_VABI sceNpCppWebApiUnknown_geae_minus_6xMaag(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gglj0cQ9K1E", sceNpCppWebApiUnknown_gglj0cQ9K1E);
int APS5_VABI sceNpCppWebApiUnknown_gglj0cQ9K1E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gijCqBFE3p0", sceNpCppWebApiUnknown_gijCqBFE3p0);
int APS5_VABI sceNpCppWebApiUnknown_gijCqBFE3p0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gisu+HIdFWU", sceNpCppWebApiUnknown_gisu_plus_HIdFWU);
int APS5_VABI sceNpCppWebApiUnknown_gisu_plus_HIdFWU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gjLRZgfb3i0", sceNpCppWebApiUnknown_gjLRZgfb3i0);
int APS5_VABI sceNpCppWebApiUnknown_gjLRZgfb3i0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gluDrexUWQs", sceNpCppWebApiUnknown_gluDrexUWQs);
int APS5_VABI sceNpCppWebApiUnknown_gluDrexUWQs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gmr-M3dWfR0", sceNpCppWebApiUnknown_gmr_minus_M3dWfR0);
int APS5_VABI sceNpCppWebApiUnknown_gmr_minus_M3dWfR0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gp3soLZ1nu0", sceNpCppWebApiUnknown_gp3soLZ1nu0);
int APS5_VABI sceNpCppWebApiUnknown_gp3soLZ1nu0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gsAVunFeDT4", sceNpCppWebApiUnknown_gsAVunFeDT4);
int APS5_VABI sceNpCppWebApiUnknown_gsAVunFeDT4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gsljWk-gMtE", sceNpCppWebApiUnknown_gsljWk_minus_gMtE);
int APS5_VABI sceNpCppWebApiUnknown_gsljWk_minus_gMtE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gui41xif0Xw", sceNpCppWebApiUnknown_gui41xif0Xw);
int APS5_VABI sceNpCppWebApiUnknown_gui41xif0Xw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("guy3X-PEbqk", sceNpCppWebApiUnknown_guy3X_minus_PEbqk);
int APS5_VABI sceNpCppWebApiUnknown_guy3X_minus_PEbqk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gw7fieR8fC4", sceNpCppWebApiUnknown_gw7fieR8fC4);
int APS5_VABI sceNpCppWebApiUnknown_gw7fieR8fC4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gxlgckSr7z4", sceNpCppWebApiUnknown_gxlgckSr7z4);
int APS5_VABI sceNpCppWebApiUnknown_gxlgckSr7z4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("gyN2KZV162U", sceNpCppWebApiUnknown_gyN2KZV162U);
int APS5_VABI sceNpCppWebApiUnknown_gyN2KZV162U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("h6hnzl2edzA", sceNpCppWebApiUnknown_h6hnzl2edzA);
int APS5_VABI sceNpCppWebApiUnknown_h6hnzl2edzA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("h8K-pa8owbg", sceNpCppWebApiUnknown_h8K_minus_pa8owbg);
int APS5_VABI sceNpCppWebApiUnknown_h8K_minus_pa8owbg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hAH4eertqiA", sceNpCppWebApiUnknown_hAH4eertqiA);
int APS5_VABI sceNpCppWebApiUnknown_hAH4eertqiA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hGJ2JfVCXI0", sceNpCppWebApiUnknown_hGJ2JfVCXI0);
int APS5_VABI sceNpCppWebApiUnknown_hGJ2JfVCXI0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hH7KHku1lqs", sceNpCppWebApiUnknown_hH7KHku1lqs);
int APS5_VABI sceNpCppWebApiUnknown_hH7KHku1lqs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hH9TSsVNT74", sceNpCppWebApiUnknown_hH9TSsVNT74);
int APS5_VABI sceNpCppWebApiUnknown_hH9TSsVNT74(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hHP6EKc7WnI", sceNpCppWebApiUnknown_hHP6EKc7WnI);
int APS5_VABI sceNpCppWebApiUnknown_hHP6EKc7WnI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hJTfD6ay7rE", sceNpCppWebApiUnknown_hJTfD6ay7rE);
int APS5_VABI sceNpCppWebApiUnknown_hJTfD6ay7rE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hL06BXMcvJA", sceNpCppWebApiUnknown_hL06BXMcvJA);
int APS5_VABI sceNpCppWebApiUnknown_hL06BXMcvJA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hLuw-otq9WI", sceNpCppWebApiUnknown_hLuw_minus_otq9WI);
int APS5_VABI sceNpCppWebApiUnknown_hLuw_minus_otq9WI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hMwGlafyzGs", sceNpCppWebApiUnknown_hMwGlafyzGs);
int APS5_VABI sceNpCppWebApiUnknown_hMwGlafyzGs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hMxydTJQ670", sceNpCppWebApiUnknown_hMxydTJQ670);
int APS5_VABI sceNpCppWebApiUnknown_hMxydTJQ670(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hOnIlcGrO6g", sceNpCppWebApiUnknown_hOnIlcGrO6g);
int APS5_VABI sceNpCppWebApiUnknown_hOnIlcGrO6g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hQdNOFlDedY", sceNpCppWebApiUnknown_hQdNOFlDedY);
int APS5_VABI sceNpCppWebApiUnknown_hQdNOFlDedY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hZTfKpftpsU", sceNpCppWebApiUnknown_hZTfKpftpsU);
int APS5_VABI sceNpCppWebApiUnknown_hZTfKpftpsU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hbD28nqQKoc", sceNpCppWebApiUnknown_hbD28nqQKoc);
int APS5_VABI sceNpCppWebApiUnknown_hbD28nqQKoc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hcn7reGoPd0", sceNpCppWebApiUnknown_hcn7reGoPd0);
int APS5_VABI sceNpCppWebApiUnknown_hcn7reGoPd0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hedUKkqxVb4", sceNpCppWebApiUnknown_hedUKkqxVb4);
int APS5_VABI sceNpCppWebApiUnknown_hedUKkqxVb4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hgyiFlIb2L4", sceNpCppWebApiUnknown_hgyiFlIb2L4);
int APS5_VABI sceNpCppWebApiUnknown_hgyiFlIb2L4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hj40eDtISJY", sceNpCppWebApiUnknown_hj40eDtISJY);
int APS5_VABI sceNpCppWebApiUnknown_hj40eDtISJY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hjhEsh7g05Q", sceNpCppWebApiUnknown_hjhEsh7g05Q);
int APS5_VABI sceNpCppWebApiUnknown_hjhEsh7g05Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hjsm6ZT-Qq8", sceNpCppWebApiUnknown_hjsm6ZT_minus_Qq8);
int APS5_VABI sceNpCppWebApiUnknown_hjsm6ZT_minus_Qq8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hjzv6lEAIuI", sceNpCppWebApiUnknown_hjzv6lEAIuI);
int APS5_VABI sceNpCppWebApiUnknown_hjzv6lEAIuI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hkkKVRY8+QA", sceNpCppWebApiUnknown_hkkKVRY8_plus_QA);
int APS5_VABI sceNpCppWebApiUnknown_hkkKVRY8_plus_QA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hmVJYbCOt60", sceNpCppWebApiUnknown_hmVJYbCOt60);
int APS5_VABI sceNpCppWebApiUnknown_hmVJYbCOt60(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hoINmSMlYjI", sceNpCppWebApiUnknown_hoINmSMlYjI);
int APS5_VABI sceNpCppWebApiUnknown_hoINmSMlYjI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hoe1JJcbvz0", sceNpCppWebApiUnknown_hoe1JJcbvz0);
int APS5_VABI sceNpCppWebApiUnknown_hoe1JJcbvz0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hqco03inQbs", sceNpCppWebApiUnknown_hqco03inQbs);
int APS5_VABI sceNpCppWebApiUnknown_hqco03inQbs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hqeACk2dkzE", sceNpCppWebApiUnknown_hqeACk2dkzE);
int APS5_VABI sceNpCppWebApiUnknown_hqeACk2dkzE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hulfhT-chcs", sceNpCppWebApiUnknown_hulfhT_minus_chcs);
int APS5_VABI sceNpCppWebApiUnknown_hulfhT_minus_chcs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hyLvzF3HeWk", sceNpCppWebApiUnknown_hyLvzF3HeWk);
int APS5_VABI sceNpCppWebApiUnknown_hyLvzF3HeWk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("hyeuycsBGsM", sceNpCppWebApiUnknown_hyeuycsBGsM);
int APS5_VABI sceNpCppWebApiUnknown_hyeuycsBGsM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("i-iEzNiWKUE", sceNpCppWebApiUnknown_i_minus_iEzNiWKUE);
int APS5_VABI sceNpCppWebApiUnknown_i_minus_iEzNiWKUE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("i1GAPIM3DL8", sceNpCppWebApiUnknown_i1GAPIM3DL8);
int APS5_VABI sceNpCppWebApiUnknown_i1GAPIM3DL8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("i4u95ruk+R4", sceNpCppWebApiUnknown_i4u95ruk_plus_R4);
int APS5_VABI sceNpCppWebApiUnknown_i4u95ruk_plus_R4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("i4ud5O7OWWI", sceNpCppWebApiUnknown_i4ud5O7OWWI);
int APS5_VABI sceNpCppWebApiUnknown_i4ud5O7OWWI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("i6+3zRRQ+Bg", sceNpCppWebApiUnknown_i6_plus_3zRRQ_plus_Bg);
int APS5_VABI sceNpCppWebApiUnknown_i6_plus_3zRRQ_plus_Bg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iByZOVzfSQI", sceNpCppWebApiUnknown_iByZOVzfSQI);
int APS5_VABI sceNpCppWebApiUnknown_iByZOVzfSQI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iDGjTvynVug", sceNpCppWebApiUnknown_iDGjTvynVug);
int APS5_VABI sceNpCppWebApiUnknown_iDGjTvynVug(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iE8QHg0jx-k", sceNpCppWebApiUnknown_iE8QHg0jx_minus_k);
int APS5_VABI sceNpCppWebApiUnknown_iE8QHg0jx_minus_k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iFm1siY0U38", sceNpCppWebApiUnknown_iFm1siY0U38);
int APS5_VABI sceNpCppWebApiUnknown_iFm1siY0U38(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iGNQ7R33THQ", sceNpCppWebApiUnknown_iGNQ7R33THQ);
int APS5_VABI sceNpCppWebApiUnknown_iGNQ7R33THQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iI4El5rK42E", sceNpCppWebApiUnknown_iI4El5rK42E);
int APS5_VABI sceNpCppWebApiUnknown_iI4El5rK42E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iLnckwH2WBQ", sceNpCppWebApiUnknown_iLnckwH2WBQ);
int APS5_VABI sceNpCppWebApiUnknown_iLnckwH2WBQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iLxnuSroJHg", sceNpCppWebApiUnknown_iLxnuSroJHg);
int APS5_VABI sceNpCppWebApiUnknown_iLxnuSroJHg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iMcKgGtukyA", sceNpCppWebApiUnknown_iMcKgGtukyA);
int APS5_VABI sceNpCppWebApiUnknown_iMcKgGtukyA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iMgibUAAwdg", sceNpCppWebApiUnknown_iMgibUAAwdg);
int APS5_VABI sceNpCppWebApiUnknown_iMgibUAAwdg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iPfI2v+oYnQ", sceNpCppWebApiUnknown_iPfI2v_plus_oYnQ);
int APS5_VABI sceNpCppWebApiUnknown_iPfI2v_plus_oYnQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iRNZ5GV5b0Q", sceNpCppWebApiUnknown_iRNZ5GV5b0Q);
int APS5_VABI sceNpCppWebApiUnknown_iRNZ5GV5b0Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iSJJLbwR2Yk", sceNpCppWebApiUnknown_iSJJLbwR2Yk);
int APS5_VABI sceNpCppWebApiUnknown_iSJJLbwR2Yk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iTIgJfwD0MY", sceNpCppWebApiUnknown_iTIgJfwD0MY);
int APS5_VABI sceNpCppWebApiUnknown_iTIgJfwD0MY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iTgPNOBdkIM", sceNpCppWebApiUnknown_iTgPNOBdkIM);
int APS5_VABI sceNpCppWebApiUnknown_iTgPNOBdkIM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iWRQ8ANp-gc", sceNpCppWebApiUnknown_iWRQ8ANp_minus_gc);
int APS5_VABI sceNpCppWebApiUnknown_iWRQ8ANp_minus_gc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iWjdWa9Y5iI", sceNpCppWebApiUnknown_iWjdWa9Y5iI);
int APS5_VABI sceNpCppWebApiUnknown_iWjdWa9Y5iI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iYFKnyetU4Q", sceNpCppWebApiUnknown_iYFKnyetU4Q);
int APS5_VABI sceNpCppWebApiUnknown_iYFKnyetU4Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ieEQWlOYTg8", sceNpCppWebApiUnknown_ieEQWlOYTg8);
int APS5_VABI sceNpCppWebApiUnknown_ieEQWlOYTg8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("igLY5HmW6qk", sceNpCppWebApiUnknown_igLY5HmW6qk);
int APS5_VABI sceNpCppWebApiUnknown_igLY5HmW6qk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("igs871aV3H0", sceNpCppWebApiUnknown_igs871aV3H0);
int APS5_VABI sceNpCppWebApiUnknown_igs871aV3H0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ihPWeLj6kkU", sceNpCppWebApiUnknown_ihPWeLj6kkU);
int APS5_VABI sceNpCppWebApiUnknown_ihPWeLj6kkU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iiYZCXOo7Go", sceNpCppWebApiUnknown_iiYZCXOo7Go);
int APS5_VABI sceNpCppWebApiUnknown_iiYZCXOo7Go(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iiqR-H-DYrM", sceNpCppWebApiUnknown_iiqR_minus_H_minus_DYrM);
int APS5_VABI sceNpCppWebApiUnknown_iiqR_minus_H_minus_DYrM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ijijXYpJzC8", sceNpCppWebApiUnknown_ijijXYpJzC8);
int APS5_VABI sceNpCppWebApiUnknown_ijijXYpJzC8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("io-+Ahbd3dA", sceNpCppWebApiUnknown_io_minus__plus_Ahbd3dA);
int APS5_VABI sceNpCppWebApiUnknown_io_minus__plus_Ahbd3dA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ioW0-HURYdQ", sceNpCppWebApiUnknown_ioW0_minus_HURYdQ);
int APS5_VABI sceNpCppWebApiUnknown_ioW0_minus_HURYdQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ipDQvhXBhuI", sceNpCppWebApiUnknown_ipDQvhXBhuI);
int APS5_VABI sceNpCppWebApiUnknown_ipDQvhXBhuI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ipZRT3HMuJA", sceNpCppWebApiUnknown_ipZRT3HMuJA);
int APS5_VABI sceNpCppWebApiUnknown_ipZRT3HMuJA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iqm6sy3xsoo", sceNpCppWebApiUnknown_iqm6sy3xsoo);
int APS5_VABI sceNpCppWebApiUnknown_iqm6sy3xsoo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("isASk6cqZ6w", sceNpCppWebApiUnknown_isASk6cqZ6w);
int APS5_VABI sceNpCppWebApiUnknown_isASk6cqZ6w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("it90JR6e378", sceNpCppWebApiUnknown_it90JR6e378);
int APS5_VABI sceNpCppWebApiUnknown_it90JR6e378(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("iu2D0k7hojY", sceNpCppWebApiUnknown_iu2D0k7hojY);
int APS5_VABI sceNpCppWebApiUnknown_iu2D0k7hojY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ivMCitpSQNk", sceNpCppWebApiUnknown_ivMCitpSQNk);
int APS5_VABI sceNpCppWebApiUnknown_ivMCitpSQNk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ivQcJO5s7vY", sceNpCppWebApiUnknown_ivQcJO5s7vY);
int APS5_VABI sceNpCppWebApiUnknown_ivQcJO5s7vY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("j-N8TB-WFjA", sceNpCppWebApiUnknown_j_minus_N8TB_minus_WFjA);
int APS5_VABI sceNpCppWebApiUnknown_j_minus_N8TB_minus_WFjA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jAJoGOxJAjo", sceNpCppWebApiUnknown_jAJoGOxJAjo);
int APS5_VABI sceNpCppWebApiUnknown_jAJoGOxJAjo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jAO1IJKMhE4", sceNpCppWebApiUnknown_jAO1IJKMhE4);
int APS5_VABI sceNpCppWebApiUnknown_jAO1IJKMhE4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jAc86M302as", sceNpCppWebApiUnknown_jAc86M302as);
int APS5_VABI sceNpCppWebApiUnknown_jAc86M302as(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jAstE+lkQCs", sceNpCppWebApiUnknown_jAstE_plus_lkQCs);
int APS5_VABI sceNpCppWebApiUnknown_jAstE_plus_lkQCs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jDR-Re3F0Xw", sceNpCppWebApiUnknown_jDR_minus_Re3F0Xw);
int APS5_VABI sceNpCppWebApiUnknown_jDR_minus_Re3F0Xw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jKDY+jQCWzE", sceNpCppWebApiUnknown_jKDY_plus_jQCWzE);
int APS5_VABI sceNpCppWebApiUnknown_jKDY_plus_jQCWzE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jNF3HS1ziFs", sceNpCppWebApiUnknown_jNF3HS1ziFs);
int APS5_VABI sceNpCppWebApiUnknown_jNF3HS1ziFs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jTEUBw5jo6g", sceNpCppWebApiUnknown_jTEUBw5jo6g);
int APS5_VABI sceNpCppWebApiUnknown_jTEUBw5jo6g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jTcdCjMNdrk", sceNpCppWebApiUnknown_jTcdCjMNdrk);
int APS5_VABI sceNpCppWebApiUnknown_jTcdCjMNdrk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jWoi+KzyT8M", sceNpCppWebApiUnknown_jWoi_plus_KzyT8M);
int APS5_VABI sceNpCppWebApiUnknown_jWoi_plus_KzyT8M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jWylMa2FtuA", sceNpCppWebApiUnknown_jWylMa2FtuA);
int APS5_VABI sceNpCppWebApiUnknown_jWylMa2FtuA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jYAXNHlcQAc", sceNpCppWebApiUnknown_jYAXNHlcQAc);
int APS5_VABI sceNpCppWebApiUnknown_jYAXNHlcQAc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jaVd5xkK0tU", sceNpCppWebApiUnknown_jaVd5xkK0tU);
int APS5_VABI sceNpCppWebApiUnknown_jaVd5xkK0tU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jcMzMcZRt-Y", sceNpCppWebApiUnknown_jcMzMcZRt_minus_Y);
int APS5_VABI sceNpCppWebApiUnknown_jcMzMcZRt_minus_Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jdEbTdOhlqI", sceNpCppWebApiUnknown_jdEbTdOhlqI);
int APS5_VABI sceNpCppWebApiUnknown_jdEbTdOhlqI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jfCufYd0-+s", sceNpCppWebApiUnknown_jfCufYd0_minus__plus_s);
int APS5_VABI sceNpCppWebApiUnknown_jfCufYd0_minus__plus_s(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jgnKK+Q5hXs", sceNpCppWebApiUnknown_jgnKK_plus_Q5hXs);
int APS5_VABI sceNpCppWebApiUnknown_jgnKK_plus_Q5hXs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jisqpUJlaPA", sceNpCppWebApiUnknown_jisqpUJlaPA);
int APS5_VABI sceNpCppWebApiUnknown_jisqpUJlaPA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jsCNTcjsYH0", sceNpCppWebApiUnknown_jsCNTcjsYH0);
int APS5_VABI sceNpCppWebApiUnknown_jsCNTcjsYH0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jxC6X62V4Yw", sceNpCppWebApiUnknown_jxC6X62V4Yw);
int APS5_VABI sceNpCppWebApiUnknown_jxC6X62V4Yw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jxM0UUOggXU", sceNpCppWebApiUnknown_jxM0UUOggXU);
int APS5_VABI sceNpCppWebApiUnknown_jxM0UUOggXU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jxn875wvWPY", sceNpCppWebApiUnknown_jxn875wvWPY);
int APS5_VABI sceNpCppWebApiUnknown_jxn875wvWPY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jybbmJ+uEwo", sceNpCppWebApiUnknown_jybbmJ_plus_uEwo);
int APS5_VABI sceNpCppWebApiUnknown_jybbmJ_plus_uEwo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jydFDMsVG-I", sceNpCppWebApiUnknown_jydFDMsVG_minus_I);
int APS5_VABI sceNpCppWebApiUnknown_jydFDMsVG_minus_I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jyqje3VCc2o", sceNpCppWebApiUnknown_jyqje3VCc2o);
int APS5_VABI sceNpCppWebApiUnknown_jyqje3VCc2o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("jzkGwpPsFqI", sceNpCppWebApiUnknown_jzkGwpPsFqI);
int APS5_VABI sceNpCppWebApiUnknown_jzkGwpPsFqI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("k-Wh+dnRRdI", sceNpCppWebApiUnknown_k_minus_Wh_plus_dnRRdI);
int APS5_VABI sceNpCppWebApiUnknown_k_minus_Wh_plus_dnRRdI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("k1-l3-S82-Y", sceNpCppWebApiUnknown_k1_minus_l3_minus_S82_minus_Y);
int APS5_VABI sceNpCppWebApiUnknown_k1_minus_l3_minus_S82_minus_Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("k1DeKHg-DP4", sceNpCppWebApiUnknown_k1DeKHg_minus_DP4);
int APS5_VABI sceNpCppWebApiUnknown_k1DeKHg_minus_DP4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("k1Hsc0qMW9s", sceNpCppWebApiUnknown_k1Hsc0qMW9s);
int APS5_VABI sceNpCppWebApiUnknown_k1Hsc0qMW9s(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("k1gBazlQOKo", sceNpCppWebApiUnknown_k1gBazlQOKo);
int APS5_VABI sceNpCppWebApiUnknown_k1gBazlQOKo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("k2IDcx-apvQ", sceNpCppWebApiUnknown_k2IDcx_minus_apvQ);
int APS5_VABI sceNpCppWebApiUnknown_k2IDcx_minus_apvQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("k7+6XfiVmCw", sceNpCppWebApiUnknown_k7_plus_6XfiVmCw);
int APS5_VABI sceNpCppWebApiUnknown_k7_plus_6XfiVmCw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("k9zKu3RL5xs", sceNpCppWebApiUnknown_k9zKu3RL5xs);
int APS5_VABI sceNpCppWebApiUnknown_k9zKu3RL5xs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kEoFa4IyFHo", sceNpCppWebApiUnknown_kEoFa4IyFHo);
int APS5_VABI sceNpCppWebApiUnknown_kEoFa4IyFHo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kF8Mcy9F5O4", sceNpCppWebApiUnknown_kF8Mcy9F5O4);
int APS5_VABI sceNpCppWebApiUnknown_kF8Mcy9F5O4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kI0wgtzMZC0", sceNpCppWebApiUnknown_kI0wgtzMZC0);
int APS5_VABI sceNpCppWebApiUnknown_kI0wgtzMZC0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kJRDf7LAnDo", sceNpCppWebApiUnknown_kJRDf7LAnDo);
int APS5_VABI sceNpCppWebApiUnknown_kJRDf7LAnDo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kNiWFq7jWQ0", sceNpCppWebApiUnknown_kNiWFq7jWQ0);
int APS5_VABI sceNpCppWebApiUnknown_kNiWFq7jWQ0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kOaUt01Wmr8", sceNpCppWebApiUnknown_kOaUt01Wmr8);
int APS5_VABI sceNpCppWebApiUnknown_kOaUt01Wmr8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kOeBKp8TgFk", sceNpCppWebApiUnknown_kOeBKp8TgFk);
int APS5_VABI sceNpCppWebApiUnknown_kOeBKp8TgFk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kRXjOtU-NTI", sceNpCppWebApiUnknown_kRXjOtU_minus_NTI);
int APS5_VABI sceNpCppWebApiUnknown_kRXjOtU_minus_NTI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kSYZo64APPs", sceNpCppWebApiUnknown_kSYZo64APPs);
int APS5_VABI sceNpCppWebApiUnknown_kSYZo64APPs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kVGz48sqNJM", sceNpCppWebApiUnknown_kVGz48sqNJM);
int APS5_VABI sceNpCppWebApiUnknown_kVGz48sqNJM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kZFwfdh0z5o", sceNpCppWebApiUnknown_kZFwfdh0z5o);
int APS5_VABI sceNpCppWebApiUnknown_kZFwfdh0z5o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kZH1UUQ43gI", sceNpCppWebApiUnknown_kZH1UUQ43gI);
int APS5_VABI sceNpCppWebApiUnknown_kZH1UUQ43gI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kaAYYbwFoAg", sceNpCppWebApiUnknown_kaAYYbwFoAg);
int APS5_VABI sceNpCppWebApiUnknown_kaAYYbwFoAg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("khThzVltokg", sceNpCppWebApiUnknown_khThzVltokg);
int APS5_VABI sceNpCppWebApiUnknown_khThzVltokg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("khvgaTcMzP0", sceNpCppWebApiUnknown_khvgaTcMzP0);
int APS5_VABI sceNpCppWebApiUnknown_khvgaTcMzP0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kjAO5VVQPWk", sceNpCppWebApiUnknown_kjAO5VVQPWk);
int APS5_VABI sceNpCppWebApiUnknown_kjAO5VVQPWk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("klLqT+Tmbd4", sceNpCppWebApiUnknown_klLqT_plus_Tmbd4);
int APS5_VABI sceNpCppWebApiUnknown_klLqT_plus_Tmbd4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kpbW7Dr44Gc", sceNpCppWebApiUnknown_kpbW7Dr44Gc);
int APS5_VABI sceNpCppWebApiUnknown_kpbW7Dr44Gc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kpjkRdlqpmw", sceNpCppWebApiUnknown_kpjkRdlqpmw);
int APS5_VABI sceNpCppWebApiUnknown_kpjkRdlqpmw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ksHRN6fY8X0", sceNpCppWebApiUnknown_ksHRN6fY8X0);
int APS5_VABI sceNpCppWebApiUnknown_ksHRN6fY8X0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ktGKgTDt-nU", sceNpCppWebApiUnknown_ktGKgTDt_minus_nU);
int APS5_VABI sceNpCppWebApiUnknown_ktGKgTDt_minus_nU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kw-FTaZzJoU", sceNpCppWebApiUnknown_kw_minus_FTaZzJoU);
int APS5_VABI sceNpCppWebApiUnknown_kw_minus_FTaZzJoU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kxz+m8I8ulo", sceNpCppWebApiUnknown_kxz_plus_m8I8ulo);
int APS5_VABI sceNpCppWebApiUnknown_kxz_plus_m8I8ulo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kz2Z38yTLq0", sceNpCppWebApiUnknown_kz2Z38yTLq0);
int APS5_VABI sceNpCppWebApiUnknown_kz2Z38yTLq0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("kztrwy5ID1w", sceNpCppWebApiUnknown_kztrwy5ID1w);
int APS5_VABI sceNpCppWebApiUnknown_kztrwy5ID1w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("l-ackmrDncM", sceNpCppWebApiUnknown_l_minus_ackmrDncM);
int APS5_VABI sceNpCppWebApiUnknown_l_minus_ackmrDncM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("l1R5eJgjwys", sceNpCppWebApiUnknown_l1R5eJgjwys);
int APS5_VABI sceNpCppWebApiUnknown_l1R5eJgjwys(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("l2oxj+E2rQg", sceNpCppWebApiUnknown_l2oxj_plus_E2rQg);
int APS5_VABI sceNpCppWebApiUnknown_l2oxj_plus_E2rQg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("l3-83fvIqvM", sceNpCppWebApiUnknown_l3_minus_83fvIqvM);
int APS5_VABI sceNpCppWebApiUnknown_l3_minus_83fvIqvM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("l9XLBgtKouw", sceNpCppWebApiUnknown_l9XLBgtKouw);
int APS5_VABI sceNpCppWebApiUnknown_l9XLBgtKouw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lA2Lycz6ugQ", sceNpCppWebApiUnknown_lA2Lycz6ugQ);
int APS5_VABI sceNpCppWebApiUnknown_lA2Lycz6ugQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lB8driFKaoU", sceNpCppWebApiUnknown_lB8driFKaoU);
int APS5_VABI sceNpCppWebApiUnknown_lB8driFKaoU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lCcoeldiMhM", sceNpCppWebApiUnknown_lCcoeldiMhM);
int APS5_VABI sceNpCppWebApiUnknown_lCcoeldiMhM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lFoCvh0UOew", sceNpCppWebApiUnknown_lFoCvh0UOew);
int APS5_VABI sceNpCppWebApiUnknown_lFoCvh0UOew(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lIunPkkDpZk", sceNpCppWebApiUnknown_lIunPkkDpZk);
int APS5_VABI sceNpCppWebApiUnknown_lIunPkkDpZk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lLUy02Mje30", sceNpCppWebApiUnknown_lLUy02Mje30);
int APS5_VABI sceNpCppWebApiUnknown_lLUy02Mje30(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lLbOUEEQeWE", sceNpCppWebApiUnknown_lLbOUEEQeWE);
int APS5_VABI sceNpCppWebApiUnknown_lLbOUEEQeWE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lMdyP3SDLps", sceNpCppWebApiUnknown_lMdyP3SDLps);
int APS5_VABI sceNpCppWebApiUnknown_lMdyP3SDLps(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lQ7+8vb2vx4", sceNpCppWebApiUnknown_lQ7_plus_8vb2vx4);
int APS5_VABI sceNpCppWebApiUnknown_lQ7_plus_8vb2vx4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lQHMdtJ9LAA", sceNpCppWebApiUnknown_lQHMdtJ9LAA);
int APS5_VABI sceNpCppWebApiUnknown_lQHMdtJ9LAA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lSe4Oe+76o4", sceNpCppWebApiUnknown_lSe4Oe_plus_76o4);
int APS5_VABI sceNpCppWebApiUnknown_lSe4Oe_plus_76o4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lT4bKLwuA8g", sceNpCppWebApiUnknown_lT4bKLwuA8g);
int APS5_VABI sceNpCppWebApiUnknown_lT4bKLwuA8g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ldwx4dhgoh8", sceNpCppWebApiUnknown_ldwx4dhgoh8);
int APS5_VABI sceNpCppWebApiUnknown_ldwx4dhgoh8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lfUiDOAsDoo", sceNpCppWebApiUnknown_lfUiDOAsDoo);
int APS5_VABI sceNpCppWebApiUnknown_lfUiDOAsDoo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lflDLnoXcwY", sceNpCppWebApiUnknown_lflDLnoXcwY);
int APS5_VABI sceNpCppWebApiUnknown_lflDLnoXcwY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lgu-GuXVCOo", sceNpCppWebApiUnknown_lgu_minus_GuXVCOo);
int APS5_VABI sceNpCppWebApiUnknown_lgu_minus_GuXVCOo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lh6XyHyme90", sceNpCppWebApiUnknown_lh6XyHyme90);
int APS5_VABI sceNpCppWebApiUnknown_lh6XyHyme90(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lhmUadzsM0I", sceNpCppWebApiUnknown_lhmUadzsM0I);
int APS5_VABI sceNpCppWebApiUnknown_lhmUadzsM0I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lifv9YgJaYc", sceNpCppWebApiUnknown_lifv9YgJaYc);
int APS5_VABI sceNpCppWebApiUnknown_lifv9YgJaYc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("limbVG3jLPY", sceNpCppWebApiUnknown_limbVG3jLPY);
int APS5_VABI sceNpCppWebApiUnknown_limbVG3jLPY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lje+7Q7rn5U", sceNpCppWebApiUnknown_lje_plus_7Q7rn5U);
int APS5_VABI sceNpCppWebApiUnknown_lje_plus_7Q7rn5U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ljkbSGgi0H4", sceNpCppWebApiUnknown_ljkbSGgi0H4);
int APS5_VABI sceNpCppWebApiUnknown_ljkbSGgi0H4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lmima8meJtc", sceNpCppWebApiUnknown_lmima8meJtc);
int APS5_VABI sceNpCppWebApiUnknown_lmima8meJtc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("luaN6xA1bn8", sceNpCppWebApiUnknown_luaN6xA1bn8);
int APS5_VABI sceNpCppWebApiUnknown_luaN6xA1bn8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lxtHJMwBsaU", sceNpCppWebApiUnknown_lxtHJMwBsaU);
int APS5_VABI sceNpCppWebApiUnknown_lxtHJMwBsaU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lyKbR8AEuG4", sceNpCppWebApiUnknown_lyKbR8AEuG4);
int APS5_VABI sceNpCppWebApiUnknown_lyKbR8AEuG4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("lydPjwJQufI", sceNpCppWebApiUnknown_lydPjwJQufI);
int APS5_VABI sceNpCppWebApiUnknown_lydPjwJQufI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
int APS5_VABI sceNpCommerceDialogTerminate_nid_postfix() { NotImplemented_nid_no_patch("sceNpCommerceDialogTerminate"); return 0; }
APS5_EXPORT("m2Z378C5GDU", sceNpCppWebApiUnknown_m2Z378C5GDU);
int APS5_VABI sceNpCppWebApiUnknown_m2Z378C5GDU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("m5DU6GnjSlo", sceNpCppWebApiUnknown_m5DU6GnjSlo);
int APS5_VABI sceNpCppWebApiUnknown_m5DU6GnjSlo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("m5kDI-aUKzQ", sceNpCppWebApiUnknown_m5kDI_minus_aUKzQ);
int APS5_VABI sceNpCppWebApiUnknown_m5kDI_minus_aUKzQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("m6B-fJjS3F0", sceNpCppWebApiUnknown_m6B_minus_fJjS3F0);
int APS5_VABI sceNpCppWebApiUnknown_m6B_minus_fJjS3F0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("m6N4DtlBu9c", sceNpCppWebApiUnknown_m6N4DtlBu9c);
int APS5_VABI sceNpCppWebApiUnknown_m6N4DtlBu9c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("m6o+BHqSlG8", sceNpCppWebApiUnknown_m6o_plus_BHqSlG8);
int APS5_VABI sceNpCppWebApiUnknown_m6o_plus_BHqSlG8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("m7+o9cTFH1w", sceNpCppWebApiUnknown_m7_plus_o9cTFH1w);
int APS5_VABI sceNpCppWebApiUnknown_m7_plus_o9cTFH1w(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("m7ykSknwnRg", sceNpCppWebApiUnknown_m7ykSknwnRg);
int APS5_VABI sceNpCppWebApiUnknown_m7ykSknwnRg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mA00Xm1g-80", sceNpCppWebApiUnknown_mA00Xm1g_minus_80);
int APS5_VABI sceNpCppWebApiUnknown_mA00Xm1g_minus_80(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mAJiQGwBKOs", sceNpCppWebApiUnknown_mAJiQGwBKOs);
int APS5_VABI sceNpCppWebApiUnknown_mAJiQGwBKOs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mB3-pVyXXps", sceNpCppWebApiUnknown_mB3_minus_pVyXXps);
int APS5_VABI sceNpCppWebApiUnknown_mB3_minus_pVyXXps(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mBbG0YM9fEM", sceNpCppWebApiUnknown_mBbG0YM9fEM);
int APS5_VABI sceNpCppWebApiUnknown_mBbG0YM9fEM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mBxWXq2rJMo", sceNpCppWebApiUnknown_mBxWXq2rJMo);
int APS5_VABI sceNpCppWebApiUnknown_mBxWXq2rJMo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mCsV7izOkjY", sceNpCppWebApiUnknown_mCsV7izOkjY);
int APS5_VABI sceNpCppWebApiUnknown_mCsV7izOkjY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mGsBNF15gDU", sceNpCppWebApiUnknown_mGsBNF15gDU);
int APS5_VABI sceNpCppWebApiUnknown_mGsBNF15gDU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mHDS3EJIZSw", sceNpCppWebApiUnknown_mHDS3EJIZSw);
int APS5_VABI sceNpCppWebApiUnknown_mHDS3EJIZSw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mHtHPNmc+2M", sceNpCppWebApiUnknown_mHtHPNmc_plus_2M);
int APS5_VABI sceNpCppWebApiUnknown_mHtHPNmc_plus_2M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mLxIRUZyHHU", sceNpCppWebApiUnknown_mLxIRUZyHHU);
int APS5_VABI sceNpCppWebApiUnknown_mLxIRUZyHHU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mQfTgWuG0FA", sceNpCppWebApiUnknown_mQfTgWuG0FA);
int APS5_VABI sceNpCppWebApiUnknown_mQfTgWuG0FA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mR5E8sI836g", sceNpCppWebApiUnknown_mR5E8sI836g);
int APS5_VABI sceNpCppWebApiUnknown_mR5E8sI836g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mSM-Qy4oFi0", sceNpCppWebApiUnknown_mSM_minus_Qy4oFi0);
int APS5_VABI sceNpCppWebApiUnknown_mSM_minus_Qy4oFi0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mTdwsiRfpf4", sceNpCppWebApiUnknown_mTdwsiRfpf4);
int APS5_VABI sceNpCppWebApiUnknown_mTdwsiRfpf4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mUft23Sr2KU", sceNpCppWebApiUnknown_mUft23Sr2KU);
int APS5_VABI sceNpCppWebApiUnknown_mUft23Sr2KU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mV3PKml1+B4", sceNpCppWebApiUnknown_mV3PKml1_plus_B4);
int APS5_VABI sceNpCppWebApiUnknown_mV3PKml1_plus_B4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mVKcd5NwiWE", sceNpCppWebApiUnknown_mVKcd5NwiWE);
int APS5_VABI sceNpCppWebApiUnknown_mVKcd5NwiWE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mYUENHsJfe0", sceNpCppWebApiUnknown_mYUENHsJfe0);
int APS5_VABI sceNpCppWebApiUnknown_mYUENHsJfe0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mYo06zO7G3c", sceNpCppWebApiUnknown_mYo06zO7G3c);
int APS5_VABI sceNpCppWebApiUnknown_mYo06zO7G3c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mZxR56GIXBk", sceNpCppWebApiUnknown_mZxR56GIXBk);
int APS5_VABI sceNpCppWebApiUnknown_mZxR56GIXBk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("makW6W6BGUw", sceNpCppWebApiUnknown_makW6W6BGUw);
int APS5_VABI sceNpCppWebApiUnknown_makW6W6BGUw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mfJBpJ+qoLA", sceNpCppWebApiUnknown_mfJBpJ_plus_qoLA);
int APS5_VABI sceNpCppWebApiUnknown_mfJBpJ_plus_qoLA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mh0OvdefVzI", sceNpCppWebApiUnknown_mh0OvdefVzI);
int APS5_VABI sceNpCppWebApiUnknown_mh0OvdefVzI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mjChPky6hL0", sceNpCppWebApiUnknown_mjChPky6hL0);
int APS5_VABI sceNpCppWebApiUnknown_mjChPky6hL0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mku1GIeN1Fw", sceNpCppWebApiUnknown_mku1GIeN1Fw);
int APS5_VABI sceNpCppWebApiUnknown_mku1GIeN1Fw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mljkaZpffbQ", sceNpCppWebApiUnknown_mljkaZpffbQ);
int APS5_VABI sceNpCppWebApiUnknown_mljkaZpffbQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mn9RCl-PJC0", sceNpCppWebApiUnknown_mn9RCl_minus_PJC0);
int APS5_VABI sceNpCppWebApiUnknown_mn9RCl_minus_PJC0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mpEZaKKV5Bw", sceNpCppWebApiUnknown_mpEZaKKV5Bw);
int APS5_VABI sceNpCppWebApiUnknown_mpEZaKKV5Bw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mqM7+dpduR0", sceNpCppWebApiUnknown_mqM7_plus_dpduR0);
int APS5_VABI sceNpCppWebApiUnknown_mqM7_plus_dpduR0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("mqXwMzp+7pE", sceNpCppWebApiUnknown_mqXwMzp_plus_7pE);
int APS5_VABI sceNpCppWebApiUnknown_mqXwMzp_plus_7pE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("n0lAac92Sgk", sceNpCppWebApiUnknown_n0lAac92Sgk);
int APS5_VABI sceNpCppWebApiUnknown_n0lAac92Sgk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("n1fn2KFeLDA", sceNpCppWebApiUnknown_n1fn2KFeLDA);
int APS5_VABI sceNpCppWebApiUnknown_n1fn2KFeLDA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("n1yCLMxpseQ", sceNpCppWebApiUnknown_n1yCLMxpseQ);
int APS5_VABI sceNpCppWebApiUnknown_n1yCLMxpseQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("n6PRMUwUtXU", sceNpCppWebApiUnknown_n6PRMUwUtXU);
int APS5_VABI sceNpCppWebApiUnknown_n6PRMUwUtXU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nAXJqQPQ9cQ", sceNpCppWebApiUnknown_nAXJqQPQ9cQ);
int APS5_VABI sceNpCppWebApiUnknown_nAXJqQPQ9cQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nIWt+5qjWmo", sceNpCppWebApiUnknown_nIWt_plus_5qjWmo);
int APS5_VABI sceNpCppWebApiUnknown_nIWt_plus_5qjWmo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nJNonkUuVms", sceNpCppWebApiUnknown_nJNonkUuVms);
int APS5_VABI sceNpCppWebApiUnknown_nJNonkUuVms(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nL9vSVq-29s", sceNpCppWebApiUnknown_nL9vSVq_minus_29s);
int APS5_VABI sceNpCppWebApiUnknown_nL9vSVq_minus_29s(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nLA4Ue4iOGU", sceNpCppWebApiUnknown_nLA4Ue4iOGU);
int APS5_VABI sceNpCppWebApiUnknown_nLA4Ue4iOGU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nNqExI9JIMk", sceNpCppWebApiUnknown_nNqExI9JIMk);
int APS5_VABI sceNpCppWebApiUnknown_nNqExI9JIMk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nP-yK1MfjXw", sceNpCppWebApiUnknown_nP_minus_yK1MfjXw);
int APS5_VABI sceNpCppWebApiUnknown_nP_minus_yK1MfjXw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nQGJlZRQtXU", sceNpCppWebApiUnknown_nQGJlZRQtXU);
int APS5_VABI sceNpCppWebApiUnknown_nQGJlZRQtXU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nVU6Ft6polI", sceNpCppWebApiUnknown_nVU6Ft6polI);
int APS5_VABI sceNpCppWebApiUnknown_nVU6Ft6polI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nXa0s7xnbsQ", sceNpCppWebApiUnknown_nXa0s7xnbsQ);
int APS5_VABI sceNpCppWebApiUnknown_nXa0s7xnbsQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nZNxsX9uPfk", sceNpCppWebApiUnknown_nZNxsX9uPfk);
int APS5_VABI sceNpCppWebApiUnknown_nZNxsX9uPfk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nZqMHNtatUU", sceNpCppWebApiUnknown_nZqMHNtatUU);
int APS5_VABI sceNpCppWebApiUnknown_nZqMHNtatUU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nbRU58b2L1E", sceNpCppWebApiUnknown_nbRU58b2L1E);
int APS5_VABI sceNpCppWebApiUnknown_nbRU58b2L1E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ncIcckZh0rk", sceNpCppWebApiUnknown_ncIcckZh0rk);
int APS5_VABI sceNpCppWebApiUnknown_ncIcckZh0rk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nj0zxZTsvg4", sceNpCppWebApiUnknown_nj0zxZTsvg4);
int APS5_VABI sceNpCppWebApiUnknown_nj0zxZTsvg4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("njBb1RnJ43M", sceNpCppWebApiUnknown_njBb1RnJ43M);
int APS5_VABI sceNpCppWebApiUnknown_njBb1RnJ43M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("njXgooO8zYw", sceNpCppWebApiUnknown_njXgooO8zYw);
int APS5_VABI sceNpCppWebApiUnknown_njXgooO8zYw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("np6xXcXEnXE", sceNpCppWebApiUnknown_np6xXcXEnXE);
int APS5_VABI sceNpCppWebApiUnknown_np6xXcXEnXE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("npMIfvNUuiQ", sceNpCppWebApiUnknown_npMIfvNUuiQ);
int APS5_VABI sceNpCppWebApiUnknown_npMIfvNUuiQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nr4+nQ18BxA", sceNpCppWebApiUnknown_nr4_plus_nQ18BxA);
int APS5_VABI sceNpCppWebApiUnknown_nr4_plus_nQ18BxA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ntfVELXIYvE", sceNpCppWebApiUnknown_ntfVELXIYvE);
int APS5_VABI sceNpCppWebApiUnknown_ntfVELXIYvE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nuGuHAit5fM", sceNpCppWebApiUnknown_nuGuHAit5fM);
int APS5_VABI sceNpCppWebApiUnknown_nuGuHAit5fM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nw694oGIoW4", sceNpCppWebApiUnknown_nw694oGIoW4);
int APS5_VABI sceNpCppWebApiUnknown_nw694oGIoW4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("nwqYelH-nW4", sceNpCppWebApiUnknown_nwqYelH_minus_nW4);
int APS5_VABI sceNpCppWebApiUnknown_nwqYelH_minus_nW4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("o-sMt4OM+Rc", sceNpCppWebApiUnknown_o_minus_sMt4OM_plus_Rc);
int APS5_VABI sceNpCppWebApiUnknown_o_minus_sMt4OM_plus_Rc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("o4qtwaucN+Y", sceNpCppWebApiUnknown_o4qtwaucN_plus_Y);
int APS5_VABI sceNpCppWebApiUnknown_o4qtwaucN_plus_Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("o6kFQQ6TGg4", sceNpCppWebApiUnknown_o6kFQQ6TGg4);
int APS5_VABI sceNpCppWebApiUnknown_o6kFQQ6TGg4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("o7Rj82lRZ98", sceNpCppWebApiUnknown_o7Rj82lRZ98);
int APS5_VABI sceNpCppWebApiUnknown_o7Rj82lRZ98(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("o83x+Wjbp6k", sceNpCppWebApiUnknown_o83x_plus_Wjbp6k);
int APS5_VABI sceNpCppWebApiUnknown_o83x_plus_Wjbp6k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("oAuFlME7yNI", sceNpCppWebApiUnknown_oAuFlME7yNI);
int APS5_VABI sceNpCppWebApiUnknown_oAuFlME7yNI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("oBcQL9+IY8U", sceNpCppWebApiUnknown_oBcQL9_plus_IY8U);
int APS5_VABI sceNpCppWebApiUnknown_oBcQL9_plus_IY8U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("oEHZ5ia8+KY", sceNpCppWebApiUnknown_oEHZ5ia8_plus_KY);
int APS5_VABI sceNpCppWebApiUnknown_oEHZ5ia8_plus_KY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("oNHtwMj4T+c", sceNpCppWebApiUnknown_oNHtwMj4T_plus_c);
int APS5_VABI sceNpCppWebApiUnknown_oNHtwMj4T_plus_c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("oQrvxj3qQq8", sceNpCppWebApiUnknown_oQrvxj3qQq8);
int APS5_VABI sceNpCppWebApiUnknown_oQrvxj3qQq8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("oW0JyXhSzCs", sceNpCppWebApiUnknown_oW0JyXhSzCs);
int APS5_VABI sceNpCppWebApiUnknown_oW0JyXhSzCs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("oYHyLfnZOYE", sceNpCppWebApiUnknown_oYHyLfnZOYE);
int APS5_VABI sceNpCppWebApiUnknown_oYHyLfnZOYE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("oYUTamHByZI", sceNpCppWebApiUnknown_oYUTamHByZI);
int APS5_VABI sceNpCppWebApiUnknown_oYUTamHByZI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("oaPVgmehY-4", sceNpCppWebApiUnknown_oaPVgmehY_minus_4);
int APS5_VABI sceNpCppWebApiUnknown_oaPVgmehY_minus_4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("obpwwLfR1Ew", sceNpCppWebApiUnknown_obpwwLfR1Ew);
int APS5_VABI sceNpCppWebApiUnknown_obpwwLfR1Ew(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ocg6JEGRtz0", sceNpCppWebApiUnknown_ocg6JEGRtz0);
int APS5_VABI sceNpCppWebApiUnknown_ocg6JEGRtz0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("og5vEJWP-w0", sceNpCppWebApiUnknown_og5vEJWP_minus_w0);
int APS5_VABI sceNpCppWebApiUnknown_og5vEJWP_minus_w0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ogc2cNOobW8", sceNpCppWebApiUnknown_ogc2cNOobW8);
int APS5_VABI sceNpCppWebApiUnknown_ogc2cNOobW8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("oicZcfjulAs", sceNpCppWebApiUnknown_oicZcfjulAs);
int APS5_VABI sceNpCppWebApiUnknown_oicZcfjulAs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ojA1dqecGw4", sceNpCppWebApiUnknown_ojA1dqecGw4);
int APS5_VABI sceNpCppWebApiUnknown_ojA1dqecGw4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ojac7V4XpDU", sceNpCppWebApiUnknown_ojac7V4XpDU);
int APS5_VABI sceNpCppWebApiUnknown_ojac7V4XpDU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("okAxtWIxibY", sceNpCppWebApiUnknown_okAxtWIxibY);
int APS5_VABI sceNpCppWebApiUnknown_okAxtWIxibY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("olf4MzGpXWM", sceNpCppWebApiUnknown_olf4MzGpXWM);
int APS5_VABI sceNpCppWebApiUnknown_olf4MzGpXWM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("onFll9DvvBY", sceNpCppWebApiUnknown_onFll9DvvBY);
int APS5_VABI sceNpCppWebApiUnknown_onFll9DvvBY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ovJChufCjXc", sceNpCppWebApiUnknown_ovJChufCjXc);
int APS5_VABI sceNpCppWebApiUnknown_ovJChufCjXc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("owrpOxU0eoE", sceNpCppWebApiUnknown_owrpOxU0eoE);
int APS5_VABI sceNpCppWebApiUnknown_owrpOxU0eoE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ozwJY4AO-V0", sceNpCppWebApiUnknown_ozwJY4AO_minus_V0);
int APS5_VABI sceNpCppWebApiUnknown_ozwJY4AO_minus_V0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("p+1CI2F796U", sceNpCppWebApiUnknown_p_plus_1CI2F796U);
int APS5_VABI sceNpCppWebApiUnknown_p_plus_1CI2F796U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("p1SPtutBLtQ", sceNpCppWebApiUnknown_p1SPtutBLtQ);
int APS5_VABI sceNpCppWebApiUnknown_p1SPtutBLtQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("p5MJfs0tn4M", sceNpCppWebApiUnknown_p5MJfs0tn4M);
int APS5_VABI sceNpCppWebApiUnknown_p5MJfs0tn4M(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("p7ieGjluqCU", sceNpCppWebApiUnknown_p7ieGjluqCU);
int APS5_VABI sceNpCppWebApiUnknown_p7ieGjluqCU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pAI9K0j3zuc", sceNpCppWebApiUnknown_pAI9K0j3zuc);
int APS5_VABI sceNpCppWebApiUnknown_pAI9K0j3zuc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pB+UVz83POQ", sceNpCppWebApiUnknown_pB_plus_UVz83POQ);
int APS5_VABI sceNpCppWebApiUnknown_pB_plus_UVz83POQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pBSgMrf0tnM", sceNpCppWebApiUnknown_pBSgMrf0tnM);
int APS5_VABI sceNpCppWebApiUnknown_pBSgMrf0tnM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pChAYPYSxWA", sceNpCppWebApiUnknown_pChAYPYSxWA);
int APS5_VABI sceNpCppWebApiUnknown_pChAYPYSxWA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pDUQVO32lZY", sceNpCppWebApiUnknown_pDUQVO32lZY);
int APS5_VABI sceNpCppWebApiUnknown_pDUQVO32lZY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pITl48PLFeg", sceNpCppWebApiUnknown_pITl48PLFeg);
int APS5_VABI sceNpCppWebApiUnknown_pITl48PLFeg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pJGfKNGIPpI", sceNpCppWebApiUnknown_pJGfKNGIPpI);
int APS5_VABI sceNpCppWebApiUnknown_pJGfKNGIPpI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pLgDetoUvdE", sceNpCppWebApiUnknown_pLgDetoUvdE);
int APS5_VABI sceNpCppWebApiUnknown_pLgDetoUvdE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pQ9+KPwjQkA", sceNpCppWebApiUnknown_pQ9_plus_KPwjQkA);
int APS5_VABI sceNpCppWebApiUnknown_pQ9_plus_KPwjQkA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pSpO3hPNv64", sceNpCppWebApiUnknown_pSpO3hPNv64);
int APS5_VABI sceNpCppWebApiUnknown_pSpO3hPNv64(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pUSjD6Wmxo4", sceNpCppWebApiUnknown_pUSjD6Wmxo4);
int APS5_VABI sceNpCppWebApiUnknown_pUSjD6Wmxo4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pVO1rIj3RBs", sceNpCppWebApiUnknown_pVO1rIj3RBs);
int APS5_VABI sceNpCppWebApiUnknown_pVO1rIj3RBs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pWE9pFFi68Q", sceNpCppWebApiUnknown_pWE9pFFi68Q);
int APS5_VABI sceNpCppWebApiUnknown_pWE9pFFi68Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pX14dbZetTQ", sceNpCppWebApiUnknown_pX14dbZetTQ);
int APS5_VABI sceNpCppWebApiUnknown_pX14dbZetTQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pdwofrxAnAs", sceNpCppWebApiUnknown_pdwofrxAnAs);
int APS5_VABI sceNpCppWebApiUnknown_pdwofrxAnAs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("phueB6QtB5o", sceNpCppWebApiUnknown_phueB6QtB5o);
int APS5_VABI sceNpCppWebApiUnknown_phueB6QtB5o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("plUh+Axzi30", sceNpCppWebApiUnknown_plUh_plus_Axzi30);
int APS5_VABI sceNpCppWebApiUnknown_plUh_plus_Axzi30(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pleuuED1nRY", sceNpCppWebApiUnknown_pleuuED1nRY);
int APS5_VABI sceNpCppWebApiUnknown_pleuuED1nRY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pmgE9a-bpHQ", sceNpCppWebApiUnknown_pmgE9a_minus_bpHQ);
int APS5_VABI sceNpCppWebApiUnknown_pmgE9a_minus_bpHQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pncKzlpsGBs", sceNpCppWebApiUnknown_pncKzlpsGBs);
int APS5_VABI sceNpCppWebApiUnknown_pncKzlpsGBs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("podVPPEQoYo", sceNpCppWebApiUnknown_podVPPEQoYo);
int APS5_VABI sceNpCppWebApiUnknown_podVPPEQoYo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pq7sFJ6E6Sg", sceNpCppWebApiUnknown_pq7sFJ6E6Sg);
int APS5_VABI sceNpCppWebApiUnknown_pq7sFJ6E6Sg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("prJ++4gQB9U", sceNpCppWebApiUnknown_prJ_plus__plus_4gQB9U);
int APS5_VABI sceNpCppWebApiUnknown_prJ_plus__plus_4gQB9U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ps9dR2UwMZE", sceNpCppWebApiUnknown_ps9dR2UwMZE);
int APS5_VABI sceNpCppWebApiUnknown_ps9dR2UwMZE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ptE7eJxWp0c", sceNpCppWebApiUnknown_ptE7eJxWp0c);
int APS5_VABI sceNpCppWebApiUnknown_ptE7eJxWp0c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("puakWgWqnmw", sceNpCppWebApiUnknown_puakWgWqnmw);
int APS5_VABI sceNpCppWebApiUnknown_puakWgWqnmw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("pzhdkWrC-2c", sceNpCppWebApiUnknown_pzhdkWrC_minus_2c);
int APS5_VABI sceNpCppWebApiUnknown_pzhdkWrC_minus_2c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("q+50oeoRpnY", sceNpCppWebApiUnknown_q_plus_50oeoRpnY);
int APS5_VABI sceNpCppWebApiUnknown_q_plus_50oeoRpnY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("q-Ue2YZGwYU", sceNpCppWebApiUnknown_q_minus_Ue2YZGwYU);
int APS5_VABI sceNpCppWebApiUnknown_q_minus_Ue2YZGwYU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("q0CBI7DJ0X8", sceNpCppWebApiUnknown_q0CBI7DJ0X8);
int APS5_VABI sceNpCppWebApiUnknown_q0CBI7DJ0X8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("q9Ac0wYBOgQ", sceNpCppWebApiUnknown_q9Ac0wYBOgQ);
int APS5_VABI sceNpCppWebApiUnknown_q9Ac0wYBOgQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("q9PEYKv1Ed0", sceNpCppWebApiUnknown_q9PEYKv1Ed0);
int APS5_VABI sceNpCppWebApiUnknown_q9PEYKv1Ed0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qApvJWktSqw", sceNpCppWebApiUnknown_qApvJWktSqw);
int APS5_VABI sceNpCppWebApiUnknown_qApvJWktSqw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qFz8fbyj0Z0", sceNpCppWebApiUnknown_qFz8fbyj0Z0);
int APS5_VABI sceNpCppWebApiUnknown_qFz8fbyj0Z0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qGP8hoLWSEI", sceNpCppWebApiUnknown_qGP8hoLWSEI);
int APS5_VABI sceNpCppWebApiUnknown_qGP8hoLWSEI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qGwknHP6ohM", sceNpCppWebApiUnknown_qGwknHP6ohM);
int APS5_VABI sceNpCppWebApiUnknown_qGwknHP6ohM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qIJiUzoPBzk", sceNpCppWebApiUnknown_qIJiUzoPBzk);
int APS5_VABI sceNpCppWebApiUnknown_qIJiUzoPBzk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qKfj8jo1vcE", sceNpCppWebApiUnknown_qKfj8jo1vcE);
int APS5_VABI sceNpCppWebApiUnknown_qKfj8jo1vcE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qMYMahQKgPQ", sceNpCppWebApiUnknown_qMYMahQKgPQ);
int APS5_VABI sceNpCppWebApiUnknown_qMYMahQKgPQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qPOsKSdHIUY", sceNpCppWebApiUnknown_qPOsKSdHIUY);
int APS5_VABI sceNpCppWebApiUnknown_qPOsKSdHIUY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
int APS5_VABI sceNpRegisterStateCallbackA_nid_postfix() { NotImplemented_nid_no_patch("sceNpRegisterStateCallbackA"); return 0; }
APS5_EXPORT("qR57vtAF3gw", sceNpCppWebApiUnknown_qR57vtAF3gw);
int APS5_VABI sceNpCppWebApiUnknown_qR57vtAF3gw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qSul5Durywc", sceNpCppWebApiUnknown_qSul5Durywc);
int APS5_VABI sceNpCppWebApiUnknown_qSul5Durywc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qUziNVrQ+wo", sceNpCppWebApiUnknown_qUziNVrQ_plus_wo);
int APS5_VABI sceNpCppWebApiUnknown_qUziNVrQ_plus_wo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qVyhgyIgQlE", sceNpCppWebApiUnknown_qVyhgyIgQlE);
int APS5_VABI sceNpCppWebApiUnknown_qVyhgyIgQlE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qfNBROhf3q8", sceNpCppWebApiUnknown_qfNBROhf3q8);
int APS5_VABI sceNpCppWebApiUnknown_qfNBROhf3q8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qi+xvgAV4bo", sceNpCppWebApiUnknown_qi_plus_xvgAV4bo);
int APS5_VABI sceNpCppWebApiUnknown_qi_plus_xvgAV4bo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ql1h2F4TIcE", sceNpCppWebApiUnknown_ql1h2F4TIcE);
int APS5_VABI sceNpCppWebApiUnknown_ql1h2F4TIcE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qnvk932UxYg", sceNpCppWebApiUnknown_qnvk932UxYg);
int APS5_VABI sceNpCppWebApiUnknown_qnvk932UxYg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qtagaBorsKQ", sceNpCppWebApiUnknown_qtagaBorsKQ);
int APS5_VABI sceNpCppWebApiUnknown_qtagaBorsKQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qu8vIEhVfZg", sceNpCppWebApiUnknown_qu8vIEhVfZg);
int APS5_VABI sceNpCppWebApiUnknown_qu8vIEhVfZg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qvp47ao23og", sceNpCppWebApiUnknown_qvp47ao23og);
int APS5_VABI sceNpCppWebApiUnknown_qvp47ao23og(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qwa0biONtPI", sceNpCppWebApiUnknown_qwa0biONtPI);
int APS5_VABI sceNpCppWebApiUnknown_qwa0biONtPI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("qxSrG71aeME", sceNpCppWebApiUnknown_qxSrG71aeME);
int APS5_VABI sceNpCppWebApiUnknown_qxSrG71aeME(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("r+bobzRe1IM", sceNpCppWebApiUnknown_r_plus_bobzRe1IM);
int APS5_VABI sceNpCppWebApiUnknown_r_plus_bobzRe1IM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("r+jU4rXRMm8", sceNpCppWebApiUnknown_r_plus_jU4rXRMm8);
int APS5_VABI sceNpCppWebApiUnknown_r_plus_jU4rXRMm8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("r-twZl6gN5k", sceNpCppWebApiUnknown_r_minus_twZl6gN5k);
int APS5_VABI sceNpCppWebApiUnknown_r_minus_twZl6gN5k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("r42bWcQbtZY", sceNpCppWebApiUnknown_r42bWcQbtZY);
int APS5_VABI sceNpCppWebApiUnknown_r42bWcQbtZY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("r4XacqHvkn4", sceNpCppWebApiUnknown_r4XacqHvkn4);
int APS5_VABI sceNpCppWebApiUnknown_r4XacqHvkn4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("r8mVMwlafF8", sceNpCppWebApiUnknown_r8mVMwlafF8);
int APS5_VABI sceNpCppWebApiUnknown_r8mVMwlafF8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("r9GyJMHv0yw", sceNpCppWebApiUnknown_r9GyJMHv0yw);
int APS5_VABI sceNpCppWebApiUnknown_r9GyJMHv0yw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rBNBuzCGsLQ", sceNpCppWebApiUnknown_rBNBuzCGsLQ);
int APS5_VABI sceNpCppWebApiUnknown_rBNBuzCGsLQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rEt3OdJ9JMY", sceNpCppWebApiUnknown_rEt3OdJ9JMY);
int APS5_VABI sceNpCppWebApiUnknown_rEt3OdJ9JMY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rFFxx44Zmtc", sceNpCppWebApiUnknown_rFFxx44Zmtc);
int APS5_VABI sceNpCppWebApiUnknown_rFFxx44Zmtc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rGVD1AHDPbw", sceNpCppWebApiUnknown_rGVD1AHDPbw);
int APS5_VABI sceNpCppWebApiUnknown_rGVD1AHDPbw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rH5-TraEnxk", sceNpCppWebApiUnknown_rH5_minus_TraEnxk);
int APS5_VABI sceNpCppWebApiUnknown_rH5_minus_TraEnxk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rJPGshRLOpc", sceNpCppWebApiUnknown_rJPGshRLOpc);
int APS5_VABI sceNpCppWebApiUnknown_rJPGshRLOpc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rM1pB40ImUI", sceNpCppWebApiUnknown_rM1pB40ImUI);
int APS5_VABI sceNpCppWebApiUnknown_rM1pB40ImUI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rNL4HZ9kH1U", sceNpCppWebApiUnknown_rNL4HZ9kH1U);
int APS5_VABI sceNpCppWebApiUnknown_rNL4HZ9kH1U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rP-rdKffodM", sceNpCppWebApiUnknown_rP_minus_rdKffodM);
int APS5_VABI sceNpCppWebApiUnknown_rP_minus_rdKffodM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rPxhVLFJBm4", sceNpCppWebApiUnknown_rPxhVLFJBm4);
int APS5_VABI sceNpCppWebApiUnknown_rPxhVLFJBm4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rQFNwRTkhZA", sceNpCppWebApiUnknown_rQFNwRTkhZA);
int APS5_VABI sceNpCppWebApiUnknown_rQFNwRTkhZA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rVdu+F4JFC4", sceNpCppWebApiUnknown_rVdu_plus_F4JFC4);
int APS5_VABI sceNpCppWebApiUnknown_rVdu_plus_F4JFC4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rYR8ya7vsGg", sceNpCppWebApiUnknown_rYR8ya7vsGg);
int APS5_VABI sceNpCppWebApiUnknown_rYR8ya7vsGg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rd6CkjAC3B0", sceNpCppWebApiUnknown_rd6CkjAC3B0);
int APS5_VABI sceNpCppWebApiUnknown_rd6CkjAC3B0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rdEN7D3ZN1E", sceNpCppWebApiUnknown_rdEN7D3ZN1E);
int APS5_VABI sceNpCppWebApiUnknown_rdEN7D3ZN1E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rpmRha1Eq9s", sceNpCppWebApiUnknown_rpmRha1Eq9s);
int APS5_VABI sceNpCppWebApiUnknown_rpmRha1Eq9s(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rq73eO5xSRc", sceNpCppWebApiUnknown_rq73eO5xSRc);
int APS5_VABI sceNpCppWebApiUnknown_rq73eO5xSRc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rtjxnvq9Qwk", sceNpCppWebApiUnknown_rtjxnvq9Qwk);
int APS5_VABI sceNpCppWebApiUnknown_rtjxnvq9Qwk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ruhMvhzt+Ow", sceNpCppWebApiUnknown_ruhMvhzt_plus_Ow);
int APS5_VABI sceNpCppWebApiUnknown_ruhMvhzt_plus_Ow(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rv0psaVHnf8", sceNpCppWebApiUnknown_rv0psaVHnf8);
int APS5_VABI sceNpCppWebApiUnknown_rv0psaVHnf8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rx3a9I1GwWw", sceNpCppWebApiUnknown_rx3a9I1GwWw);
int APS5_VABI sceNpCppWebApiUnknown_rx3a9I1GwWw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rxe7HVaQePA", sceNpCppWebApiUnknown_rxe7HVaQePA);
int APS5_VABI sceNpCppWebApiUnknown_rxe7HVaQePA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("rz5k8OHfefQ", sceNpCppWebApiUnknown_rz5k8OHfefQ);
int APS5_VABI sceNpCppWebApiUnknown_rz5k8OHfefQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("s+lrU-zZsEc", sceNpCppWebApiUnknown_s_plus_lrU_minus_zZsEc);
int APS5_VABI sceNpCppWebApiUnknown_s_plus_lrU_minus_zZsEc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("s+xL+l2aMsg", sceNpCppWebApiUnknown_s_plus_xL_plus_l2aMsg);
int APS5_VABI sceNpCppWebApiUnknown_s_plus_xL_plus_l2aMsg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("s0E+jrkzo-A", sceNpCppWebApiUnknown_s0E_plus_jrkzo_minus_A);
int APS5_VABI sceNpCppWebApiUnknown_s0E_plus_jrkzo_minus_A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("s4G45M9w8tc", sceNpCppWebApiUnknown_s4G45M9w8tc);
int APS5_VABI sceNpCppWebApiUnknown_s4G45M9w8tc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("s5xvFcUb3Ao", sceNpCppWebApiUnknown_s5xvFcUb3Ao);
int APS5_VABI sceNpCppWebApiUnknown_s5xvFcUb3Ao(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("s7YtGdt8Sg4", sceNpCppWebApiUnknown_s7YtGdt8Sg4);
int APS5_VABI sceNpCppWebApiUnknown_s7YtGdt8Sg4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("s8GYZ2aPoTU", sceNpCppWebApiUnknown_s8GYZ2aPoTU);
int APS5_VABI sceNpCppWebApiUnknown_s8GYZ2aPoTU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("sCnkOGMZjH8", sceNpCppWebApiUnknown_sCnkOGMZjH8);
int APS5_VABI sceNpCppWebApiUnknown_sCnkOGMZjH8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("sHydxqXaEeA", sceNpCppWebApiUnknown_sHydxqXaEeA);
int APS5_VABI sceNpCppWebApiUnknown_sHydxqXaEeA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("sRn6qePT54Q", sceNpCppWebApiUnknown_sRn6qePT54Q);
int APS5_VABI sceNpCppWebApiUnknown_sRn6qePT54Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("sSYpL+hDUFw", sceNpCppWebApiUnknown_sSYpL_plus_hDUFw);
int APS5_VABI sceNpCppWebApiUnknown_sSYpL_plus_hDUFw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("sSr4mR33SaY", sceNpCppWebApiUnknown_sSr4mR33SaY);
int APS5_VABI sceNpCppWebApiUnknown_sSr4mR33SaY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("sW6h+-un80k", sceNpCppWebApiUnknown_sW6h_plus__minus_un80k);
int APS5_VABI sceNpCppWebApiUnknown_sW6h_plus__minus_un80k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("sZIoMRGO+jk", sceNpCppWebApiUnknown_sZIoMRGO_plus_jk);
int APS5_VABI sceNpCppWebApiUnknown_sZIoMRGO_plus_jk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("sbfbBFhDwAM", sceNpCppWebApiUnknown_sbfbBFhDwAM);
int APS5_VABI sceNpCppWebApiUnknown_sbfbBFhDwAM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("sc7nVAAAwxg", sceNpCppWebApiUnknown_sc7nVAAAwxg);
int APS5_VABI sceNpCppWebApiUnknown_sc7nVAAAwxg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("seBoSuoZ3T8", sceNpCppWebApiUnknown_seBoSuoZ3T8);
int APS5_VABI sceNpCppWebApiUnknown_seBoSuoZ3T8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("shsyj1FfVkQ", sceNpCppWebApiUnknown_shsyj1FfVkQ);
int APS5_VABI sceNpCppWebApiUnknown_shsyj1FfVkQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("skniY8iy+9Y", sceNpCppWebApiUnknown_skniY8iy_plus_9Y);
int APS5_VABI sceNpCppWebApiUnknown_skniY8iy_plus_9Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("slvvHT5LSEw", sceNpCppWebApiUnknown_slvvHT5LSEw);
int APS5_VABI sceNpCppWebApiUnknown_slvvHT5LSEw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("smwAAHq-Q-o", sceNpCppWebApiUnknown_smwAAHq_minus_Q_minus_o);
int APS5_VABI sceNpCppWebApiUnknown_smwAAHq_minus_Q_minus_o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("snUone92hj8", sceNpCppWebApiUnknown_snUone92hj8);
int APS5_VABI sceNpCppWebApiUnknown_snUone92hj8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("snWcGhO1omE", sceNpCppWebApiUnknown_snWcGhO1omE);
int APS5_VABI sceNpCppWebApiUnknown_snWcGhO1omE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("srbWnHQ6hX8", sceNpCppWebApiUnknown_srbWnHQ6hX8);
int APS5_VABI sceNpCppWebApiUnknown_srbWnHQ6hX8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("su65s8-rLsk", sceNpCppWebApiUnknown_su65s8_minus_rLsk);
int APS5_VABI sceNpCppWebApiUnknown_su65s8_minus_rLsk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("syDD+xcUmEs", sceNpCppWebApiUnknown_syDD_plus_xcUmEs);
int APS5_VABI sceNpCppWebApiUnknown_syDD_plus_xcUmEs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("syGFFMBNH2g", sceNpCppWebApiUnknown_syGFFMBNH2g);
int APS5_VABI sceNpCppWebApiUnknown_syGFFMBNH2g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("syK5g6mvSX8", sceNpCppWebApiUnknown_syK5g6mvSX8);
int APS5_VABI sceNpCppWebApiUnknown_syK5g6mvSX8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("sySG4MR0gZU", sceNpCppWebApiUnknown_sySG4MR0gZU);
int APS5_VABI sceNpCppWebApiUnknown_sySG4MR0gZU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("t091XfeKHng", sceNpCppWebApiUnknown_t091XfeKHng);
int APS5_VABI sceNpCppWebApiUnknown_t091XfeKHng(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("t0Upc-BAuQs", sceNpCppWebApiUnknown_t0Upc_minus_BAuQs);
int APS5_VABI sceNpCppWebApiUnknown_t0Upc_minus_BAuQs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("t0gtqDVSszU", sceNpCppWebApiUnknown_t0gtqDVSszU);
int APS5_VABI sceNpCppWebApiUnknown_t0gtqDVSszU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("t2firQTyFsI", sceNpCppWebApiUnknown_t2firQTyFsI);
int APS5_VABI sceNpCppWebApiUnknown_t2firQTyFsI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("t3Y63SE7O08", sceNpCppWebApiUnknown_t3Y63SE7O08);
int APS5_VABI sceNpCppWebApiUnknown_t3Y63SE7O08(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("t4FA0L6NjqY", sceNpCppWebApiUnknown_t4FA0L6NjqY);
int APS5_VABI sceNpCppWebApiUnknown_t4FA0L6NjqY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("t8-zRyLWOXE", sceNpCppWebApiUnknown_t8_minus_zRyLWOXE);
int APS5_VABI sceNpCppWebApiUnknown_t8_minus_zRyLWOXE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("t84AiYnMwPg", sceNpCppWebApiUnknown_t84AiYnMwPg);
int APS5_VABI sceNpCppWebApiUnknown_t84AiYnMwPg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tCi2HGtNZYc", sceNpCppWebApiUnknown_tCi2HGtNZYc);
int APS5_VABI sceNpCppWebApiUnknown_tCi2HGtNZYc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tF0jtg3c2Gs", sceNpCppWebApiUnknown_tF0jtg3c2Gs);
int APS5_VABI sceNpCppWebApiUnknown_tF0jtg3c2Gs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tF4iLLzeCTc", sceNpCppWebApiUnknown_tF4iLLzeCTc);
int APS5_VABI sceNpCppWebApiUnknown_tF4iLLzeCTc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tFe8YsdlKyc", sceNpCppWebApiUnknown_tFe8YsdlKyc);
int APS5_VABI sceNpCppWebApiUnknown_tFe8YsdlKyc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tFuPAKkrjl8", sceNpCppWebApiUnknown_tFuPAKkrjl8);
int APS5_VABI sceNpCppWebApiUnknown_tFuPAKkrjl8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tHnYDUM6SpQ", sceNpCppWebApiUnknown_tHnYDUM6SpQ);
int APS5_VABI sceNpCppWebApiUnknown_tHnYDUM6SpQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tIkZcQS39gU", sceNpCppWebApiUnknown_tIkZcQS39gU);
int APS5_VABI sceNpCppWebApiUnknown_tIkZcQS39gU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tJ2EMNwxLic", sceNpCppWebApiUnknown_tJ2EMNwxLic);
int APS5_VABI sceNpCppWebApiUnknown_tJ2EMNwxLic(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tJMAm9ZBelo", sceNpCppWebApiUnknown_tJMAm9ZBelo);
int APS5_VABI sceNpCppWebApiUnknown_tJMAm9ZBelo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tO9I58zL4Ko", sceNpCppWebApiUnknown_tO9I58zL4Ko);
int APS5_VABI sceNpCppWebApiUnknown_tO9I58zL4Ko(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tQgPrFvN92g", sceNpCppWebApiUnknown_tQgPrFvN92g);
int APS5_VABI sceNpCppWebApiUnknown_tQgPrFvN92g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tUT0r9LCo9g", sceNpCppWebApiUnknown_tUT0r9LCo9g);
int APS5_VABI sceNpCppWebApiUnknown_tUT0r9LCo9g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tZ+2Lm15pOE", sceNpCppWebApiUnknown_tZ_plus_2Lm15pOE);
int APS5_VABI sceNpCppWebApiUnknown_tZ_plus_2Lm15pOE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tdgoXqPSNKk", sceNpCppWebApiUnknown_tdgoXqPSNKk);
int APS5_VABI sceNpCppWebApiUnknown_tdgoXqPSNKk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tdt1pcVlfac", sceNpCppWebApiUnknown_tdt1pcVlfac);
int APS5_VABI sceNpCppWebApiUnknown_tdt1pcVlfac(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tewFk6K6aB8", sceNpCppWebApiUnknown_tewFk6K6aB8);
int APS5_VABI sceNpCppWebApiUnknown_tewFk6K6aB8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tjOYtZ4xFN0", sceNpCppWebApiUnknown_tjOYtZ4xFN0);
int APS5_VABI sceNpCppWebApiUnknown_tjOYtZ4xFN0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tjZgJ24KMJs", sceNpCppWebApiUnknown_tjZgJ24KMJs);
int APS5_VABI sceNpCppWebApiUnknown_tjZgJ24KMJs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tluz-CQSrnM", sceNpCppWebApiUnknown_tluz_minus_CQSrnM);
int APS5_VABI sceNpCppWebApiUnknown_tluz_minus_CQSrnM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tme8lbzh1Kc", sceNpCppWebApiUnknown_tme8lbzh1Kc);
int APS5_VABI sceNpCppWebApiUnknown_tme8lbzh1Kc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tnym874g8jE", sceNpCppWebApiUnknown_tnym874g8jE);
int APS5_VABI sceNpCppWebApiUnknown_tnym874g8jE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tpx0zNA9X+o", sceNpCppWebApiUnknown_tpx0zNA9X_plus_o);
int APS5_VABI sceNpCppWebApiUnknown_tpx0zNA9X_plus_o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tpz4YwJwqcQ", sceNpCppWebApiUnknown_tpz4YwJwqcQ);
int APS5_VABI sceNpCppWebApiUnknown_tpz4YwJwqcQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("tuqxNaYtmmA", sceNpCppWebApiUnknown_tuqxNaYtmmA);
int APS5_VABI sceNpCppWebApiUnknown_tuqxNaYtmmA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("u1BIPLpvRLo", sceNpCppWebApiUnknown_u1BIPLpvRLo);
int APS5_VABI sceNpCppWebApiUnknown_u1BIPLpvRLo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("u1wbCugKzYQ", sceNpCppWebApiUnknown_u1wbCugKzYQ);
int APS5_VABI sceNpCppWebApiUnknown_u1wbCugKzYQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("u49J-rEDPCA", sceNpCppWebApiUnknown_u49J_minus_rEDPCA);
int APS5_VABI sceNpCppWebApiUnknown_u49J_minus_rEDPCA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("u5GYBCMlh-I", sceNpCppWebApiUnknown_u5GYBCMlh_minus_I);
int APS5_VABI sceNpCppWebApiUnknown_u5GYBCMlh_minus_I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("u6NRNJREiEE", sceNpCppWebApiUnknown_u6NRNJREiEE);
int APS5_VABI sceNpCppWebApiUnknown_u6NRNJREiEE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("u8fpXsjm+Rk", sceNpCppWebApiUnknown_u8fpXsjm_plus_Rk);
int APS5_VABI sceNpCppWebApiUnknown_u8fpXsjm_plus_Rk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uBvSzZvntAU", sceNpCppWebApiUnknown_uBvSzZvntAU);
int APS5_VABI sceNpCppWebApiUnknown_uBvSzZvntAU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uC-DmNKwiOA", sceNpCppWebApiUnknown_uC_minus_DmNKwiOA);
int APS5_VABI sceNpCppWebApiUnknown_uC_minus_DmNKwiOA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uCZdii3T6zc", sceNpCppWebApiUnknown_uCZdii3T6zc);
int APS5_VABI sceNpCppWebApiUnknown_uCZdii3T6zc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uG3xw77eZvc", sceNpCppWebApiUnknown_uG3xw77eZvc);
int APS5_VABI sceNpCppWebApiUnknown_uG3xw77eZvc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uGK-c7gP1lQ", sceNpCppWebApiUnknown_uGK_minus_c7gP1lQ);
int APS5_VABI sceNpCppWebApiUnknown_uGK_minus_c7gP1lQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uJH0UZFEMRg", sceNpCppWebApiUnknown_uJH0UZFEMRg);
int APS5_VABI sceNpCppWebApiUnknown_uJH0UZFEMRg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uPBxGgP0xcM", sceNpCppWebApiUnknown_uPBxGgP0xcM);
int APS5_VABI sceNpCppWebApiUnknown_uPBxGgP0xcM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uTWW7GR8-YM", sceNpCppWebApiUnknown_uTWW7GR8_minus_YM);
int APS5_VABI sceNpCppWebApiUnknown_uTWW7GR8_minus_YM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uWx7Xd2dgkE", sceNpCppWebApiUnknown_uWx7Xd2dgkE);
int APS5_VABI sceNpCppWebApiUnknown_uWx7Xd2dgkE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uYdiOWB804s", sceNpCppWebApiUnknown_uYdiOWB804s);
int APS5_VABI sceNpCppWebApiUnknown_uYdiOWB804s(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ub-4aF7I0Lg", sceNpCppWebApiUnknown_ub_minus_4aF7I0Lg);
int APS5_VABI sceNpCppWebApiUnknown_ub_minus_4aF7I0Lg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ucOP9llCucE", sceNpCppWebApiUnknown_ucOP9llCucE);
int APS5_VABI sceNpCppWebApiUnknown_ucOP9llCucE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("udZmgiZVFZ8", sceNpCppWebApiUnknown_udZmgiZVFZ8);
int APS5_VABI sceNpCppWebApiUnknown_udZmgiZVFZ8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("udajK8+CB+g", sceNpCppWebApiUnknown_udajK8_plus_CB_plus_g);
int APS5_VABI sceNpCppWebApiUnknown_udajK8_plus_CB_plus_g(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ukMRZael2xs", sceNpCppWebApiUnknown_ukMRZael2xs);
int APS5_VABI sceNpCppWebApiUnknown_ukMRZael2xs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ul8RI0nVEIQ", sceNpCppWebApiUnknown_ul8RI0nVEIQ);
int APS5_VABI sceNpCppWebApiUnknown_ul8RI0nVEIQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("umjYKp8Bc6c", sceNpCppWebApiUnknown_umjYKp8Bc6c);
int APS5_VABI sceNpCppWebApiUnknown_umjYKp8Bc6c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("umm5m+mXiZs", sceNpCppWebApiUnknown_umm5m_plus_mXiZs);
int APS5_VABI sceNpCppWebApiUnknown_umm5m_plus_mXiZs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uoLEglsvThA", sceNpCppWebApiUnknown_uoLEglsvThA);
int APS5_VABI sceNpCppWebApiUnknown_uoLEglsvThA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("urz1KK4Je0U", sceNpCppWebApiUnknown_urz1KK4Je0U);
int APS5_VABI sceNpCppWebApiUnknown_urz1KK4Je0U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("us+hb1r9BN4", sceNpCppWebApiUnknown_us_plus_hb1r9BN4);
int APS5_VABI sceNpCppWebApiUnknown_us_plus_hb1r9BN4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uu5+2E0u2hI", sceNpCppWebApiUnknown_uu5_plus_2E0u2hI);
int APS5_VABI sceNpCppWebApiUnknown_uu5_plus_2E0u2hI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uw9QT3dcY9c", sceNpCppWebApiUnknown_uw9QT3dcY9c);
int APS5_VABI sceNpCppWebApiUnknown_uw9QT3dcY9c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uwHcXkst1Dw", sceNpCppWebApiUnknown_uwHcXkst1Dw);
int APS5_VABI sceNpCppWebApiUnknown_uwHcXkst1Dw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uwaDkiieYIM", sceNpCppWebApiUnknown_uwaDkiieYIM);
int APS5_VABI sceNpCppWebApiUnknown_uwaDkiieYIM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("uzgnlW3QnTo", sceNpCppWebApiUnknown_uzgnlW3QnTo);
int APS5_VABI sceNpCppWebApiUnknown_uzgnlW3QnTo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("v+tXF9ppYiQ", sceNpCppWebApiUnknown_v_plus_tXF9ppYiQ);
int APS5_VABI sceNpCppWebApiUnknown_v_plus_tXF9ppYiQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("v1SALP2Agbk", sceNpCppWebApiUnknown_v1SALP2Agbk);
int APS5_VABI sceNpCppWebApiUnknown_v1SALP2Agbk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("v6OJwqfxqzQ", sceNpCppWebApiUnknown_v6OJwqfxqzQ);
int APS5_VABI sceNpCppWebApiUnknown_v6OJwqfxqzQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vANdRKdHPvk", sceNpCppWebApiUnknown_vANdRKdHPvk);
int APS5_VABI sceNpCppWebApiUnknown_vANdRKdHPvk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vBTyBcdhvLM", sceNpCppWebApiUnknown_vBTyBcdhvLM);
int APS5_VABI sceNpCppWebApiUnknown_vBTyBcdhvLM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vCFTfAoPeZo", sceNpCppWebApiUnknown_vCFTfAoPeZo);
int APS5_VABI sceNpCppWebApiUnknown_vCFTfAoPeZo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vDv8dP6VDik", sceNpCppWebApiUnknown_vDv8dP6VDik);
int APS5_VABI sceNpCppWebApiUnknown_vDv8dP6VDik(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vEv-yQtu1Ms", sceNpCppWebApiUnknown_vEv_minus_yQtu1Ms);
int APS5_VABI sceNpCppWebApiUnknown_vEv_minus_yQtu1Ms(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vF8HDGTpci8", sceNpCppWebApiUnknown_vF8HDGTpci8);
int APS5_VABI sceNpCppWebApiUnknown_vF8HDGTpci8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vFMRb8-gA7I", sceNpCppWebApiUnknown_vFMRb8_minus_gA7I);
int APS5_VABI sceNpCppWebApiUnknown_vFMRb8_minus_gA7I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vFSzbv76iMA", sceNpCppWebApiUnknown_vFSzbv76iMA);
int APS5_VABI sceNpCppWebApiUnknown_vFSzbv76iMA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vHAKNmxiL-U", sceNpCppWebApiUnknown_vHAKNmxiL_minus_U);
int APS5_VABI sceNpCppWebApiUnknown_vHAKNmxiL_minus_U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vJrnbutABoE", sceNpCppWebApiUnknown_vJrnbutABoE);
int APS5_VABI sceNpCppWebApiUnknown_vJrnbutABoE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vLpNFjC9lc4", sceNpCppWebApiUnknown_vLpNFjC9lc4);
int APS5_VABI sceNpCppWebApiUnknown_vLpNFjC9lc4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vQBgtFYNqlM", sceNpCppWebApiUnknown_vQBgtFYNqlM);
int APS5_VABI sceNpCppWebApiUnknown_vQBgtFYNqlM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vRkef+KyE4A", sceNpCppWebApiUnknown_vRkef_plus_KyE4A);
int APS5_VABI sceNpCppWebApiUnknown_vRkef_plus_KyE4A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vRpYko5WTm4", sceNpCppWebApiUnknown_vRpYko5WTm4);
int APS5_VABI sceNpCppWebApiUnknown_vRpYko5WTm4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vTVWjoFDvL0", sceNpCppWebApiUnknown_vTVWjoFDvL0);
int APS5_VABI sceNpCppWebApiUnknown_vTVWjoFDvL0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vURZNQ8+cP0", sceNpCppWebApiUnknown_vURZNQ8_plus_cP0);
int APS5_VABI sceNpCppWebApiUnknown_vURZNQ8_plus_cP0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vbJxberpWTs", sceNpCppWebApiUnknown_vbJxberpWTs);
int APS5_VABI sceNpCppWebApiUnknown_vbJxberpWTs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vcGcgbhP0BU", sceNpCppWebApiUnknown_vcGcgbhP0BU);
int APS5_VABI sceNpCppWebApiUnknown_vcGcgbhP0BU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vcgfSZOpqLg", sceNpCppWebApiUnknown_vcgfSZOpqLg);
int APS5_VABI sceNpCppWebApiUnknown_vcgfSZOpqLg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vcrFJP7x4wE", sceNpCppWebApiUnknown_vcrFJP7x4wE);
int APS5_VABI sceNpCppWebApiUnknown_vcrFJP7x4wE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vdk6rLp8ivc", sceNpCppWebApiUnknown_vdk6rLp8ivc);
int APS5_VABI sceNpCppWebApiUnknown_vdk6rLp8ivc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vdpVwH3lbwU", sceNpCppWebApiUnknown_vdpVwH3lbwU);
int APS5_VABI sceNpCppWebApiUnknown_vdpVwH3lbwU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("veRySWMcttw", sceNpCppWebApiUnknown_veRySWMcttw);
int APS5_VABI sceNpCppWebApiUnknown_veRySWMcttw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vgTyRFegScw", sceNpCppWebApiUnknown_vgTyRFegScw);
int APS5_VABI sceNpCppWebApiUnknown_vgTyRFegScw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vjw99oC+SaU", sceNpCppWebApiUnknown_vjw99oC_plus_SaU);
int APS5_VABI sceNpCppWebApiUnknown_vjw99oC_plus_SaU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vm2V6pGIJBo", sceNpCppWebApiUnknown_vm2V6pGIJBo);
int APS5_VABI sceNpCppWebApiUnknown_vm2V6pGIJBo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vnsy7vxfH74", sceNpCppWebApiUnknown_vnsy7vxfH74);
int APS5_VABI sceNpCppWebApiUnknown_vnsy7vxfH74(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vpen+AiTSKY", sceNpCppWebApiUnknown_vpen_plus_AiTSKY);
int APS5_VABI sceNpCppWebApiUnknown_vpen_plus_AiTSKY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vpkkVwCPaxE", sceNpCppWebApiUnknown_vpkkVwCPaxE);
int APS5_VABI sceNpCppWebApiUnknown_vpkkVwCPaxE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vs2SuyIZid4", sceNpCppWebApiUnknown_vs2SuyIZid4);
int APS5_VABI sceNpCppWebApiUnknown_vs2SuyIZid4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vx4I6uMoiww", sceNpCppWebApiUnknown_vx4I6uMoiww);
int APS5_VABI sceNpCppWebApiUnknown_vx4I6uMoiww(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vy-0DOQobkU", sceNpCppWebApiUnknown_vy_minus_0DOQobkU);
int APS5_VABI sceNpCppWebApiUnknown_vy_minus_0DOQobkU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("vzKNY2nY7Jk", sceNpCppWebApiUnknown_vzKNY2nY7Jk);
int APS5_VABI sceNpCppWebApiUnknown_vzKNY2nY7Jk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("w-E4CYjDhgk", sceNpCppWebApiUnknown_w_minus_E4CYjDhgk);
int APS5_VABI sceNpCppWebApiUnknown_w_minus_E4CYjDhgk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("w2TjgpWqhb4", sceNpCppWebApiUnknown_w2TjgpWqhb4);
int APS5_VABI sceNpCppWebApiUnknown_w2TjgpWqhb4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("w3jAgmDwR2c", sceNpCppWebApiUnknown_w3jAgmDwR2c);
int APS5_VABI sceNpCppWebApiUnknown_w3jAgmDwR2c(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("w4K4nTYwhVE", sceNpCppWebApiUnknown_w4K4nTYwhVE);
int APS5_VABI sceNpCppWebApiUnknown_w4K4nTYwhVE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("w7CYvjrBvbQ", sceNpCppWebApiUnknown_w7CYvjrBvbQ);
int APS5_VABI sceNpCppWebApiUnknown_w7CYvjrBvbQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wFcm6bgWB9Q", sceNpCppWebApiUnknown_wFcm6bgWB9Q);
int APS5_VABI sceNpCppWebApiUnknown_wFcm6bgWB9Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wFdynVv+1oQ", sceNpCppWebApiUnknown_wFdynVv_plus_1oQ);
int APS5_VABI sceNpCppWebApiUnknown_wFdynVv_plus_1oQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wJWqKZOj4Ts", sceNpCppWebApiUnknown_wJWqKZOj4Ts);
int APS5_VABI sceNpCppWebApiUnknown_wJWqKZOj4Ts(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wP-FIy-gRq0", sceNpCppWebApiUnknown_wP_minus_FIy_minus_gRq0);
int APS5_VABI sceNpCppWebApiUnknown_wP_minus_FIy_minus_gRq0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wPEv3CHUImU", sceNpCppWebApiUnknown_wPEv3CHUImU);
int APS5_VABI sceNpCppWebApiUnknown_wPEv3CHUImU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wPO9107ThVw", sceNpCppWebApiUnknown_wPO9107ThVw);
int APS5_VABI sceNpCppWebApiUnknown_wPO9107ThVw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wQ-fzWkSvs0", sceNpCppWebApiUnknown_wQ_minus_fzWkSvs0);
int APS5_VABI sceNpCppWebApiUnknown_wQ_minus_fzWkSvs0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wQZSd2EOn0E", sceNpCppWebApiUnknown_wQZSd2EOn0E);
int APS5_VABI sceNpCppWebApiUnknown_wQZSd2EOn0E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wR0o0b2T5gM", sceNpCppWebApiUnknown_wR0o0b2T5gM);
int APS5_VABI sceNpCppWebApiUnknown_wR0o0b2T5gM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wS6DC0qTbqk", sceNpCppWebApiUnknown_wS6DC0qTbqk);
int APS5_VABI sceNpCppWebApiUnknown_wS6DC0qTbqk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wVqxM58sIKs", sceNpCppWebApiUnknown_wVqxM58sIKs);
int APS5_VABI sceNpCppWebApiUnknown_wVqxM58sIKs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wW4jTznZqAE", sceNpCppWebApiUnknown_wW4jTznZqAE);
int APS5_VABI sceNpCppWebApiUnknown_wW4jTznZqAE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wW5ObWAQ8HA", sceNpCppWebApiUnknown_wW5ObWAQ8HA);
int APS5_VABI sceNpCppWebApiUnknown_wW5ObWAQ8HA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wWdKuMPssRQ", sceNpCppWebApiUnknown_wWdKuMPssRQ);
int APS5_VABI sceNpCppWebApiUnknown_wWdKuMPssRQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wZST0SYCWWc", sceNpCppWebApiUnknown_wZST0SYCWWc);
int APS5_VABI sceNpCppWebApiUnknown_wZST0SYCWWc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("waM0cFaBPbA", sceNpCppWebApiUnknown_waM0cFaBPbA);
int APS5_VABI sceNpCppWebApiUnknown_waM0cFaBPbA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("waic+rKvYd0", sceNpCppWebApiUnknown_waic_plus_rKvYd0);
int APS5_VABI sceNpCppWebApiUnknown_waic_plus_rKvYd0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wc+k3ulMh0Y", sceNpCppWebApiUnknown_wc_plus_k3ulMh0Y);
int APS5_VABI sceNpCppWebApiUnknown_wc_plus_k3ulMh0Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wd3t5dSrf+Y", sceNpCppWebApiUnknown_wd3t5dSrf_plus_Y);
int APS5_VABI sceNpCppWebApiUnknown_wd3t5dSrf_plus_Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wezlRzu+ng8", sceNpCppWebApiUnknown_wezlRzu_plus_ng8);
int APS5_VABI sceNpCppWebApiUnknown_wezlRzu_plus_ng8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("whIG7nhC3VQ", sceNpCppWebApiUnknown_whIG7nhC3VQ);
int APS5_VABI sceNpCppWebApiUnknown_whIG7nhC3VQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wi2nosL6l1k", sceNpCppWebApiUnknown_wi2nosL6l1k);
int APS5_VABI sceNpCppWebApiUnknown_wi2nosL6l1k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wl-mK89cDIs", sceNpCppWebApiUnknown_wl_minus_mK89cDIs);
int APS5_VABI sceNpCppWebApiUnknown_wl_minus_mK89cDIs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wl2g7GlHxIY", sceNpCppWebApiUnknown_wl2g7GlHxIY);
int APS5_VABI sceNpCppWebApiUnknown_wl2g7GlHxIY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wnn7JlD7SeQ", sceNpCppWebApiUnknown_wnn7JlD7SeQ);
int APS5_VABI sceNpCppWebApiUnknown_wnn7JlD7SeQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("woPfFeWQXBs", sceNpCppWebApiUnknown_woPfFeWQXBs);
int APS5_VABI sceNpCppWebApiUnknown_woPfFeWQXBs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wp8+c84G5Xw", sceNpCppWebApiUnknown_wp8_plus_c84G5Xw);
int APS5_VABI sceNpCppWebApiUnknown_wp8_plus_c84G5Xw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wqHK7gA8TAY", sceNpCppWebApiUnknown_wqHK7gA8TAY);
int APS5_VABI sceNpCppWebApiUnknown_wqHK7gA8TAY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wt3qUO8WM3U", sceNpCppWebApiUnknown_wt3qUO8WM3U);
int APS5_VABI sceNpCppWebApiUnknown_wt3qUO8WM3U(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wuettmP-HHY", sceNpCppWebApiUnknown_wuettmP_minus_HHY);
int APS5_VABI sceNpCppWebApiUnknown_wuettmP_minus_HHY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wv+uok7-UBk", sceNpCppWebApiUnknown_wv_plus_uok7_minus_UBk);
int APS5_VABI sceNpCppWebApiUnknown_wv_plus_uok7_minus_UBk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wwsoDm+0Uo4", sceNpCppWebApiUnknown_wwsoDm_plus_0Uo4);
int APS5_VABI sceNpCppWebApiUnknown_wwsoDm_plus_0Uo4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("wzlainWo3eE", sceNpCppWebApiUnknown_wzlainWo3eE);
int APS5_VABI sceNpCppWebApiUnknown_wzlainWo3eE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("x+bH7Ykt6bc", sceNpCppWebApiUnknown_x_plus_bH7Ykt6bc);
int APS5_VABI sceNpCppWebApiUnknown_x_plus_bH7Ykt6bc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("x+e5jfR5gbc", sceNpCppWebApiUnknown_x_plus_e5jfR5gbc);
int APS5_VABI sceNpCppWebApiUnknown_x_plus_e5jfR5gbc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("x+jZ+7cfi9s", sceNpCppWebApiUnknown_x_plus_jZ_plus_7cfi9s);
int APS5_VABI sceNpCppWebApiUnknown_x_plus_jZ_plus_7cfi9s(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("x+nxQC52xh4", sceNpCppWebApiUnknown_x_plus_nxQC52xh4);
int APS5_VABI sceNpCppWebApiUnknown_x_plus_nxQC52xh4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("x0SVrzZN7bg", sceNpCppWebApiUnknown_x0SVrzZN7bg);
int APS5_VABI sceNpCppWebApiUnknown_x0SVrzZN7bg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("x4G-29np7pM", sceNpCppWebApiUnknown_x4G_minus_29np7pM);
int APS5_VABI sceNpCppWebApiUnknown_x4G_minus_29np7pM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("x5fMr5644iE", sceNpCppWebApiUnknown_x5fMr5644iE);
int APS5_VABI sceNpCppWebApiUnknown_x5fMr5644iE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("x6k8rREYAZs", sceNpCppWebApiUnknown_x6k8rREYAZs);
int APS5_VABI sceNpCppWebApiUnknown_x6k8rREYAZs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("x8OFjLd4sZ4", sceNpCppWebApiUnknown_x8OFjLd4sZ4);
int APS5_VABI sceNpCppWebApiUnknown_x8OFjLd4sZ4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("x9hkly4XJFc", sceNpCppWebApiUnknown_x9hkly4XJFc);
int APS5_VABI sceNpCppWebApiUnknown_x9hkly4XJFc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xBrYWSEri04", sceNpCppWebApiUnknown_xBrYWSEri04);
int APS5_VABI sceNpCppWebApiUnknown_xBrYWSEri04(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xCHE7jJ7j4E", sceNpCppWebApiUnknown_xCHE7jJ7j4E);
int APS5_VABI sceNpCppWebApiUnknown_xCHE7jJ7j4E(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xHr7sblfjwc", sceNpCppWebApiUnknown_xHr7sblfjwc);
int APS5_VABI sceNpCppWebApiUnknown_xHr7sblfjwc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xIKDGNGXNgE", sceNpCppWebApiUnknown_xIKDGNGXNgE);
int APS5_VABI sceNpCppWebApiUnknown_xIKDGNGXNgE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xIdBBTvAz7o", sceNpCppWebApiUnknown_xIdBBTvAz7o);
int APS5_VABI sceNpCppWebApiUnknown_xIdBBTvAz7o(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xNel84IThc0", sceNpCppWebApiUnknown_xNel84IThc0);
int APS5_VABI sceNpCppWebApiUnknown_xNel84IThc0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xR9aJZla4Gk", sceNpCppWebApiUnknown_xR9aJZla4Gk);
int APS5_VABI sceNpCppWebApiUnknown_xR9aJZla4Gk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xZ2OF6809Mk", sceNpCppWebApiUnknown_xZ2OF6809Mk);
int APS5_VABI sceNpCppWebApiUnknown_xZ2OF6809Mk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xZLL0Y9IImE", sceNpCppWebApiUnknown_xZLL0Y9IImE);
int APS5_VABI sceNpCppWebApiUnknown_xZLL0Y9IImE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xZqiZvmcp9k", sceNpCppWebApiUnknown_xZqiZvmcp9k);
int APS5_VABI sceNpCppWebApiUnknown_xZqiZvmcp9k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xae8xDbhfws", sceNpCppWebApiUnknown_xae8xDbhfws);
int APS5_VABI sceNpCppWebApiUnknown_xae8xDbhfws(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xcauS25MvZ0", sceNpCppWebApiUnknown_xcauS25MvZ0);
int APS5_VABI sceNpCppWebApiUnknown_xcauS25MvZ0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xfmGwgBnPp8", sceNpCppWebApiUnknown_xfmGwgBnPp8);
int APS5_VABI sceNpCppWebApiUnknown_xfmGwgBnPp8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xg9A5nBRkB0", sceNpCppWebApiUnknown_xg9A5nBRkB0);
int APS5_VABI sceNpCppWebApiUnknown_xg9A5nBRkB0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xhAcaIwnrgk", sceNpCppWebApiUnknown_xhAcaIwnrgk);
int APS5_VABI sceNpCppWebApiUnknown_xhAcaIwnrgk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xhlKg2g4Nnc", sceNpCppWebApiUnknown_xhlKg2g4Nnc);
int APS5_VABI sceNpCppWebApiUnknown_xhlKg2g4Nnc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xjCW0JsAcmI", sceNpCppWebApiUnknown_xjCW0JsAcmI);
int APS5_VABI sceNpCppWebApiUnknown_xjCW0JsAcmI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xjVhrq2KPJA", sceNpCppWebApiUnknown_xjVhrq2KPJA);
int APS5_VABI sceNpCppWebApiUnknown_xjVhrq2KPJA(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xjp4yqGNY2A", sceNpCppWebApiUnknown_xjp4yqGNY2A);
int APS5_VABI sceNpCppWebApiUnknown_xjp4yqGNY2A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xjs+AdvF4j0", sceNpCppWebApiUnknown_xjs_plus_AdvF4j0);
int APS5_VABI sceNpCppWebApiUnknown_xjs_plus_AdvF4j0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xmJUuG8cBgs", sceNpCppWebApiUnknown_xmJUuG8cBgs);
int APS5_VABI sceNpCppWebApiUnknown_xmJUuG8cBgs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xptStTyYP6Y", sceNpCppWebApiUnknown_xptStTyYP6Y);
int APS5_VABI sceNpCppWebApiUnknown_xptStTyYP6Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xugo4hJuBQQ", sceNpCppWebApiUnknown_xugo4hJuBQQ);
int APS5_VABI sceNpCppWebApiUnknown_xugo4hJuBQQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xxdwmfFNjjE", sceNpCppWebApiUnknown_xxdwmfFNjjE);
int APS5_VABI sceNpCppWebApiUnknown_xxdwmfFNjjE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("xy3jCkIDL44", sceNpCppWebApiUnknown_xy3jCkIDL44);
int APS5_VABI sceNpCppWebApiUnknown_xy3jCkIDL44(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("y0BDcAt6FGE", sceNpCppWebApiUnknown_y0BDcAt6FGE);
int APS5_VABI sceNpCppWebApiUnknown_y0BDcAt6FGE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("y0q33hmapKk", sceNpCppWebApiUnknown_y0q33hmapKk);
int APS5_VABI sceNpCppWebApiUnknown_y0q33hmapKk(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("y1MqEVNE55Y", sceNpCppWebApiUnknown_y1MqEVNE55Y);
int APS5_VABI sceNpCppWebApiUnknown_y1MqEVNE55Y(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("y3jG5RBYv60", sceNpCppWebApiUnknown_y3jG5RBYv60);
int APS5_VABI sceNpCppWebApiUnknown_y3jG5RBYv60(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("y7+O6u182Rg", sceNpCppWebApiUnknown_y7_plus_O6u182Rg);
int APS5_VABI sceNpCppWebApiUnknown_y7_plus_O6u182Rg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("y7xm2JcXgOM", sceNpCppWebApiUnknown_y7xm2JcXgOM);
int APS5_VABI sceNpCppWebApiUnknown_y7xm2JcXgOM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("y9Vc1aQH-Vw", sceNpCppWebApiUnknown_y9Vc1aQH_minus_Vw);
int APS5_VABI sceNpCppWebApiUnknown_y9Vc1aQH_minus_Vw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yBEI+rzE0tU", sceNpCppWebApiUnknown_yBEI_plus_rzE0tU);
int APS5_VABI sceNpCppWebApiUnknown_yBEI_plus_rzE0tU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yD3I2CRcKmw", sceNpCppWebApiUnknown_yD3I2CRcKmw);
int APS5_VABI sceNpCppWebApiUnknown_yD3I2CRcKmw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yDfMLEGDphw", sceNpCppWebApiUnknown_yDfMLEGDphw);
int APS5_VABI sceNpCppWebApiUnknown_yDfMLEGDphw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yE2GDqAMHwY", sceNpCppWebApiUnknown_yE2GDqAMHwY);
int APS5_VABI sceNpCppWebApiUnknown_yE2GDqAMHwY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yH83vkJ7cEY", sceNpCppWebApiUnknown_yH83vkJ7cEY);
int APS5_VABI sceNpCppWebApiUnknown_yH83vkJ7cEY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yJw2m6UWDYU", sceNpCppWebApiUnknown_yJw2m6UWDYU);
int APS5_VABI sceNpCppWebApiUnknown_yJw2m6UWDYU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yOUuI64kBuc", sceNpCppWebApiUnknown_yOUuI64kBuc);
int APS5_VABI sceNpCppWebApiUnknown_yOUuI64kBuc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yS0XpsJEDzo", sceNpCppWebApiUnknown_yS0XpsJEDzo);
int APS5_VABI sceNpCppWebApiUnknown_yS0XpsJEDzo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yU4-BCjWhHo", sceNpCppWebApiUnknown_yU4_minus_BCjWhHo);
int APS5_VABI sceNpCppWebApiUnknown_yU4_minus_BCjWhHo(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yU8y1LOWdq4", sceNpCppWebApiUnknown_yU8y1LOWdq4);
int APS5_VABI sceNpCppWebApiUnknown_yU8y1LOWdq4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yaLoGJ4WXyc", sceNpCppWebApiUnknown_yaLoGJ4WXyc);
int APS5_VABI sceNpCppWebApiUnknown_yaLoGJ4WXyc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yc7kZNvtoI8", sceNpCppWebApiUnknown_yc7kZNvtoI8);
int APS5_VABI sceNpCppWebApiUnknown_yc7kZNvtoI8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ydHbnzAObIs", sceNpCppWebApiUnknown_ydHbnzAObIs);
int APS5_VABI sceNpCppWebApiUnknown_ydHbnzAObIs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yfQPriQMuiU", sceNpCppWebApiUnknown_yfQPriQMuiU);
int APS5_VABI sceNpCppWebApiUnknown_yfQPriQMuiU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ygL-RUMNAn0", sceNpCppWebApiUnknown_ygL_minus_RUMNAn0);
int APS5_VABI sceNpCppWebApiUnknown_ygL_minus_RUMNAn0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ygLeNBGWgf8", sceNpCppWebApiUnknown_ygLeNBGWgf8);
int APS5_VABI sceNpCppWebApiUnknown_ygLeNBGWgf8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yixDW3EPcQ8", sceNpCppWebApiUnknown_yixDW3EPcQ8);
int APS5_VABI sceNpCppWebApiUnknown_yixDW3EPcQ8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yj9KGoSd8E8", sceNpCppWebApiUnknown_yj9KGoSd8E8);
int APS5_VABI sceNpCppWebApiUnknown_yj9KGoSd8E8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yk05VJ8evEs", sceNpCppWebApiUnknown_yk05VJ8evEs);
int APS5_VABI sceNpCppWebApiUnknown_yk05VJ8evEs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ykIRhYqogeM", sceNpCppWebApiUnknown_ykIRhYqogeM);
int APS5_VABI sceNpCppWebApiUnknown_ykIRhYqogeM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yl3T9brJay4", sceNpCppWebApiUnknown_yl3T9brJay4);
int APS5_VABI sceNpCppWebApiUnknown_yl3T9brJay4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ylTRxi27Neg", sceNpCppWebApiUnknown_ylTRxi27Neg);
int APS5_VABI sceNpCppWebApiUnknown_ylTRxi27Neg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ymB4A1QMkI0", sceNpCppWebApiUnknown_ymB4A1QMkI0);
int APS5_VABI sceNpCppWebApiUnknown_ymB4A1QMkI0(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ymTIKjlVMtc", sceNpCppWebApiUnknown_ymTIKjlVMtc);
int APS5_VABI sceNpCppWebApiUnknown_ymTIKjlVMtc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ynvhoQekbwg", sceNpCppWebApiUnknown_ynvhoQekbwg);
int APS5_VABI sceNpCppWebApiUnknown_ynvhoQekbwg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ypQ1+CIeMQ4", sceNpCppWebApiUnknown_ypQ1_plus_CIeMQ4);
int APS5_VABI sceNpCppWebApiUnknown_ypQ1_plus_CIeMQ4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yq0dlbH8P24", sceNpCppWebApiUnknown_yq0dlbH8P24);
int APS5_VABI sceNpCppWebApiUnknown_yq0dlbH8P24(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yqnYT+cUvIQ", sceNpCppWebApiUnknown_yqnYT_plus_cUvIQ);
int APS5_VABI sceNpCppWebApiUnknown_yqnYT_plus_cUvIQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ysAnsG6EfkM", sceNpCppWebApiUnknown_ysAnsG6EfkM);
int APS5_VABI sceNpCppWebApiUnknown_ysAnsG6EfkM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yt73vm77v4Q", sceNpCppWebApiUnknown_yt73vm77v4Q);
int APS5_VABI sceNpCppWebApiUnknown_yt73vm77v4Q(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("ytHxQmhBCGM", sceNpCppWebApiUnknown_ytHxQmhBCGM);
int APS5_VABI sceNpCppWebApiUnknown_ytHxQmhBCGM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yw2QhADovM8", sceNpCppWebApiUnknown_yw2QhADovM8);
int APS5_VABI sceNpCppWebApiUnknown_yw2QhADovM8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("yzYn2Pwc2LM", sceNpCppWebApiUnknown_yzYn2Pwc2LM);
int APS5_VABI sceNpCppWebApiUnknown_yzYn2Pwc2LM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("z+-YaF-ukxw", sceNpCppWebApiUnknown_z_plus__minus_YaF_minus_ukxw);
int APS5_VABI sceNpCppWebApiUnknown_z_plus__minus_YaF_minus_ukxw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("z+aXXhCwtKU", sceNpCppWebApiUnknown_z_plus_aXXhCwtKU);
int APS5_VABI sceNpCppWebApiUnknown_z_plus_aXXhCwtKU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("z3id8GlN+Hc", sceNpCppWebApiUnknown_z3id8GlN_plus_Hc);
int APS5_VABI sceNpCppWebApiUnknown_z3id8GlN_plus_Hc(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("z4rLQmYM8dU", sceNpCppWebApiUnknown_z4rLQmYM8dU);
int APS5_VABI sceNpCppWebApiUnknown_z4rLQmYM8dU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("z506+nKTUW4", sceNpCppWebApiUnknown_z506_plus_nKTUW4);
int APS5_VABI sceNpCppWebApiUnknown_z506_plus_nKTUW4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("z542ftrjD5A", sceNpCppWebApiUnknown_z542ftrjD5A);
int APS5_VABI sceNpCppWebApiUnknown_z542ftrjD5A(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("z5ornO9coA4", sceNpCppWebApiUnknown_z5ornO9coA4);
int APS5_VABI sceNpCppWebApiUnknown_z5ornO9coA4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("z8Ii6vFJINw", sceNpCppWebApiUnknown_z8Ii6vFJINw);
int APS5_VABI sceNpCppWebApiUnknown_z8Ii6vFJINw(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zBCt-Jv4PIg", sceNpCppWebApiUnknown_zBCt_minus_Jv4PIg);
int APS5_VABI sceNpCppWebApiUnknown_zBCt_minus_Jv4PIg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zCIiOlzrCLs", sceNpCppWebApiUnknown_zCIiOlzrCLs);
int APS5_VABI sceNpCppWebApiUnknown_zCIiOlzrCLs(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zCi5QdW+4ts", sceNpCppWebApiUnknown_zCi5QdW_plus_4ts);
int APS5_VABI sceNpCppWebApiUnknown_zCi5QdW_plus_4ts(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zDTxL-Jz1iM", sceNpCppWebApiUnknown_zDTxL_minus_Jz1iM);
int APS5_VABI sceNpCppWebApiUnknown_zDTxL_minus_Jz1iM(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zFDPaFShCH8", sceNpCppWebApiUnknown_zFDPaFShCH8);
int APS5_VABI sceNpCppWebApiUnknown_zFDPaFShCH8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zI5kPLikrZY", sceNpCppWebApiUnknown_zI5kPLikrZY);
int APS5_VABI sceNpCppWebApiUnknown_zI5kPLikrZY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zJGf8xjFnQE", sceNpCppWebApiUnknown_zJGf8xjFnQE);
int APS5_VABI sceNpCppWebApiUnknown_zJGf8xjFnQE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zJdh+LWNjow", sceNpCppWebApiUnknown_zJdh_plus_LWNjow);
int APS5_VABI sceNpCppWebApiUnknown_zJdh_plus_LWNjow(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zMSyHPD9hd4", sceNpCppWebApiUnknown_zMSyHPD9hd4);
int APS5_VABI sceNpCppWebApiUnknown_zMSyHPD9hd4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zOA2tHzOY8I", sceNpCppWebApiUnknown_zOA2tHzOY8I);
int APS5_VABI sceNpCppWebApiUnknown_zOA2tHzOY8I(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zS-rkk0AOoY", sceNpCppWebApiUnknown_zS_minus_rkk0AOoY);
int APS5_VABI sceNpCppWebApiUnknown_zS_minus_rkk0AOoY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zV4C5bcYMCI", sceNpCppWebApiUnknown_zV4C5bcYMCI);
int APS5_VABI sceNpCppWebApiUnknown_zV4C5bcYMCI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zXPERrs8Gh4", sceNpCppWebApiUnknown_zXPERrs8Gh4);
int APS5_VABI sceNpCppWebApiUnknown_zXPERrs8Gh4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zYTCgEvUBxU", sceNpCppWebApiUnknown_zYTCgEvUBxU);
int APS5_VABI sceNpCppWebApiUnknown_zYTCgEvUBxU(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zYttnqIaAS4", sceNpCppWebApiUnknown_zYttnqIaAS4);
int APS5_VABI sceNpCppWebApiUnknown_zYttnqIaAS4(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zc4gMADxk94", sceNpCppWebApiUnknown_zc4gMADxk94);
int APS5_VABI sceNpCppWebApiUnknown_zc4gMADxk94(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zfvBGft4XrY", sceNpCppWebApiUnknown_zfvBGft4XrY);
int APS5_VABI sceNpCppWebApiUnknown_zfvBGft4XrY(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zk7Es711EV8", sceNpCppWebApiUnknown_zk7Es711EV8);
int APS5_VABI sceNpCppWebApiUnknown_zk7Es711EV8(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zp-8udvcWSE", sceNpCppWebApiUnknown_zp_minus_8udvcWSE);
int APS5_VABI sceNpCppWebApiUnknown_zp_minus_8udvcWSE(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zq+qJ57PqpQ", sceNpCppWebApiUnknown_zq_plus_qJ57PqpQ);
int APS5_VABI sceNpCppWebApiUnknown_zq_plus_qJ57PqpQ(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zrEp8lu3vAI", sceNpCppWebApiUnknown_zrEp8lu3vAI);
int APS5_VABI sceNpCppWebApiUnknown_zrEp8lu3vAI(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zv9FDHSv88k", sceNpCppWebApiUnknown_zv9FDHSv88k);
int APS5_VABI sceNpCppWebApiUnknown_zv9FDHSv88k(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }
APS5_EXPORT("zzDN3aMsHzg", sceNpCppWebApiUnknown_zzDN3aMsHzg);
int APS5_VABI sceNpCppWebApiUnknown_zzDN3aMsHzg(std::uint64_t a, std::uint64_t b, std::uint64_t c) { return Unavailable(__func__, __builtin_return_address(0), a, b, c); }

}

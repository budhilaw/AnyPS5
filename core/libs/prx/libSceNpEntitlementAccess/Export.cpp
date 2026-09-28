#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Entitlement access answers as a full retail installation with no additional content: the
// host has no PSN entitlements to consult, and titles only read these at start-up.

static constexpr int NP_ENTITLEMENT_ACCESS_OK = 0;
static constexpr int NP_ENTITLEMENT_ACCESS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80D25002u);
static constexpr int NP_ENTITLEMENT_ACCESS_ERROR_NOT_FOUND = static_cast<int>(0x80D25007u);
static constexpr std::uint32_t NP_ENTITLEMENT_ACCESS_SKU_FLAG_FULL = 2;

extern "C" {

int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfo(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, NpEntitlementAccessAddcontEntitlementInfo* info) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)service_label;
    if (entitlement_label == nullptr || info == nullptr) return NP_ENTITLEMENT_ACCESS_ERROR_INVALID_ARGUMENT;
    std::memset(info, 0, sizeof(*info));
    return NP_ENTITLEMENT_ACCESS_ERROR_NOT_FOUND; // no additional content is installed
}

int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfoList(uint32_t service_label, NpEntitlementAccessAddcontEntitlementInfo* list, uint32_t list_num, uint32_t* hit_num) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)service_label;
    (void)list;
    (void)list_num;
    if (hit_num == nullptr) return NP_ENTITLEMENT_ACCESS_ERROR_INVALID_ARGUMENT;
    *hit_num = 0;
    return NP_ENTITLEMENT_ACCESS_OK;
}

int APS5_VABI sceNpEntitlementAccessGetSkuFlag(uint32_t* sku_flag) {
    Aps5TraceCall_nid_no_patch(__func__);
    if (sku_flag == nullptr) return NP_ENTITLEMENT_ACCESS_ERROR_INVALID_ARGUMENT;
    *sku_flag = NP_ENTITLEMENT_ACCESS_SKU_FLAG_FULL;
    return NP_ENTITLEMENT_ACCESS_OK;
}

int APS5_VABI sceNpEntitlementAccessInitialize(const NpEntitlementAccessInitParam* init_param, NpEntitlementAccessBootParam* boot_param) {
    Aps5TraceCall_nid_no_patch(__func__);
    (void)init_param;
    if (boot_param != nullptr) std::memset(boot_param, 0, sizeof(*boot_param));
    return NP_ENTITLEMENT_ACCESS_OK;
}

}

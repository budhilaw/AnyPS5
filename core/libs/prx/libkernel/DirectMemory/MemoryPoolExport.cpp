#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceKernelMemoryPoolBatch(const KernelMemoryPoolBatchEntry* entries, int num_entries, int* num_entries_out, int flags) {
 (void)entries;
 (void)num_entries;
 (void)num_entries_out;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelMemoryPoolCommit(void* addr, size_t len, int type, int prot, int flags) {
    APS5_LOG_OUT("pool commit %p+0x%zx type %d prot 0x%x flags 0x%x", addr, len, type, prot, flags);
 (void)addr;
 (void)len;
 (void)type;
 (void)prot;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelMemoryPoolDecommit(void* addr, size_t len, int flags) {
    APS5_LOG_OUT("pool decommit %p+0x%zx flags 0x%x", addr, len, flags);
 (void)addr;
 (void)len;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelMemoryPoolExpand(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int64_t* phys_addr_out) {
 (void)search_start;
 (void)search_end;
 (void)len;
 (void)alignment;
 (void)phys_addr_out;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelMemoryPoolGetBlockStats(KernelMemoryPoolBlockStats* output, size_t output_size) {
 (void)output;
 (void)output_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelMemoryPoolReserve(void* addr_in, size_t len, size_t alignment, int flags, void** addr_out) {
    APS5_LOG_OUT("pool reserve %p+0x%zx align 0x%zx flags 0x%x", addr_in, len, alignment, flags);
 (void)addr_in;
 (void)len;
 (void)alignment;
 (void)flags;
 (void)addr_out;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

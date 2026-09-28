#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <stdexcept>

// Guest operator new/delete on the application heap, so guest malloc/free interoperate.
extern "C" {

void* APS5_VABI _Znwm_nid_postfix(std::size_t size) {
    void* result = ApplicationHeapAllocate_nid_no_patch(size != 0 ? size : 1);
    if (result == nullptr) throw std::runtime_error("operator new: application heap is exhausted");
    return result;
}

void* APS5_VABI _Znam_nid_postfix(std::size_t size) { return _Znwm_nid_postfix(size); }
void APS5_VABI _ZdlPv_nid_postfix(void* pointer) { if (pointer != nullptr) ApplicationHeapFree_nid_no_patch(pointer); }
void APS5_VABI _ZdaPv_nid_postfix(void* pointer) { _ZdlPv_nid_postfix(pointer); }

}

// std::get_new_handler: no guest handler can be installed yet, so none is ever set.
extern "C" void* APS5_VABI _ZSt15get_new_handlerv_nid_postfix() { return nullptr; }

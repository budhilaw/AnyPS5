#include <cstdlib>
#include <cstdio>
#include "prx/libc/include/MemoryBackingPlatform.hpp"
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>

namespace GuestMemoryBacking::Platform {
namespace {


void check(kern_return_t result, const char* operation) {
    if (result != KERN_SUCCESS) throw std::runtime_error(std::string(operation) + ": " + mach_error_string(result));
}

void checkErrno(bool success, const char* operation) {
    if (!success) throw std::system_error(errno, std::generic_category(), operation);
}

}

Mapping Map(void* address, std::size_t bytes, std::size_t alignment, int protection) {
    if (bytes == 0 || bytes > static_cast<std::size_t>(std::numeric_limits<mach_vm_size_t>::max() / 2)) throw std::overflow_error("guest backing size overflow");
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) throw std::invalid_argument("guest backing alignment must be a power of two");
    if (address != nullptr && reinterpret_cast<std::uintptr_t>(address) % alignment != 0) throw std::invalid_argument("fixed guest view is not aligned");
    const auto task = mach_task_self();
    mach_vm_address_t alias = 0;
    check(mach_vm_allocate(task, &alias, bytes, VM_FLAGS_ANYWHERE), "mach_vm_allocate guest backing");
    mach_vm_address_t guest = reinterpret_cast<mach_vm_address_t>(address);
    try {
        vm_prot_t current = VM_PROT_NONE;
        vm_prot_t maximum = VM_PROT_NONE;
        const auto remapped = mach_vm_remap(task, &guest, bytes, static_cast<mach_vm_offset_t>(alignment - 1), address != nullptr ? VM_FLAGS_FIXED : VM_FLAGS_ANYWHERE, task, alias, FALSE, &current, &maximum, VM_INHERIT_NONE);
        if (remapped != KERN_SUCCESS && address != nullptr) {
            mach_vm_address_t region = reinterpret_cast<mach_vm_address_t>(address);
            mach_vm_size_t size = 0;
            vm_region_extended_info_data_t info{};
            mach_msg_type_number_t count = VM_REGION_EXTENDED_INFO_COUNT;
            mach_port_t object = MACH_PORT_NULL;
            const bool found = mach_vm_region(task, &region, &size, VM_REGION_EXTENDED_INFO, reinterpret_cast<vm_region_info_t>(&info), &count, &object) == KERN_SUCCESS;
            if (object != MACH_PORT_NULL) mach_port_deallocate(task, object);
            char text[224];
            std::snprintf(text, sizeof(text), "mach_vm_remap guest backing view 0x%llx+0x%zx: %s; first region at or above it: %s0x%llx+0x%llx prot %d tag %u", static_cast<unsigned long long>(reinterpret_cast<mach_vm_address_t>(address)), bytes, mach_error_string(remapped), found ? "" : "(none) ", static_cast<unsigned long long>(region), static_cast<unsigned long long>(size), info.protection, info.user_tag);
            throw std::runtime_error(text);
        }
        check(remapped, "mach_vm_remap guest backing view");
        try {
            if (address != nullptr && guest != reinterpret_cast<mach_vm_address_t>(address)) throw std::runtime_error("fixed guest backing reservation address mismatch");
            if (guest % alignment != 0) throw std::runtime_error("guest backing view is not aligned");
            if (std::getenv("ANYPS5_TRACE_PROTECT") != nullptr) std::fprintf(stderr, "[backing] view 0x%llx+0x%zx prot %d\n", static_cast<unsigned long long>(guest), bytes, protection);
            checkErrno(mprotect(reinterpret_cast<void*>(guest), bytes, protection) == 0, "mprotect guest backing view");
        } catch (...) {
            check(mach_vm_deallocate(task, guest, bytes), "mach_vm_deallocate failed guest view");
            throw;
        }
        return {static_cast<std::uint64_t>(guest), bytes, reinterpret_cast<void*>(alias), 0};
    } catch (...) {
        check(mach_vm_deallocate(task, alias, bytes), "mach_vm_deallocate failed guest alias");
        throw;
    }
}

void UnmapView(const Mapping& mapping) {
    check(mach_vm_deallocate(mach_task_self(), mapping.address, mapping.bytes), "mach_vm_deallocate guest view");
}

void UnmapAlias(const Mapping& mapping) {
    check(mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(mapping.alias), mapping.bytes), "mach_vm_deallocate guest alias");
}

void Unmap(const Mapping& mapping) {
    UnmapView(mapping);
    UnmapAlias(mapping);
}

void Protect(std::uint64_t address, std::size_t bytes, int protection) {
    if (std::getenv("ANYPS5_TRACE_PROTECT") != nullptr) std::fprintf(stderr, "[backing] protect 0x%llx+0x%zx prot %d\n", static_cast<unsigned long long>(address), bytes, protection);
    checkErrno(mprotect(reinterpret_cast<void*>(address), bytes, protection) == 0, "mprotect guest backing protect");
}

void Deactivate(std::uint64_t address, std::size_t bytes) {
    if (std::getenv("ANYPS5_TRACE_PROTECT") != nullptr) std::fprintf(stderr, "[backing] unmap 0x%llx+0x%zx\n", static_cast<unsigned long long>(address), bytes);
    checkErrno(mprotect(reinterpret_cast<void*>(address), bytes, PROT_NONE) == 0, "mprotect guest backing unmap");
}

}

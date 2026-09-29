#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_ITANIUM_CXXABI_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_ITANIUM_CXXABI_HPP

#include <cxxabi.h>
#include <typeinfo>

#if defined(_LIBCPP_VERSION)
namespace __cxxabiv1 {

class __class_type_info : public std::type_info {
public:
    ~__class_type_info() override;
    enum __sub_kind { __unknown = 0, __not_contained, __contained_ambig, __contained_virtual_mask = 1, __contained_public_mask = 2, __contained_mask = 4, __contained_private = 4, __contained_public = 6 };
    struct __upcast_result { const void* dst_ptr; __sub_kind part2dst; int src_details; const __class_type_info* base_type; };
    struct __dyncast_result { const void* dst_ptr; __sub_kind whole2dst; __sub_kind whole2src; __sub_kind dst2src; int whole_details; };
};

class __si_class_type_info : public __class_type_info {
public:
    ~__si_class_type_info() override;
    const __class_type_info* __base_type;
};

class __base_class_type_info {
public:
    const __class_type_info* __base_type;
    long __offset_flags;
    enum __offset_flags_masks { __virtual_mask = 0x1, __public_mask = 0x2, __hwm_bit = 2, __offset_shift = 8 };
};

class __vmi_class_type_info : public __class_type_info {
public:
    ~__vmi_class_type_info() override;
    unsigned __flags;
    unsigned __base_count;
    __base_class_type_info __base_info[1];
};

class __pbase_type_info : public std::type_info {
public:
    ~__pbase_type_info() override;
    unsigned __flags;
    const std::type_info* __pointee;
};

class __pointer_type_info : public __pbase_type_info {
public:
    ~__pointer_type_info() override;
};

class __pointer_to_member_type_info : public __pbase_type_info {
public:
    ~__pointer_to_member_type_info() override;
    const __class_type_info* __context;
};

}
#endif

#endif

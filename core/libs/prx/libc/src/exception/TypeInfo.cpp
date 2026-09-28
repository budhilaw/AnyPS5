#include "prx/libc/include/exceptions/Runtime.hpp"
#include <stdexcept>

namespace LibcException {
namespace {

using ClassTypeInfo = __cxxabiv1::__class_type_info;

void APS5_VABI DestroyTypeInfo(std::type_info*) {}

void APS5_VABI DeleteTypeInfo(std::type_info*) {
    throw std::runtime_error("deleting guest RTTI is not supported");
}

bool APS5_VABI IsPointerTypeInfo(const std::type_info*) { return false; }

bool APS5_VABI CatchTypeInfo(const std::type_info* self, const std::type_info* thrown, void** object, unsigned) {
    if (!self || !thrown || !object) throw std::invalid_argument("invalid RTTI catch arguments");
    return Match(self, thrown, *object);
}

bool APS5_VABI UpcastTypeInfo(const std::type_info* self, const ClassTypeInfo* base, void** object) {
    if (!self || !base || !object) throw std::invalid_argument("invalid RTTI upcast arguments");
    Search search {base};
    Bases(self, *object, search);
    if (search.count != 1) return false;
    *object = search.found;
    return true;
}

bool APS5_VABI UpcastTypeInfoResult(const ClassTypeInfo*, const ClassTypeInfo*, const void*, ClassTypeInfo::__upcast_result&) {
    throw std::runtime_error("RTTI __upcast_result is not supported");
}

bool APS5_VABI DynamicCastTypeInfo(const ClassTypeInfo*, std::ptrdiff_t, ClassTypeInfo::__sub_kind, const ClassTypeInfo*, const void*, const ClassTypeInfo*, const void*, ClassTypeInfo::__dyncast_result&) {
    throw std::runtime_error("RTTI __dyncast_result is not supported");
}

ClassTypeInfo::__sub_kind APS5_VABI FindPublicTypeInfo(const ClassTypeInfo*, std::ptrdiff_t, const void*, const ClassTypeInfo*, const void*) {
    throw std::runtime_error("RTTI __do_find_public_src is not supported");
}

}

struct TypeInfoVtable {
    std::ptrdiff_t offset;
    const std::type_info* type;
    decltype(&DestroyTypeInfo) destroy = DestroyTypeInfo;
    decltype(&DeleteTypeInfo) deleteObject = DeleteTypeInfo;
    decltype(&IsPointerTypeInfo) isPointer = IsPointerTypeInfo;
    decltype(&IsPointerTypeInfo) isFunction = IsPointerTypeInfo;
    decltype(&CatchTypeInfo) catchType = CatchTypeInfo;
    decltype(&UpcastTypeInfo) upcast = UpcastTypeInfo;
    decltype(&UpcastTypeInfoResult) upcastResult = UpcastTypeInfoResult;
    decltype(&DynamicCastTypeInfo) dynamicCast = DynamicCastTypeInfo;
    decltype(&FindPublicTypeInfo) findPublic = FindPublicTypeInfo;
};

static_assert(offsetof(TypeInfoVtable, destroy) == 2 * sizeof(void*));
static_assert(sizeof(TypeInfoVtable) == 11 * sizeof(void*));

}

#if defined(_LIBCPP_VERSION)
// libc++abi does not export the type_info objects of its RTTI classes. The guest vtables only need
// objects whose name() yields the mangled class name (see Kind()), so equivalent objects are laid
// out here in libc++'s type_info shape: a vtable pointer followed by the name pointer.
namespace {
struct HostTypeInfo { const void* vtable; const char* name; };
static_assert(sizeof(HostTypeInfo) == sizeof(std::type_info));
constexpr HostTypeInfo ClassTypeInfoObject{nullptr, "N10__cxxabiv117__class_type_infoE"};
constexpr HostTypeInfo SiClassTypeInfoObject{nullptr, "N10__cxxabiv120__si_class_type_infoE"};
constexpr HostTypeInfo VmiClassTypeInfoObject{nullptr, "N10__cxxabiv121__vmi_class_type_infoE"};
constexpr HostTypeInfo FundamentalTypeInfoObject{nullptr, "N10__cxxabiv123__fundamental_type_infoE"};
constexpr HostTypeInfo PointerTypeInfoObject{nullptr, "N10__cxxabiv119__pointer_type_infoE"};
constexpr HostTypeInfo FunctionTypeInfoObject{nullptr, "N10__cxxabiv120__function_type_infoE"};
constexpr HostTypeInfo EnumTypeInfoObject{nullptr, "N10__cxxabiv116__enum_type_infoE"};
}
#define APS5_RTTI_OBJECT(object) reinterpret_cast<const std::type_info*>(&object)
extern "C" {
LibcException::TypeInfoVtable _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix {0, APS5_RTTI_OBJECT(ClassTypeInfoObject)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix {0, APS5_RTTI_OBJECT(SiClassTypeInfoObject)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix {0, APS5_RTTI_OBJECT(VmiClassTypeInfoObject)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix {0, APS5_RTTI_OBJECT(FundamentalTypeInfoObject)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix {0, APS5_RTTI_OBJECT(PointerTypeInfoObject)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__function_type_infoE_nid_postfix {0, APS5_RTTI_OBJECT(FunctionTypeInfoObject)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv116__enum_type_infoE_nid_postfix {0, APS5_RTTI_OBJECT(EnumTypeInfoObject)};
}
#else
extern "C" {
LibcException::TypeInfoVtable _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix {0, &typeid(__cxxabiv1::__class_type_info)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix {0, &typeid(__cxxabiv1::__si_class_type_info)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix {0, &typeid(__cxxabiv1::__vmi_class_type_info)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix {0, &typeid(__cxxabiv1::__fundamental_type_info)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix {0, &typeid(__cxxabiv1::__pointer_type_info)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__function_type_infoE_nid_postfix {0, &typeid(__cxxabiv1::__function_type_info)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv116__enum_type_infoE_nid_postfix {0, &typeid(__cxxabiv1::__enum_type_info)};
}
#endif

// Guest type_info objects of the fundamental types (a vtable pointer and the mangled name, the
// Itanium layout) and of const char* (a __pointer_type_info with the const qualifier flag).
namespace {
struct GuestFundamentalTypeInfo { const void* vtable; const char* name; };
struct GuestPointerTypeInfo { const void* vtable; const char* name; unsigned flags; const void* pointee; };
}
extern "C" {
#define APS5_FUNDAMENTAL_TYPE_INFO(mangled, text) \
    GuestFundamentalTypeInfo _ZTI##mangled##_nid_postfix {reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix) + 2, text};
APS5_FUNDAMENTAL_TYPE_INFO(v, "v")
APS5_FUNDAMENTAL_TYPE_INFO(b, "b")
APS5_FUNDAMENTAL_TYPE_INFO(i, "i")
APS5_FUNDAMENTAL_TYPE_INFO(l, "l")
APS5_FUNDAMENTAL_TYPE_INFO(f, "f")
APS5_FUNDAMENTAL_TYPE_INFO(d, "d")
APS5_FUNDAMENTAL_TYPE_INFO(Dn, "Dn")
#undef APS5_FUNDAMENTAL_TYPE_INFO
static GuestFundamentalTypeInfo CharTypeInfo {reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix) + 2, "c"};
GuestPointerTypeInfo _ZTIPKc_nid_postfix {reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix) + 2, "PKc", 1u, &CharTypeInfo};
}

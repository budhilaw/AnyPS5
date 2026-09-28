#if defined(__APPLE__)
#include "prx/libc/include/specifics/darwin/GuestImage.hpp"
#endif
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <mutex>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <nid/NidCompute.hpp>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#endif

// Titles load their own PRX modules (plugins, IL2CPP assemblies) at run time. A module is used in
// its relinked form: the host library that the relinker produced from the PRX, placed next to the
// system libraries under the executable's `libs` directory and named like the PRX.
namespace {

constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
constexpr int SCE_KERNEL_ERROR_ESRCH = 0x80020003;

struct LoadedModule {
    void* native;
    std::string name;
    unsigned references;
};

std::mutex modulesMutex;
std::map<KernelModule, LoadedModule> modules;
KernelModule nextHandle = 0x40000000;

std::filesystem::path executableDirectory() {
#if defined(_WIN32)
    wchar_t buffer[32768];
    const auto length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) throw std::runtime_error("module loader: cannot locate the executable");
    return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
    const char* path = _dyld_get_image_name(0);
    if (path == nullptr) throw std::runtime_error("module loader: cannot locate the executable");
    return std::filesystem::absolute(path).parent_path();
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}

void* nativeSymbol(void* native, const std::string& name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(native), name.c_str()));
#else
    return ::dlsym(native, name.c_str());
#endif
}

using ModuleEntry = int (APS5_VABI *)(std::size_t, const void*);

#if defined(__APPLE__)
using ModuleInit = int (APS5_VABI *)(std::size_t, const void*, ModuleEntry);

// Runs a relinked module's `_init(args, argp, start)`: its constructors, then module_start.
int startModule(void* native, const std::string& path, std::size_t args, const void* argp) {
    auto* start = nativeSymbol(native, Nid::ComputeNid("module_start", ""));
    if (auto* init = GuestImage::FindModuleInit(path.c_str())) return reinterpret_cast<ModuleInit>(init)(args, argp, reinterpret_cast<ModuleEntry>(start));
    return start != nullptr ? reinterpret_cast<ModuleEntry>(start)(args, argp) : 0;
}

// Guest modules that dyld loaded as dependencies of another module never went through the
// loader, so their constructors have not run. The console's loader starts every module it maps,
// dependencies first, with empty arguments; this does the same for the images that are new.
// Requires modulesMutex to be held.
void startDependencies(const std::string& skipName) {
    std::map<std::string, std::uint32_t> images;
    const auto count = _dyld_image_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        const char* imagePath = _dyld_get_image_name(index);
        if (imagePath == nullptr) continue;
        const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(index));
        unsigned long size = 0;
        if (header == nullptr || header->magic != MH_MAGIC_64 || getsectiondata(header, "__ANYPS5", "__init", &size) == nullptr) continue;
        images.emplace(std::filesystem::path(imagePath).filename().string(), index);
    }
    std::set<std::string> visiting;
    const std::function<void(const std::string&)> start = [&](const std::string& name) {
        if (name == skipName || !visiting.insert(name).second) return;
        for (const auto& [handle, module] : modules) if (module.name == name) return;
        const auto found = images.find(name);
        if (found == images.end()) return;
        const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(found->second));
        const auto* command = reinterpret_cast<const load_command*>(header + 1);
        for (std::uint32_t index = 0; index < header->ncmds; ++index) {
            if (command->cmd == LC_LOAD_DYLIB || command->cmd == LC_LOAD_WEAK_DYLIB || command->cmd == LC_REEXPORT_DYLIB) {
                const auto* dylib = reinterpret_cast<const dylib_command*>(command);
                const std::string dependency = std::filesystem::path(reinterpret_cast<const char*>(command) + dylib->dylib.name.offset).filename().string();
                if (images.count(dependency) != 0) start(dependency);
            }
            command = reinterpret_cast<const load_command*>(reinterpret_cast<const char*>(command) + command->cmdsize);
        }
        const char* imagePath = _dyld_get_image_name(found->second);
        void* native = ::dlopen(imagePath, RTLD_NOW | RTLD_GLOBAL | RTLD_NOLOAD);
        if (native == nullptr) return;
        APS5_LOG_OUT("starting dependency module %s", name.c_str());
        startModule(native, imagePath, 0, nullptr);
        modules.emplace(nextHandle++, LoadedModule{native, name, 1});
    };
    for (const auto& [name, index] : images) start(name);
}
#endif

}

extern "C" {

KernelModule APS5_VABI sceKernelLoadStartModule(const char* module_file_name, size_t args, const void* argp, uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
    APS5_LOG_OUT("sceKernelLoadStartModule %s args=%zu argp=%p flags=0x%x caller=%p", module_file_name ? module_file_name : "(null)", args, argp, flags, __builtin_return_address(0));
    (void)opt;
    if (module_file_name == nullptr || *module_file_name == '\0') return SCE_KERNEL_ERROR_EINVAL;
    if (flags != 0) throw std::runtime_error("sceKernelLoadStartModule: unsupported flags");
    const auto name = std::filesystem::path(module_file_name).filename().string();
    {
        // Loading a module that is already loaded returns its handle; the console does not start
        // it again (a plugin's module_start loads its own dependencies this way).
        std::lock_guard lock(modulesMutex);
        for (auto& [handle, module] : modules) {
            if (module.name != name) continue;
            ++module.references;
            if (res != nullptr) *res = 0;
            return handle;
        }
    }
    const auto candidate = executableDirectory() / "libs" / name;
    if (!std::filesystem::exists(candidate))
        throw std::runtime_error("sceKernelLoadStartModule: " + std::string(module_file_name) + " is not available; relink the module and place it at " + candidate.string());
#ifdef _WIN32
    void* native = LoadLibraryW(candidate.wstring().c_str());
    if (native == nullptr) throw std::runtime_error("sceKernelLoadStartModule: cannot load " + candidate.string());
#else
    // Global: the console makes every loaded module's exports visible to modules loaded later.
    void* native = ::dlopen(candidate.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (native == nullptr) throw std::runtime_error(std::string("sceKernelLoadStartModule: ") + ::dlerror());
#endif
    // The console's loader calls the module's `_init(args, argp, start)`, which runs the
    // constructors and then module_start with the caller's arguments. dyld cannot pass them, so
    // the relinker publishes `_init` and it is called here; modules without one start directly.
    int startResult = 0;
#if defined(__APPLE__)
    {
        std::lock_guard lock(modulesMutex);
        startDependencies(name);
    }
    startResult = startModule(native, candidate.string(), args, argp);
#else
    if (auto* start = nativeSymbol(native, Nid::ComputeNid("module_start", ""))) startResult = reinterpret_cast<ModuleEntry>(start)(args, argp);
#endif
    if (res != nullptr) *res = startResult;
    std::lock_guard lock(modulesMutex);
    const auto handle = nextHandle++;
    modules.emplace(handle, LoadedModule{native, name, 1});
    return handle;
}

int APS5_VABI sceKernelStopUnloadModule(KernelModule handle, size_t args, const void* argp, uint32_t flags, const KernelUnloadModuleOpt* opt, int* res) {
    (void)opt;
    if (flags != 0) throw std::runtime_error("sceKernelStopUnloadModule: unsupported flags");
    LoadedModule module{};
    {
        std::lock_guard lock(modulesMutex);
        const auto found = modules.find(handle);
        if (found == modules.end()) return SCE_KERNEL_ERROR_ESRCH;
        if (--found->second.references != 0) {
            if (res != nullptr) *res = 0;
            return 0;
        }
        module = found->second;
        modules.erase(found);
    }
    int stopResult = 0;
    if (auto* stop = nativeSymbol(module.native, Nid::ComputeNid("module_stop", ""))) stopResult = reinterpret_cast<ModuleEntry>(stop)(args, argp);
    if (res != nullptr) *res = stopResult;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(module.native));
#else
    ::dlclose(module.native);
#endif
    return 0;
}

int APS5_VABI sceKernelDlsym(KernelModule handle, const char* symbol, void** addr) {
    if (symbol == nullptr || addr == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    void* native = nullptr;
    {
        std::lock_guard lock(modulesMutex);
        const auto found = modules.find(handle);
        if (found == modules.end()) return SCE_KERNEL_ERROR_ESRCH;
        native = found->second.native;
    }
    void* result = nativeSymbol(native, Nid::ComputeNid(symbol, ""));
    if (result == nullptr) result = nativeSymbol(native, symbol);
    if (result == nullptr) return SCE_KERNEL_ERROR_ESRCH;
    *addr = result;
    return 0;
}

int APS5_VABI sceKernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
    (void)addr; (void)flags; (void)info;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetModuleInfoFromAddr(uint64_t addr, int n, ModuleInfo* r) {
    (void)addr; (void)n; (void)r;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

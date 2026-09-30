#include "prx/libSceAgcDriver/Execution/include/VulkanLibrary.hpp"
#include <SDL.h>
#include <array>
#include <mutex>
#include <stdexcept>
#include <string>
#if defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace AgcDriver {
namespace {

#if defined(_WIN32)
constexpr std::array<const char*, 1> Candidates{"vulkan-1.dll"};
#elif defined(__APPLE__)
constexpr std::array<const char*, 7> Candidates{
    "libvulkan.1.dylib",
    "libvulkan.dylib",
    "libMoltenVK.dylib",
    "/opt/homebrew/lib/libvulkan.1.dylib",
    "/opt/homebrew/lib/libMoltenVK.dylib",
    "/usr/local/lib/libvulkan.1.dylib",
    "/usr/local/lib/libMoltenVK.dylib",
};
#else
constexpr std::array<const char*, 1> Candidates{"libvulkan.so.1"};
#endif

constexpr const char* SdlVulkanLibraryVariable = "SDL_VULKAN_LIBRARY";

bool loadable(const char* name, std::string& errors) {
    void* library = SDL_LoadObject(name);
    if (library == nullptr) {
        errors += std::string("\n  ") + name + ": " + SDL_GetError();
        return false;
    }
    const bool usable = SDL_LoadFunction(library, "vkGetInstanceProcAddr") != nullptr;
    if (!usable) errors += std::string("\n  ") + name + ": vkGetInstanceProcAddr missing";
    SDL_UnloadObject(library);
    return usable;
}

std::string resolve() {
    std::string errors;
#if defined(__APPLE__)
    if (const char* sdk = SDL_getenv("VULKAN_SDK"); sdk != nullptr && *sdk != '\0') {
        const std::string loader = std::string(sdk) + "/lib/libvulkan.1.dylib";
        if (loadable(loader.c_str(), errors)) {
            if (SDL_getenv("VK_ICD_FILENAMES") == nullptr) {
                const std::string icd = std::string(sdk) + "/share/vulkan/icd.d/MoltenVK_icd.json";
                if (SDL_setenv("VK_ICD_FILENAMES", icd.c_str(), 1) != 0) throw std::runtime_error("Vulkan loader: cannot publish the SDK ICD manifest");
            }
            if (SDL_setenv(SdlVulkanLibraryVariable, loader.c_str(), 1) != 0) throw std::runtime_error("Vulkan loader: cannot publish the library name to SDL");
            return loader;
        }
    }
#endif
    if (const char* requested = SDL_getenv(SdlVulkanLibraryVariable); requested != nullptr && *requested != '\0') {
        if (loadable(requested, errors)) return requested;
        throw std::runtime_error(std::string("Vulkan loader: ") + SdlVulkanLibraryVariable + " is not loadable:" + errors);
    }
    for (const char* candidate : Candidates) {
        if (!loadable(candidate, errors)) continue;
        if (SDL_setenv(SdlVulkanLibraryVariable, candidate, 1) != 0) throw std::runtime_error("Vulkan loader: cannot publish the library name to SDL");
        return candidate;
    }
    throw std::runtime_error("Vulkan loader: no usable Vulkan library found:" + errors);
}

}

const char* ResolveVulkanLibrary_nid_no_patch() {
    static const std::string resolved = resolve();
    return resolved.c_str();
}

#if defined(__APPLE__)
namespace {

using Object = void*;
using Selector = void*;

struct MetalRuntime {
    Object (*createDevice)() = nullptr;
    Selector (*selector)(const char*) = nullptr;
    void* (*classOf)(Object) = nullptr;
    void* (*instanceMethod)(void*, Selector) = nullptr;
    void* (*setImplementation)(void*, void*) = nullptr;
    void* send = nullptr;
    Selector retainReferences = nullptr;
    void* commandBuffer = nullptr;
};

MetalRuntime metal;

Object retainedCommandBuffer(Object queue, Selector command, Object descriptor) {
    reinterpret_cast<void (*)(Object, Selector, bool)>(metal.send)(descriptor, metal.retainReferences, true);
    return reinterpret_cast<Object (*)(Object, Selector, Object)>(metal.commandBuffer)(queue, command, descriptor);
}

}

void RetainMetalCommandReferences() {
    static std::once_flag once;
    std::call_once(once, [] {
        metal.createDevice = reinterpret_cast<Object (*)()>(dlsym(RTLD_DEFAULT, "MTLCreateSystemDefaultDevice"));
        metal.selector = reinterpret_cast<Selector (*)(const char*)>(dlsym(RTLD_DEFAULT, "sel_registerName"));
        metal.classOf = reinterpret_cast<void* (*)(Object)>(dlsym(RTLD_DEFAULT, "object_getClass"));
        metal.instanceMethod = reinterpret_cast<void* (*)(void*, Selector)>(dlsym(RTLD_DEFAULT, "class_getInstanceMethod"));
        metal.setImplementation = reinterpret_cast<void* (*)(void*, void*)>(dlsym(RTLD_DEFAULT, "method_setImplementation"));
        metal.send = dlsym(RTLD_DEFAULT, "objc_msgSend");
        if (!metal.createDevice || !metal.selector || !metal.classOf || !metal.instanceMethod || !metal.setImplementation || !metal.send) throw std::runtime_error("Vulkan loader: the Metal runtime is unavailable");
        const auto send = reinterpret_cast<Object (*)(Object, Selector)>(metal.send);
        const auto device = metal.createDevice();
        if (device == nullptr) throw std::runtime_error("Vulkan loader: no Metal device");
        const auto queue = send(device, metal.selector("newCommandQueue"));
        const auto method = queue != nullptr ? metal.instanceMethod(metal.classOf(queue), metal.selector("commandBufferWithDescriptor:")) : nullptr;
        if (method != nullptr) {
            metal.retainReferences = metal.selector("setRetainedReferences:");
            metal.commandBuffer = metal.setImplementation(method, reinterpret_cast<void*>(&retainedCommandBuffer));
        }
        if (queue != nullptr) send(queue, metal.selector("release"));
        send(device, metal.selector("release"));
        if (method == nullptr) throw std::runtime_error("Vulkan loader: Metal command queues cannot be configured");
    });
}
#else
void RetainMetalCommandReferences() {}
#endif

}

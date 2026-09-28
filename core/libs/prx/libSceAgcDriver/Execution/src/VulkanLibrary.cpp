#include "prx/libSceAgcDriver/Execution/include/VulkanLibrary.hpp"
#include <SDL.h>
#include <array>
#include <stdexcept>
#include <string>

namespace AgcDriver {
namespace {

#if defined(_WIN32)
constexpr std::array<const char*, 1> Candidates{"vulkan-1.dll"};
#elif defined(__APPLE__)
// The Khronos loader is preferred because it provides layers and ICD selection. MoltenVK is a
// complete ICD and works standalone when no loader is installed. Homebrew prefixes are not on
// the default dyld search path, so they are probed explicitly.
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
    // A LunarG SDK (VULKAN_SDK from its setup-env.sh) carries a universal loader and MoltenVK; its
    // loader needs the ICD manifest when the environment does not name one.
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
    // SDL 2 reads the library to use for SDL_WINDOW_VULKAN windows from this environment variable.
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

const char* ResolveVulkanLibrary() {
    static const std::string resolved = resolve();
    return resolved.c_str();
}

}

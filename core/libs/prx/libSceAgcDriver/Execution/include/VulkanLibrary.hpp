#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VULKANLIBRARY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VULKANLIBRARY_HPP

namespace AgcDriver {

// Locates the Vulkan loader (or a standalone ICD such as MoltenVK) the host can load,
// tells SDL to use the same library for SDL_WINDOW_VULKAN windows and returns its name.
// The result is cached; throws std::runtime_error when no usable library exists.
const char* ResolveVulkanLibrary();

}

#endif

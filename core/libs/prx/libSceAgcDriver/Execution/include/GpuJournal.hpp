#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GPUJOURNAL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GPUJOURNAL_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <chrono>
#include <cstdint>
#include <string>

namespace AgcDriver::GpuJournal {

inline thread_local std::uint64_t CurrentProgram = 0;
inline PFN_vkCmdInsertDebugUtilsLabelEXT CommandLabel = nullptr;

inline void Label(VkCommandBuffer commands, const char* text) {
    if (CommandLabel == nullptr) return;
    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = text;
    CommandLabel(commands, &label);
}

void Record(std::string description);

void Dump(const char* heading);

void Watched(const char* what, std::chrono::seconds limit, void (*wait)(void*), void* context);

template <typename Wait>
void Watched(const char* what, std::chrono::seconds limit, Wait&& wait) {
    Watched(what, limit, [](void* context) { (*static_cast<Wait*>(context))(); }, &wait);
}

}

#endif

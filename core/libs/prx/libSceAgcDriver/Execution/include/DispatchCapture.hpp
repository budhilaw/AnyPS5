#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DISPATCHCAPTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DISPATCHCAPTURE_HPP

#include "prx/libSceAgcDriver/Graphics/include/CaptureFormat.hpp"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace AgcDriver {

struct DispatchCaptureSelector {
    std::uint64_t value = 0;
    std::uint32_t count = 1;
};

struct DispatchCaptureSettings {
    std::vector<DispatchCaptureSelector> selectors;
    std::vector<std::string> errors;
    std::filesystem::path root;
    double after = 0.0;
};

DispatchCaptureSettings ParseDispatchCaptureSettings(const char* selection, const char* directory, const char* after);
bool DispatchCaptureEnabled();
std::shared_ptr<const Graphics::CaptureTarget> SelectDispatchCapture(std::uint64_t program, std::uint64_t codeHash, const std::function<std::string()>& request);
std::string DispatchReplayCommand(const std::filesystem::path& capture);

}

#endif

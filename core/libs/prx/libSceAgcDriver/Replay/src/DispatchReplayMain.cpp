#include "prx/libSceAgcDriver/Replay/include/DispatchReplay.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanLibrary.hpp"
#include <SDL_error.h>
#include <SDL_loadso.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace AgcDriver::Replay;

void printUsage() {
    std::fprintf(stderr,
        "usage: dispatch_replay [--captured-spirv | --spirv <file>] [--write-spirv <directory>] [--iterations <n>] [--warmup <n>]\n"
        "                       [--device-local] <capture directory or directory of captures>...\n"
        "Replays compute dispatches captured with ANYPS5_CAPTURE_DISPATCH=<program address or code hash>[:count],... (comma\n"
        "separated; ANYPS5_CAPTURE_DIR sets the output directory, by default dispatch-captures under the game working\n"
        "directory, and ANYPS5_CAPTURE_AFTER=<seconds> delays the captures).\n"
        "Each capture is recompiled with the current shader recompiler (or run with the captured SPIR-V, or a SPIR-V file),\n"
        "its buffers, images and samplers are recreated with the captured contents and memory placement, and the dispatch\n"
        "runs --warmup + --iterations times (2 + 20 by default) with every written range restored before each run.\n"
        "The GPU time of each run is measured with timestamps; the minimum and median are reported. After the last run\n"
        "everything the dispatch wrote is compared byte for byte with the contents the game produced.\n"
        "--device-local places every buffer in device-local memory instead of where the driver placed it.\n"
        "The exit code is 0 when every replay matches its capture, 2 when an output differs and 1 on errors.\n");
    std::fflush(stderr);
}

bool parseCount(const char* text, std::uint32_t& value) {
    char* end = nullptr;
    const auto parsed = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || parsed > 100000ul) return false;
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

}

int main(int argc, char** argv) {
    ReplayOptions options;
    std::vector<std::filesystem::path> inputs;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        const bool hasValue = index + 1 < argc;
        if (argument == "--captured-spirv") {
            options.shader = ReplayShader::Captured;
        } else if (argument == "--spirv" && hasValue) {
            options.shader = ReplayShader::File;
            options.spirvFile = argv[++index];
        } else if (argument == "--write-spirv" && hasValue) {
            options.writeSpirv = argv[++index];
        } else if (argument == "--iterations" && hasValue) {
            if (!parseCount(argv[++index], options.iterations) || options.iterations == 0) {
                printUsage();
                return 1;
            }
        } else if (argument == "--warmup" && hasValue) {
            if (!parseCount(argv[++index], options.warmup)) {
                printUsage();
                return 1;
            }
        } else if (argument == "--device-local") {
            options.memory = ReplayMemory::DeviceLocal;
        } else if (argument.starts_with("--")) {
            printUsage();
            return 1;
        } else {
            inputs.emplace_back(argument);
        }
    }
    if (inputs.empty()) {
        printUsage();
        return 1;
    }
    try {
        void* library = SDL_LoadObject(AgcDriver::ResolveVulkanLibrary_nid_no_patch());
        if (library == nullptr) throw std::runtime_error(std::string("Vulkan loader: ") + SDL_GetError());
        const auto instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
        int exitCode = 0;
        {
            ReplayDevice device(instanceProc);
            std::printf("device: %s\n", device.Describe().c_str());
            std::fflush(stdout);
            std::size_t replayed = 0;
            std::size_t identical = 0;
            std::size_t mismatched = 0;
            std::size_t failed = 0;
            for (const auto& input : inputs) {
                std::vector<std::filesystem::path> captures;
                try {
                    captures = FindCaptures(input);
                } catch (const std::exception& error) {
                    std::printf("%s\n", error.what());
                    std::fflush(stdout);
                    ++failed;
                    continue;
                }
                for (const auto& capture : captures) {
                    try {
                        const auto result = ReplayCapture(device, capture, options);
                        ++replayed;
                        ++(result.mismatches.empty() ? identical : mismatched);
                        std::printf("%s\n", DescribeReplay(result).c_str());
                    } catch (const std::exception& error) {
                        ++failed;
                        std::printf("%s: FAILED: %s\n", capture.generic_string().c_str(), error.what());
                    }
                    std::fflush(stdout);
                }
            }
            std::printf("%zu capture(s) replayed: %zu identical, %zu mismatched, %zu failed\n", replayed, identical, mismatched, failed);
            std::fflush(stdout);
            exitCode = failed != 0 ? 1 : mismatched != 0 ? 2 : 0;
        }
        SDL_UnloadObject(library);
        return exitCode;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        std::fflush(stderr);
        return 1;
    }
}

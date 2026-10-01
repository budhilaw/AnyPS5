#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_REPLAY_INCLUDE_DISPATCHREPLAY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_REPLAY_INCLUDE_DISPATCHREPLAY_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace AgcDriver::Replay {

enum class ReplayShader { Recompile, Captured, File };
enum class ReplayMemory { Captured, DeviceLocal };

struct ReplayOptions {
    ReplayShader shader = ReplayShader::Recompile;
    ReplayMemory memory = ReplayMemory::Captured;
    std::filesystem::path spirvFile;
    std::filesystem::path writeSpirv;
    std::uint32_t iterations = 20;
    std::uint32_t warmup = 2;
};

struct ReplayResult {
    std::filesystem::path directory;
    std::uint64_t program = 0;
    std::uint64_t codeHash = 0;
    std::array<std::uint32_t, 3> groups{};
    std::size_t spirvWords = 0;
    std::size_t capturedSpirvWords = 0;
    bool spirvMatchesCapture = false;
    std::uint32_t lanes = 1;
    double compileMilliseconds = 0.0;
    std::vector<double> milliseconds;
    double minimum = 0.0;
    double median = 0.0;
    bool timestamps = false;
    std::size_t compared = 0;
    std::uint64_t comparedBytes = 0;
    std::vector<std::string> mismatches;
    std::vector<std::string> notes;
};

class ReplayDevice {
public:
    explicit ReplayDevice(PFN_vkGetInstanceProcAddr instanceProc);
    ~ReplayDevice();
    ReplayDevice(const ReplayDevice&) = delete;
    ReplayDevice& operator=(const ReplayDevice&) = delete;
    std::string Describe() const;
    std::uint32_t SubgroupSize() const;
    struct State;
    State& Internal() { return *state; }

private:
    std::unique_ptr<State> state;
};

ReplayResult ReplayCapture(ReplayDevice& device, const std::filesystem::path& directory, const ReplayOptions& options);
std::vector<std::filesystem::path> FindCaptures(const std::filesystem::path& path);
std::string DescribeReplay(const ReplayResult& result);

}

#endif

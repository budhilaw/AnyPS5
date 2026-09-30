#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VULKANDEVICE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VULKANDEVICE_HPP

#define VK_NO_PROTOTYPES
#include <array>
#include <span>
#include <functional>
#include <vulkan/vulkan.h>
#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include <memory>

namespace AgcDriver {

class VulkanDevice {
public:
    explicit VulkanDevice(const PresentationWindow* window = nullptr);
    ~VulkanDevice();
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    ShaderRecompiler::SpirvTarget Target() const;
    void WaitIdle();
    void WaitDraws();
    void FlushDraws();
    std::uint64_t SubmitTicket();
    void WaitTicket(std::uint64_t ticket);
    void Defer(std::function<void()> action);
    bool HasPendingWork();
    void Collect();
    void RecordBarrier();
    void WriteOcclusionDump(std::uint64_t destination);
    void ResolveGpuWrites(std::uint64_t address, std::size_t bytes);
    void GdsTransfer(std::span<const std::uint32_t> packet);
    std::array<std::uint32_t, 3> IndirectDispatchGroups(std::uint64_t address);
    void AcquireGpuMemory();
    void ResolveMemory(std::uint64_t address, std::size_t bytes, bool writable);
    bool NeedsResolve(std::uint64_t address, std::size_t bytes) const;
    void* Window() const;
    void Resize(std::uint32_t width, std::uint32_t height, PresentModeRequest request);
    bool Presentable() const;
    VkPresentModeKHR PresentMode() const;
    void PresentClear(std::uint32_t width, std::uint32_t height, bool opaque);
    void PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels);
    void PresentDisplayBuffer(const DisplayBuffer& buffer);
    void Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});
    void Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});
    void EnqueueDraw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});
    void ValidateDraw(const Graphics::State& graphics, std::span<const Graphics::CompiledShader> shaders) const;

private:
    const Graphics::Context& graphicsContext() const;
    Graphics::Context buildContext() const;
    void present(std::uint32_t width, std::uint32_t height, bool opaque, std::span<const std::byte> pixels, const DisplayBuffer* display = nullptr);
    struct State;
    std::unique_ptr<State> state;
};

}

#endif

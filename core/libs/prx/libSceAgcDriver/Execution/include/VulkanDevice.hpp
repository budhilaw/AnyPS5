#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VULKANDEVICE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VULKANDEVICE_HPP

#define VK_NO_PROTOTYPES
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
    // Submits the work queued so far without waiting for it (a flip: presentation waits for it).
    void FlushDraws();
    // Submits the work queued so far; WaitTicket waits for it to complete without holding the
    // device's locks, so the driver keeps recording meanwhile.
    std::uint64_t SubmitTicket();
    void WaitTicket(std::uint64_t ticket);
    // Runs `action` when the GPU work queued so far completes (see DrawQueue::EnqueueCompletion).
    void Defer(std::function<void()> action);
    bool HasPendingWork();
    // Retires completed GPU work (runs its write-backs and completion actions) without waiting.
    void Collect();
    // A GPU-side cache/pipeline barrier between the work recorded before and after it.
    void RecordBarrier();
    // Waits for pending GPU work only if it writes [address, address + bytes).
    void ResolveGpuWrites(std::uint64_t address, std::size_t bytes);
    // A DMA_DATA packet with a GDS source or destination: completes the GPU work recorded so far
    // (GDS holds counters that work updates) and copies through the host-visible GDS buffer.
    void GdsTransfer(std::span<const std::uint32_t> packet);
    void AcquireGpuMemory();
    void ResolveMemory(std::uint64_t address, std::size_t bytes, bool writable);
    // Whether a host access to the range may need ResolveMemory (queued GPU writes or resident
    // render targets overlap it), without the device's lock: a list published as they change. An
    // access racing a change reaches the memory tracking (a protected page faults and resolves).
    bool NeedsResolve(std::uint64_t address, std::size_t bytes) const;
    void* Window() const;
    void Resize(std::uint32_t width, std::uint32_t height);
    bool Presentable() const;
    void PresentClear(std::uint32_t width, std::uint32_t height, bool opaque);
    void PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels);
    void PresentDisplayBuffer(const DisplayBuffer& buffer);
    void Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});
    void Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});
    void EnqueueDraw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});

private:
    Graphics::Context graphicsContext() const;
    void present(std::uint32_t width, std::uint32_t height, bool opaque, std::span<const std::byte> pixels, const DisplayBuffer* display = nullptr);
    struct State;
    std::unique_ptr<State> state;
};

}

#endif

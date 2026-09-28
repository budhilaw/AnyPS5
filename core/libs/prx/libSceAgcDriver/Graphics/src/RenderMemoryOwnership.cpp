#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DrawQueue.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <limits>
#include <cstdio>
#include <stdexcept>

namespace AgcDriver::Graphics {

ResidentColor::ResidentColor(const Context& context, const ColorTarget& color) : context(context), color(color), transfer(context) {
    if (color.gpuOnly) return; // never exchanged with guest memory
    if (context.guestBufferCache != nullptr) context.guestBufferCache->ReleaseTracking(color.address, color.bytes);
    memoryWatch = std::make_unique<GuestMemoryTracking::Watch>(color.address, color.bytes, this, [](void* owner, GuestMemoryTracking::Access access) {
        static_cast<ResidentColor*>(owner)->resolveCpuAccess(access);
    });
}

ResidentColor::~ResidentColor() = default;

void ResidentColor::Invalidate() {
    if (color.gpuOnly) { dirty = false; return; } // nothing to hand back: the image keeps its contents
    Require(!dirty, "cannot discard GPU-owned render target contents");
    // The pages keep their protection until the CPU touches them (resolveCpuAccess): a target a
    // compute fill clears every frame would change its protection several times a frame, each
    // an mprotect of megabytes that every core has to see.
    valid = false;
}

void ResidentColor::ReleaseMemory() {
    if (dirty && !color.gpuOnly) {
        char message[160];
        std::snprintf(message, sizeof(message), "cannot release GPU-owned render target 0x%llx (%ux%u) memory while its contents are unsaved", static_cast<unsigned long long>(color.address), color.extent.width, color.extent.height);
        throw std::runtime_error(message);
    }
    Invalidate();
    memoryWatch.reset();  // restores the pages' own protection
}

bool ResidentColor::SharesPages(const ColorTarget& other) const {
    const auto pageSize = GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix();
    Require(color.bytes != 0 && other.bytes != 0, "empty render target page range");
    Require(color.bytes <= std::numeric_limits<std::uint64_t>::max() - color.address && other.bytes <= std::numeric_limits<std::uint64_t>::max() - other.address, "render target page range overflow");
    const auto first = color.address / pageSize;
    const auto last = (color.address + color.bytes - 1) / pageSize;
    const auto otherFirst = other.address / pageSize;
    const auto otherLast = (other.address + other.bytes - 1) / pageSize;
    return first <= otherLast && otherFirst <= last;
}

void ResidentColor::resolveCpuAccess(GuestMemoryTracking::Access access) {
    PerformanceTimer timing("Graphics.RenderMemory.CpuAccess");
    Require(memoryWatch != nullptr && context.drawQueue != nullptr, "render target memory resolver is unavailable");
    if (dirty || access == GuestMemoryTracking::Access::Invalidate) {
        context.drawQueue->WaitGpu();
        timing.Mark("draw_wait");
    }
    if (dirty) {
        CommandBatch batch(context);
        Download(batch.Handle());
        batch.SubmitAndWait();
        timing.Mark("download_wait");
        Commit();
        timing.Mark("guest_writeback");
    }
    if (access != GuestMemoryTracking::Access::Read) {
        Invalidate();
        memoryWatch->Protect(GuestMemoryTracking::Protection::ReadWrite);
    } else {
        // Guest memory holds the contents (saved above, or the target is invalid): reads may go on.
        memoryWatch->Protect(valid ? GuestMemoryTracking::Protection::Read : GuestMemoryTracking::Protection::ReadWrite);
    }
    if (access == GuestMemoryTracking::Access::Invalidate) context.drawQueue->Wait();
}

RenderCache::~RenderCache() {
    for (const auto& [address, entry] : entries) entry->ReleaseMemory();
}

}

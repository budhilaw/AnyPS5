#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include <cstdlib>
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace AgcDriver::Graphics {

GuestBufferMemory::GuestBufferMemory(const Context& context) : context(context) {}

bool GuestBufferMemory::WritesOverlap(std::uint64_t address, std::size_t bytes) const {
    return std::any_of(writes.begin(), writes.end(), [&](const auto& range) { return address < range.second && range.first < address + bytes; });
}

bool GuestBufferMemory::WritesOverlapCopied(std::uint64_t address, std::size_t bytes) const {
    return std::any_of(writes.begin(), writes.end(), [&](const auto& range) {
        if (!(address < range.second && range.first < address + bytes)) return false;
        if (!uploaded) return true;
        const auto found = std::upper_bound(regions.begin(), regions.end(), range.first, [](auto value, const auto& region) { return value < region.begin; });
        if (found == regions.begin()) return true;
        const auto& region = *std::prev(found);
        return !region.inPlace;
    });
}

void GuestBufferMemory::validate(std::uint64_t address, std::size_t bytes) const {
    Require(!uploaded, "guest memory ownership is frozen for GPU execution");
    Require(address != 0 && bytes != 0, "empty guest memory range");
    Require(bytes <= std::numeric_limits<std::uint64_t>::max() - address, "guest memory range overflow");
}

void GuestBufferMemory::AddWritable(std::uint64_t address, std::size_t bytes, bool checked) {
    validate(address, bytes);
    if (!checked) {
        const GuestMemory::MemoryAccessScope deferred(nullptr, nullptr);
        GuestMemory::CheckRange(reinterpret_cast<const void*>(address), bytes, 1, true);
    }
    accesses.push_back({address, bytes, true});
    regions.push_back({address, address + bytes, true, false, {}, nullptr});
    writes.emplace_back(address, address + bytes);
}

void GuestBufferMemory::AddReadOnly(std::uint64_t address, std::size_t bytes, bool checked) {
    validate(address, bytes);
    if (!checked) {
        const GuestMemory::MemoryAccessScope deferred(nullptr, nullptr);
        GuestMemory::CheckRange(reinterpret_cast<const void*>(address), bytes, 1, false);
    }
    accesses.push_back({address, bytes, false});
    regions.push_back({address, address + bytes, false, true, {}, nullptr});
}

void GuestBufferMemory::AddSnapshot(const GuestMemorySnapshot& snapshot) {
    validate(snapshot.address, snapshot.bytes.size());
    for (const auto& region : regions) {
        if (region.begin <= snapshot.address && snapshot.address + snapshot.bytes.size() <= region.end) {
            const auto offset = static_cast<std::size_t>(snapshot.address - region.begin);
            const auto* source = region.writable || region.fromGuest || region.image ? reinterpret_cast<const std::byte*>(snapshot.address) : region.snapshot.data() + offset;
            Require(std::memcmp(source, snapshot.bytes.data(), snapshot.bytes.size()) == 0, "guest snapshot differs from registered memory");
            return;
        }
    }
    regions.push_back({snapshot.address, snapshot.address + snapshot.bytes.size(), false, false, {snapshot.bytes.begin(), snapshot.bytes.end()}, nullptr});
}

void GuestBufferMemory::Upload(bool addressable) {
    Require(!uploaded, "guest memory was already uploaded");
    uploaded = true;
    std::sort(regions.begin(), regions.end(), [](const Region& left, const Region& right) { return left.begin < right.begin; });
    std::vector<Region> merged;
    for (auto& region : regions) {
        if (!merged.empty() && region.begin < merged.back().end) {
            auto& previous = merged.back();
            Require(!previous.image && !region.image, "guest memory overlaps registered image data");
            const bool previousGuest = previous.writable || previous.fromGuest;
            const bool regionGuest = region.writable || region.fromGuest;
            Require(previousGuest == regionGuest, "guest memory overlaps an immutable snapshot");
            if (regionGuest) {
                previous.writable = previous.writable || region.writable;
                previous.fromGuest = !previous.writable;
                previous.end = std::max(previous.end, region.end);
                continue;
            }
            if (!region.writable) {
                const auto offset = static_cast<std::size_t>(region.begin - previous.begin);
                const auto overlap = static_cast<std::size_t>(std::min(previous.end, region.end) - region.begin);
                Require(std::memcmp(previous.snapshot.data() + offset, region.snapshot.data(), overlap) == 0, "inconsistent overlapping guest snapshots");
                if (region.end > previous.end) previous.snapshot.insert(previous.snapshot.end(), region.snapshot.begin() + overlap, region.snapshot.end());
            }
            previous.end = std::max(previous.end, region.end);
        } else {
            merged.push_back(std::move(region));
        }
    }
    regions = std::move(merged);
    for (auto& region : regions) {
        region.padding = region.begin % ViewAlignment;
        const auto bytes = region.end - region.begin + region.padding;
        Require(bytes <= std::numeric_limits<std::size_t>::max(), "guest GPU allocation size overflow");
        const auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | (addressable ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
        if (region.image) {
            region.buffer = context.guestBufferCache->ImageCopy(region.image, region.padding, usage);
            continue;
        }
        const auto resolveAccesses = [&](bool inPlace) {
            for (const auto& access : accesses) {
                if (access.address < region.begin || access.address >= region.end) continue;
                if (!inPlace) GuestMemory::MemoryAccessScope::Resolve(access.address, access.bytes, access.writable);
                else if (context.renderCache != nullptr) context.renderCache->Resolve(access.address, access.bytes, access.writable);
            }
        };
        if ((region.writable || region.fromGuest) && context.guestBufferCache != nullptr) {
            static const bool inPlaceEnabled = std::getenv("ANYPS5_NO_BUFFERS_IN_PLACE") == nullptr;
            if (inPlaceEnabled && (!addressable || context.bufferDeviceAddress)) {
                if (auto view = context.guestBufferCache->HostRange(region.begin - region.padding, region.end - region.begin + region.padding); view.buffer && (view.buffer->Usage() & usage) == usage) {
                    resolveAccesses(true);
                    region.buffer = std::move(view.buffer);
                    region.bufferOffset = view.offset;
                    region.inPlace = true;
                    continue;
                }
            }
            resolveAccesses(false);
            region.mirror = context.guestBufferCache->Acquire(region.begin - region.padding, region.end, usage);
            region.buffer = region.mirror->buffer;
            region.inPlace = region.mirror->imported;
            continue;
        }
        if (region.writable || region.fromGuest) resolveAccesses(false);
        region.buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), usage);
        auto target = region.buffer->Bytes();
        if (region.writable || region.fromGuest) GuestMemory::Read(region.begin - region.padding, target);
        else {
            std::memset(target.data(), 0, static_cast<std::size_t>(region.padding));
            std::memcpy(target.data() + region.padding, region.snapshot.data(), region.snapshot.size());
        }
        region.snapshot.clear();
    }
}

VkDescriptorBufferInfo GuestBufferMemory::Descriptor(std::uint64_t address, std::size_t bytes) const {
    Require(uploaded && !committed, "guest GPU memory is not available");
    Require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest buffer view");
    const auto found = std::upper_bound(regions.begin(), regions.end(), address, [](std::uint64_t value, const Region& region) { return value < region.begin; });
    Require(found != regions.begin(), "guest buffer has no GPU owner");
    const auto& region = *std::prev(found);
    Require(address >= region.begin && address + bytes <= region.end && region.buffer != nullptr, "guest buffer view exceeds its GPU owner");
    const auto residue = address % ViewAlignment;
    const auto offset = region.bufferOffset + (address - residue - (region.begin - region.padding));
    Require(context.limits.minStorageBufferOffsetAlignment != 0 && ViewAlignment % context.limits.minStorageBufferOffsetAlignment == 0 && offset % context.limits.minStorageBufferOffsetAlignment == 0, "guest buffer view violates storage buffer offset alignment");
    Require(bytes + residue <= context.limits.maxStorageBufferRange, "guest buffer view exceeds descriptor range limit");
    return {region.buffer->Handle(), offset, bytes + residue};
}

std::vector<ShaderRecompiler::BdaAbi::Range> GuestBufferMemory::AddressRanges() const {
    Require(uploaded && !committed, "guest GPU address ranges are not available");
    std::vector<ShaderRecompiler::BdaAbi::Range> result;
    for (const auto& region : regions) {
        Require(region.buffer != nullptr, "incomplete guest GPU upload");
        const auto address = region.buffer->DeviceAddress() + region.bufferOffset + region.padding;
        Require(region.end - region.begin <= std::numeric_limits<std::uint64_t>::max() - address, "GPU address range overflow");
        result.push_back({region.begin, region.end, address, ShaderRecompiler::BdaAbi::Read, 0});
    }
    return result;
}

static bool TraceWriteBack() {
    static const bool enabled = std::getenv("ANYPS5_TRACE_WRITEBACK") != nullptr;
    return enabled;
}

static bool NoWriteBack() {
    static const bool enabled = std::getenv("ANYPS5_DEBUG_NO_WRITEBACK") != nullptr;
    return enabled;
}

void GuestBufferMemory::WriteBack(std::uint64_t sequence) {
    Require(uploaded && !committed, "guest memory cannot be committed twice or before upload");
    if (NoWriteBack()) {
        committed = true;
        lease.clear();
        return;
    }
    std::sort(writes.begin(), writes.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    for (const auto& range : writes) {
        if (!merged.empty() && range.first < merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
        else merged.push_back(range);
    }
    std::vector<std::span<const std::byte>> sources;
    std::vector<bool> inPlace;
    for (const auto& [begin, end] : merged) {
        const auto found = std::upper_bound(regions.begin(), regions.end(), begin, [](auto address, const auto& region) { return address < region.begin; });
        Require(found != regions.begin(), "write-back range has no GPU owner");
        const auto& region = *std::prev(found);
        Require(region.buffer != nullptr && region.writable && end <= region.end, "write-back range exceeds its GPU owner");
        sources.push_back(region.buffer->Bytes().subspan(static_cast<std::size_t>(region.bufferOffset + begin - region.begin + region.padding), static_cast<std::size_t>(end - begin)));
        inPlace.push_back(region.inPlace);
        if (inPlace.back() && context.renderCache != nullptr) {
            for (const auto& [first, last] : context.renderCache->UnadoptedRanges(begin, end, sequence)) GuestMemory::CheckRange(reinterpret_cast<const void*>(first), static_cast<std::size_t>(last - first), 1, true);
        } else {
            GuestMemory::CheckRange(reinterpret_cast<const void*>(begin), static_cast<std::size_t>(end - begin), 1, true);
        }
    }
    for (std::size_t i = 0; i < merged.size(); ++i) {
        if (inPlace[i] && !TraceWriteBack()) continue;
        if (TraceWriteBack()) {
            const auto& bytes = sources[i];
            std::string detail;
            if (bytes.size() <= 64) {
                for (std::size_t at = 0; at + 4 <= bytes.size(); at += 4) {
                    std::uint32_t word;
                    std::memcpy(&word, bytes.data() + at, sizeof(word));
                    char item[16];
                    std::snprintf(item, sizeof(item), " %08x", word);
                    detail += item;
                }
            } else {
                std::size_t nonzero = 0;
                for (std::size_t at = 0; at + 4 <= bytes.size(); at += 4) {
                    std::uint32_t word;
                    std::memcpy(&word, bytes.data() + at, sizeof(word));
                    nonzero += word != 0;
                }
                char item[48];
                std::snprintf(item, sizeof(item), " %zu of %zu dwords nonzero", nonzero, bytes.size() / 4);
                detail += item;
            }
            APS5_LOG_OUT("write-back 0x%llx+0x%llx:%s", static_cast<unsigned long long>(merged[i].first), static_cast<unsigned long long>(merged[i].second - merged[i].first), detail.c_str());
        }
        GuestMemory::Write(merged[i].first, sources[i]);
    }
    for (const auto& region : regions) {
        if (region.writable && region.mirror && context.guestBufferCache != nullptr) context.guestBufferCache->MarkSynced(region.mirror);
    }
    committed = true;
    lease.clear();
}

}

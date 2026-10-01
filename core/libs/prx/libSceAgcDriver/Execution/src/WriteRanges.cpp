#include "prx/libSceAgcDriver/Execution/include/WriteRanges.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <algorithm>
#include <cstddef>
#include <span>

namespace AgcDriver {
namespace {

WriteRange clampedWrite(std::uint64_t first, std::uint64_t last, WriteOrigin origin) {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    if (GuestAllocations::GuestAllocationsExtent_nid_postfix(first, &begin, &end)) last = std::min(last, end);
    return {first, last, origin};
}

bool storageImageWrites(const ShaderRecompiler::DescriptorBinding& binding, std::vector<WriteRange>& writes) {
    if (binding.count == 0 || binding.guestDescriptor.size() != static_cast<std::size_t>(binding.count) * 8) return false;
    for (std::size_t element = 0; element < binding.count; ++element) {
        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(element * 8, 8);
        if (words[0] == 0 && (words[1] & 0xffu) == 0) continue;
        std::uint64_t base = 0;
        std::uint64_t bytes = 0;
        if (Graphics::DecodeTextureExtent(words, base, bytes)) {
            writes.push_back(clampedWrite(base, base + bytes, WriteOrigin::StorageImage));
            continue;
        }
        base = ((static_cast<std::uint64_t>(words[0]) | (static_cast<std::uint64_t>(words[1]) << 32u)) & 0xffffffffffull) << 8u;
        std::uint64_t begin = 0;
        std::uint64_t end = 0;
        if (!GuestAllocations::GuestAllocationsExtent_nid_postfix(base, &begin, &end)) return false;
        writes.push_back({base, end, WriteOrigin::AllocationFallback});
    }
    return true;
}

}

void CollectWrites(const std::vector<ShaderRecompiler::DescriptorBinding>& bindings, std::vector<WriteRange>& writes, bool& writesUnknown) {
    for (const auto& binding : bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers) {
            for (std::size_t element = 0; element < binding.elementWritten.size() && element * 4 + 4 <= binding.guestDescriptor.size(); ++element) {
                if (!binding.elementWritten[element]) continue;
                const auto* words = binding.guestDescriptor.data() + element * 4;
                const auto base = (words[0] | (static_cast<std::uint64_t>(words[1]) << 32u)) & 0xffffffffffffull;
                const auto stride = (words[1] >> 16u) & 0x3fffu;
                const auto bytes = stride == 0 ? static_cast<std::uint64_t>(words[2]) : static_cast<std::uint64_t>(stride) * words[2];
                if (base == 0 || bytes == 0 || (element < binding.elementOptional.size() && binding.elementOptional[element] && bytes > (64ull << 20u))) continue;
                writes.push_back(clampedWrite(base, base + bytes, WriteOrigin::BufferElement));
            }
        } else if (!binding.readOnly && binding.kind == ShaderRecompiler::DescriptorKind::StorageImage && !storageImageWrites(binding, writes)) {
            writesUnknown = true;
        }
    }
}

}

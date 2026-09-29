#include <elfpatcher/darwin/MachOTlsRewriter.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <bit>

namespace Elfpatcher::Darwin {

TlsRewriteResult RewriteTlsAccesses(std::vector<std::uint8_t>& image, const std::vector<Domain::ProgramHeader>& headers, const Domain::ProgramHeader* tls, const bool requireTls) {
    const Codegen::X64InstructionDecoder decoder;
    TlsRewriteResult result;
    for (const auto& header : headers) {
        if (header.Type != 1 || (header.Flags & 1) == 0) continue;
        if (header.Offset > image.size() || header.FileSize > image.size() - header.Offset) throw Domain::RelinkerException("Executable segment exceeds the image", header.Offset);
        for (std::uint64_t offset = 0; offset < header.FileSize;) {
            auto* bytes = image.data() + header.Offset + offset;
            const auto info = decoder.DecodeInstruction(bytes, header.FileSize - offset);
            if (info.SegmentPrefix != 0) {
                const auto position = info.OpcodeOffset;
                const bool supported = info.SegmentPrefix == 0x64 && info.RexPrefix == 0x48 && info.Length - position == 7 && bytes[position] == 0x8b && bytes[position + 1] == 0x04 && bytes[position + 2] == 0x25 && bytes[position + 3] == 0 && bytes[position + 4] == 0 && bytes[position + 5] == 0 && bytes[position + 6] == 0;
                if (!supported) throw Domain::RelinkerException("Unsupported macOS guest TLS instruction", header.Offset + offset);
                if (requireTls && (tls == nullptr || tls->MemorySize == 0)) throw Domain::RelinkerException("Guest TLS access without a usable PT_TLS", header.Offset + offset);
                std::size_t prefix = 0;
                while (bytes[prefix] != 0x64) ++prefix;
                bytes[prefix] = 0x65;
                const std::uint32_t displacement = GuestTcbSlot * 8u;
                bytes[position + 3] = static_cast<std::uint8_t>(displacement & 0xff);
                bytes[position + 4] = static_cast<std::uint8_t>((displacement >> 8) & 0xff);
                bytes[position + 5] = static_cast<std::uint8_t>((displacement >> 16) & 0xff);
                bytes[position + 6] = static_cast<std::uint8_t>((displacement >> 24) & 0xff);
                ++result.RewrittenAccesses;
            }
            offset += info.Length;
        }
    }
    if (tls != nullptr && tls->MemorySize != 0 && (tls->FileSize > tls->MemorySize || !std::has_single_bit(tls->Alignment) || tls->Alignment > 8192))
        throw Domain::RelinkerException("Invalid or unsupported ELF TLS layout", tls->Offset);
    return result;
}

}

#include <elfpatcher/darwin/MachOPatcher.hpp>
#include <elfpatcher/darwin/MachOTlsRewriter.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace Elfpatcher::Darwin {
namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint64_t Page = 0x1000;
constexpr std::uint64_t PageZeroSize = 0x100000000;       // 4 GiB __PAGEZERO, as Apple's linker emits
constexpr std::uint64_t HeaderSize = 0x4000;              // __TEXT: Mach-O header, load commands, entry stub
constexpr std::uint16_t ElfTypeSceModule = 0xFE18;

constexpr std::uint32_t MhMagic64 = 0xfeedfacf;
constexpr std::uint32_t CpuTypeX86_64 = 0x01000007;
constexpr std::uint32_t CpuSubtypeX86_64All = 0x80000003;
constexpr std::uint32_t MhExecute = 2, MhDylib = 6;
// The executable is not position independent: dyld would slide it by a 4 KiB multiple, while the
// guest assumes 16 KiB pages and rounds addresses accordingly, which must stay inside the image.
constexpr std::uint32_t MhFlagsExecute = 0x1 | 0x4 | 0x80; // NOUNDEFS | DYLDLINK | TWOLEVEL
constexpr std::uint32_t MhFlagsDylib = 0x1 | 0x4 | 0x80;

constexpr std::uint32_t LcSegment64 = 0x19;
constexpr std::uint32_t LcSymtab = 0x2;
constexpr std::uint32_t LcDysymtab = 0xb;
constexpr std::uint32_t LcLoadDylinker = 0xe;
constexpr std::uint32_t LcLoadDylib = 0xc;
constexpr std::uint32_t LcRpath = 0x8000001c;
constexpr std::uint32_t LcDyldInfoOnly = 0x80000022;
constexpr std::uint32_t LcMain = 0x80000028;
constexpr std::uint32_t LcBuildVersion = 0x32;
constexpr std::uint32_t LcIdDylib = 0xd;
constexpr std::uint32_t SectionModInitFuncPointers = 0x9;

constexpr std::uint32_t VmProtRead = 1, VmProtWrite = 2, VmProtExecute = 4;
constexpr std::uint32_t SectionRegular = 0;

constexpr std::uint32_t RelocationJumpSlot = 7, RelocationGlobData = 6, RelocationAbsolute64 = 1, RelocationRelative = 8, RelocationTlsModule = 16, RelocationTlsOffset = 17;
constexpr std::uint32_t PtLoad = 1, PtDynamic = 2, PtTls = 7, PtGnuEhFrame = 0x6474e550;
constexpr std::int64_t DtStrtab = 5, DtSymtab = 6, DtInit = 12, DtInitArray = 25, DtInitArraySz = 27, DtSceSymtabSz = 0x6100003f;

std::uint64_t alignUp(std::uint64_t value, std::uint64_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }

void appendU8(Bytes& out, std::uint8_t value) { out.push_back(value); }
void appendU16(Bytes& out, std::uint16_t value) { Io::AppendU16(out, value); }
void appendU32(Bytes& out, std::uint32_t value) { Io::AppendU32(out, value); }
void appendU64(Bytes& out, std::uint64_t value) { Io::AppendU64(out, value); }
void appendUleb(Bytes& out, std::uint64_t value) {
    do { std::uint8_t byte = value & 0x7f; value >>= 7; if (value) byte |= 0x80; out.push_back(byte); } while (value);
}
void appendSleb(Bytes& out, std::int64_t value) {
    bool more = true;
    while (more) {
        std::uint8_t byte = value & 0x7f;
        value >>= 7;
        more = !((value == 0 && (byte & 0x40) == 0) || (value == -1 && (byte & 0x40) != 0));
        if (more) byte |= 0x80;
        out.push_back(byte);
    }
}
void appendName16(Bytes& out, const char* name) {
    char buffer[16]{};
    std::strncpy(buffer, name, 16);
    out.insert(out.end(), buffer, buffer + 16);
}
void appendPadded(Bytes& out, const std::string& text, std::size_t alignment) {
    out.insert(out.end(), text.begin(), text.end());
    out.push_back(0);
    while (out.size() % alignment) out.push_back(0);
}

struct Section { std::string Name; std::uint64_t Vmaddr; std::uint64_t Fileoff; Bytes Data; };
struct Segment {
    std::string Name;
    std::uint64_t Vmaddr = 0, Vmsize = 0, Fileoff = 0, Filesize = 0;
    std::uint32_t Prot = 0;
    std::vector<Section> Sections;
    // Guest segments only: the ELF program headers they carry (loads sharing a page are merged).
    std::vector<Domain::ProgramHeader> Members;
};

struct Relocation { std::uint64_t Address; std::uint32_t Type; std::int64_t Addend; std::string Symbol; };

std::uint32_t protection(std::uint32_t elfFlags) {
    return ((elfFlags & 4) ? VmProtRead : 0) | ((elfFlags & 2) ? VmProtWrite : 0) | ((elfFlags & 1) ? VmProtExecute : 0);
}

std::string translateRunPath(const std::string& runPath) {
    std::string result = runPath;
    const std::string origin = "$ORIGIN";
    for (auto position = result.find(origin); position != std::string::npos; position = result.find(origin, position + 1))
        result.replace(position, origin.size(), "@executable_path");
    if (result.empty() || result.find("$") != std::string::npos) throw Domain::RelinkerException("Unsupported macOS run path: " + runPath);
    return result;
}

std::vector<std::string> neededLibraries(const Domain::SysVDynamicSection& dynamicSection) {
    const auto& bytes = dynamicSection.DynamicSegmentData;
    if (bytes.size() % 16 != 0) throw Domain::RelinkerException("Invalid dynamic segment size");
    std::vector<std::string> result;
    std::set<std::string> unique;
    for (std::size_t offset = 0; offset < bytes.size(); offset += 16) {
        if (Io::ReadU64(bytes, offset) != 1) throw Domain::RelinkerException("Unexpected tag in rebuilt ELF dependency table", offset);
        const auto nameOffset = Io::ReadU64(bytes, offset + 8);
        if (nameOffset >= dynamicSection.DynStrData.size()) throw Domain::RelinkerException("DT_NEEDED string offset is out of bounds", nameOffset);
        std::string name(reinterpret_cast<const char*>(dynamicSection.DynStrData.data() + nameOffset));
        if (name.empty() || name.find_first_of("/\\:@") != std::string::npos || !unique.insert(name).second) throw Domain::RelinkerException("Invalid or duplicate DT_NEEDED library: " + name);
        result.push_back(std::move(name));
    }
    return result;
}

std::vector<Relocation> readRelocations(const Domain::SysVDynamicSection& dynamicSection) {
    std::vector<Relocation> result;
    const auto symbolName = [&](std::uint32_t index) {
        const std::size_t entry = static_cast<std::size_t>(index) * 24;
        if (entry + 24 > dynamicSection.DynSymData.size()) throw Domain::RelinkerException("Relocation symbol index is out of bounds", index);
        const auto nameOffset = Io::ReadU32(dynamicSection.DynSymData, entry);
        if (nameOffset >= dynamicSection.DynStrData.size()) throw Domain::RelinkerException("Symbol name offset is out of bounds", nameOffset);
        return std::string(reinterpret_cast<const char*>(dynamicSection.DynStrData.data() + nameOffset));
    };
    for (const auto* table : {&dynamicSection.RelaPltData, &dynamicSection.RelaData}) {
        if (table->size() % 24 != 0) throw Domain::RelinkerException("Invalid relocation table size");
        for (std::size_t offset = 0; offset < table->size(); offset += 24) {
            const auto info = Io::ReadU64(*table, offset + 8);
            Relocation relocation{Io::ReadU64(*table, offset), static_cast<std::uint32_t>(info & 0xffffffff), static_cast<std::int64_t>(Io::ReadU64(*table, offset + 16)), {}};
            const auto symbol = static_cast<std::uint32_t>(info >> 32);
            if (relocation.Type == RelocationRelative || relocation.Type == RelocationTlsModule || relocation.Type == RelocationTlsOffset) {
                if (symbol != 0) throw Domain::RelinkerException("Module-relative relocation with a symbol", offset);
            } else if (relocation.Type == RelocationJumpSlot || relocation.Type == RelocationGlobData || relocation.Type == RelocationAbsolute64) {
                if (symbol == 0) throw Domain::RelinkerException("Symbol relocation without a symbol", offset);
                relocation.Symbol = symbolName(symbol);
                if (relocation.Symbol.empty()) throw Domain::RelinkerException("Symbol relocation with an empty name", offset);
            } else {
                throw Domain::RelinkerException("Unsupported relocation type for macOS: " + std::to_string(relocation.Type), offset);
            }
            result.push_back(std::move(relocation));
        }
    }
    return result;
}

struct GuestExport { std::string Name; std::uint64_t Vaddr; };

std::uint64_t moduleIdentifier(const std::string& name) {
    std::uint64_t hash = 1469598103934665603ull; // FNV-1a; libkernel matches this against __modtls
    for (const unsigned char c : name) { hash ^= c; hash *= 1099511628211ull; }
    return (hash & 0x7fffffffu) | 1u;
}

// Reads DT_* values of the ELF's PT_DYNAMIC segment.
std::map<std::int64_t, std::vector<std::uint64_t>> dynamicTags(const Bytes& elf, const std::vector<Domain::ProgramHeader>& headers) {
    std::map<std::int64_t, std::vector<std::uint64_t>> tags;
    for (const auto& header : headers) {
        if (header.Type != PtDynamic) continue;
        if (header.Offset > elf.size() || header.FileSize > elf.size() - header.Offset || header.FileSize % 16 != 0) throw Domain::RelinkerException("Invalid PT_DYNAMIC", header.Offset);
        for (std::uint64_t offset = 0; offset < header.FileSize; offset += 16) {
            const auto tag = static_cast<std::int64_t>(Io::ReadU64(elf, static_cast<std::size_t>(header.Offset + offset)));
            if (tag == 0) break;
            tags[tag].push_back(Io::ReadU64(elf, static_cast<std::size_t>(header.Offset + offset + 8)));
        }
    }
    return tags;
}

std::uint64_t fileOffsetOf(std::uint64_t vaddr, std::uint64_t size, const std::vector<Domain::ProgramHeader>& loads) {
    for (const auto& load : loads)
        if (vaddr >= load.MappedAddress && size <= load.FileSize && vaddr - load.MappedAddress <= load.FileSize - size) return load.Offset + (vaddr - load.MappedAddress);
    throw Domain::RelinkerException("Address is not backed by a loadable segment", vaddr);
}

// Defined dynamic symbols of a module: NID (the "#lib#module" suffix stripped) and address.
std::vector<GuestExport> moduleExports(const Bytes& elf, const std::map<std::int64_t, std::vector<std::uint64_t>>& tags, const std::vector<Domain::ProgramHeader>& loads) {
    const auto single = [&](std::int64_t tag) -> std::uint64_t {
        const auto found = tags.find(tag);
        if (found == tags.end() || found->second.size() != 1) throw Domain::RelinkerException("Module lacks dynamic tag " + std::to_string(tag));
        return found->second.front();
    };
    const auto symbolCount = single(DtSceSymtabSz) / 24;
    const auto symbols = fileOffsetOf(single(DtSymtab), symbolCount * 24, loads);
    const auto strings = single(DtStrtab);
    std::vector<GuestExport> exports;
    std::set<std::string> unique;
    for (std::uint64_t index = 1; index < symbolCount; ++index) {
        const auto entry = static_cast<std::size_t>(symbols + index * 24);
        const auto value = Io::ReadU64(elf, entry + 8);
        if (value == 0) continue;
        const auto nameOffset = Io::ReadU32(elf, entry);
        const auto nameFile = fileOffsetOf(strings + nameOffset, 1, loads);
        std::string name(reinterpret_cast<const char*>(elf.data() + nameFile));
        name = name.substr(0, name.find('#'));
        if (name.empty()) throw Domain::RelinkerException("Module export with an empty name", entry);
        if (!unique.insert(name).second) continue; // the same NID exported under several library suffixes
        exports.push_back({name, value});
    }
    return exports;
}

// dyld export trie (LC_DYLD_INFO export_off): uleb terminal size [flags, offset], child count, edges.
class ExportTrie {
public:
    explicit ExportTrie(const std::vector<std::pair<std::string, std::uint64_t>>& entries) {
        for (const auto& [name, offset] : entries) insert(name, offset);
    }
    Bytes Serialize() const {
        std::vector<const Node*> order;
        collect(&_root, order);
        std::vector<std::uint64_t> offsets(order.size(), 0);
        std::map<const Node*, std::size_t> indices;
        for (std::size_t i = 0; i < order.size(); ++i) indices[order[i]] = i;
        // Node sizes depend on child offsets (uleb); iterate until stable.
        for (int pass = 0; pass < 16; ++pass) {
            std::uint64_t cursor = 0;
            bool changed = false;
            for (std::size_t i = 0; i < order.size(); ++i) {
                if (offsets[i] != cursor) { offsets[i] = cursor; changed = true; }
                cursor += nodeSize(*order[i], offsets, indices);
            }
            if (!changed) break;
        }
        Bytes out;
        for (std::size_t i = 0; i < order.size(); ++i) emit(*order[i], offsets, indices, out);
        return out;
    }
private:
    struct Node { bool terminal = false; std::uint64_t offset = 0; std::map<std::string, Node> children; };
    Node _root;
    void insert(const std::string& name, std::uint64_t offset) {
        Node* node = &_root;
        std::string rest = name;
        while (!rest.empty()) {
            bool advanced = false;
            for (auto& [edgeKey, child] : node->children) {
                const std::string edge = edgeKey; // copied: the map entry may be erased below
                std::size_t common = 0;
                while (common < edge.size() && common < rest.size() && edge[common] == rest[common]) ++common;
                if (common == 0) continue;
                if (common < edge.size()) { // split the edge
                    Node split;
                    split.children.emplace(edge.substr(common), std::move(child));
                    node->children.erase(edge);
                    auto [it, inserted] = node->children.emplace(edge.substr(0, common), std::move(split));
                    (void)inserted;
                    node = &it->second;
                } else {
                    node = &child;
                }
                rest = rest.substr(common);
                advanced = true;
                break;
            }
            if (!advanced) {
                auto [it, inserted] = node->children.emplace(rest, Node{});
                (void)inserted;
                node = &it->second;
                rest.clear();
            }
        }
        node->terminal = true;
        node->offset = offset;
    }
    static void collect(const Node* node, std::vector<const Node*>& order) {
        order.push_back(node);
        for (const auto& [edge, child] : node->children) collect(&child, order);
    }
    static std::size_t ulebSize(std::uint64_t value) { std::size_t size = 0; do { ++size; value >>= 7; } while (value); return size; }
    static std::size_t nodeSize(const Node& node, const std::vector<std::uint64_t>& offsets, const std::map<const Node*, std::size_t>& indices) {
        std::size_t terminal = node.terminal ? ulebSize(0) + ulebSize(node.offset) : 0;
        std::size_t size = ulebSize(terminal) + terminal + 1;
        for (const auto& [edge, child] : node.children) size += edge.size() + 1 + ulebSize(offsets[indices.at(&child)]);
        return size;
    }
    static void emit(const Node& node, const std::vector<std::uint64_t>& offsets, const std::map<const Node*, std::size_t>& indices, Bytes& out) {
        if (node.terminal) { Bytes terminal; appendUleb(terminal, 0); appendUleb(terminal, node.offset); appendUleb(out, terminal.size()); out.insert(out.end(), terminal.begin(), terminal.end()); }
        else appendUleb(out, 0);
        appendU8(out, static_cast<std::uint8_t>(node.children.size()));
        for (const auto& [edge, child] : node.children) { out.insert(out.end(), edge.begin(), edge.end()); appendU8(out, 0); appendUleb(out, offsets[indices.at(&child)]); }
    }
};

class Layout {
public:
    std::vector<Segment> Segments;

    // Segment index and offset for a Mach-O virtual address.
    std::pair<std::size_t, std::uint64_t> Locate(std::uint64_t vmaddr) const {
        for (std::size_t index = 0; index < Segments.size(); ++index) {
            const auto& segment = Segments[index];
            if (segment.Vmsize != 0 && vmaddr >= segment.Vmaddr && vmaddr - segment.Vmaddr < segment.Vmsize) return {index, vmaddr - segment.Vmaddr};
        }
        throw Domain::RelinkerException("Address is not mapped by any Mach-O segment", vmaddr);
    }
};

}

std::vector<std::uint8_t> MachOPatcher::Patch(
    const std::vector<std::uint8_t>& sourceElf,
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    const Domain::SysVDynamicSection& dynamicSection,
    const std::uint64_t originalPltGotVaddr,
    const std::string& runPath,
    const bool lazyBinding,
    const bool dependencyDiagnostics)
{
    (void)originalPltGotVaddr;
    (void)lazyBinding; // dyld binds every import at load; the guest never observes lazy resolution
    if (dependencyDiagnostics) throw Domain::RelinkerException("macOS target does not support --windows-diagnostics");
    if (sourceElf.size() < 64) throw Domain::RelinkerException("ELF too small for a Mach-O conversion");
    const auto entry = Io::ReadU64(sourceElf, kEhdrEntryOffset);
    const bool module = Io::ReadU16(sourceElf, 16) == ElfTypeSceModule;
    const std::uint64_t HeaderVmaddr = module ? 0 : PageZeroSize;
    const std::uint64_t GuestBase = HeaderVmaddr + HeaderSize; // guest virtual address 0 maps here
    const auto tags = dynamicTags(sourceElf, originalHeaders);
    const auto moduleId = moduleIdentifier(_outputName);

    // Guest segments in address order.
    std::vector<Domain::ProgramHeader> loads;
    const Domain::ProgramHeader* tls = nullptr;
    const Domain::ProgramHeader* processParameters = nullptr;
    const Domain::ProgramHeader* exceptionHeader = nullptr;
    for (const auto& header : originalHeaders) {
        if (header.Type == PtLoad) {
            if (header.MemorySize == 0) continue;
            if (header.FileSize > header.MemorySize || header.Offset > sourceElf.size() || header.FileSize > sourceElf.size() - header.Offset) throw Domain::RelinkerException("Invalid PT_LOAD", header.Offset);
            if ((header.MappedAddress & (Page - 1)) != (header.Offset & (Page - 1))) throw Domain::RelinkerException("PT_LOAD address and offset disagree modulo the page size", header.Offset);
            loads.push_back(header);
        } else if (header.Type == PtTls) {
            if (tls != nullptr) throw Domain::RelinkerException("Multiple ELF TLS segments");
            tls = &header;
        } else if (header.Type == PT_OS_PROCPARAM) {
            if (processParameters != nullptr || header.FileSize < 0x40) throw Domain::RelinkerException("Invalid process parameter segment");
            processParameters = &header;
        } else if (header.Type == PtGnuEhFrame) {
            if (exceptionHeader != nullptr) throw Domain::RelinkerException("Multiple GNU_EH_FRAME segments");
            exceptionHeader = &header;
        }
    }
    if (loads.empty()) throw Domain::RelinkerException("ELF has no loadable segment");
    std::sort(loads.begin(), loads.end(), [](const auto& a, const auto& b) { return a.MappedAddress < b.MappedAddress; });
    bool entryMapped = false;
    for (const auto& load : loads) entryMapped = entryMapped || ((load.Flags & 1) && entry >= load.MappedAddress && entry - load.MappedAddress < load.MemorySize);
    if (!module && !entryMapped) throw Domain::RelinkerException("ELF entry point is not in an executable segment", entry);

    // Guest code: rewrite thread-local storage access before the bytes are copied.
    Bytes image = sourceElf;
    const auto tlsRewrite = RewriteTlsAccesses(image, originalHeaders, tls, !module);
    const bool publishTls = tls != nullptr && tls->MemorySize != 0;
    const auto exports = module ? moduleExports(sourceElf, tags, loads) : std::vector<GuestExport>{};

    const auto libraries = neededLibraries(dynamicSection);
    const auto relocations = readRelocations(dynamicSection);
    const auto rpath = translateRunPath(runPath);

    // Layout: __PAGEZERO, __TEXT (header + stub), guest segments, __ANYPS5, __LINKEDIT.
    Layout layout;
    if (!module) layout.Segments.push_back({"__PAGEZERO", 0, PageZeroSize, 0, 0, 0, {}, {}});
    layout.Segments.push_back({"__TEXT", HeaderVmaddr, HeaderSize, 0, HeaderSize, VmProtRead | VmProtExecute, {}, {}});
    // Loads whose page ranges intersect (a zero-filled tail followed by the next segment on the
    // same page, as ELF linkers emit) become one Mach-O segment with the union of their permissions;
    // later members overwrite earlier ones on the shared page, as the ELF loader would map them.
    std::vector<Segment> guests;
    for (const auto& load : loads) {
        const auto first = load.MappedAddress & ~(Page - 1);
        const auto last = alignUp(load.MappedAddress + load.MemorySize, Page);
        if (!guests.empty() && GuestBase + first < guests.back().Vmaddr + guests.back().Vmsize) {
            auto& segment = guests.back();
            segment.Vmsize = std::max(segment.Vmsize, GuestBase + last - segment.Vmaddr);
            segment.Prot |= protection(load.Flags);
            segment.Members.push_back(load);
            continue;
        }
        Segment segment;
        segment.Name = "__GUEST" + std::to_string(guests.size());
        segment.Vmaddr = GuestBase + first;
        segment.Vmsize = last - first;
        segment.Prot = protection(load.Flags) | VmProtRead; // dyld and this runtime read every segment
        segment.Members.push_back(load);
        guests.push_back(std::move(segment));
    }
    std::uint64_t fileCursor = HeaderSize;
    for (auto& segment : guests) {
        // A __text section over executable guest bytes lets otool/objdump/lldb disassemble them; dyld ignores it.
        segment.Sections.clear();
        // macOS enforces W^X for translated code and dyld cannot apply fixups to read-execute pages.
        if ((segment.Prot & VmProtWrite) != 0 && (segment.Prot & VmProtExecute) != 0)
            throw Domain::RelinkerException("Writable and executable segments are not supported on macOS", segment.Members.front().Offset);
        std::uint64_t fileEnd = 0;
        for (const auto& member : segment.Members) fileEnd = std::max(fileEnd, (GuestBase + member.MappedAddress + member.FileSize) - segment.Vmaddr);
        segment.Fileoff = fileCursor;
        segment.Filesize = alignUp(fileEnd, Page);
        if ((segment.Prot & VmProtExecute) != 0) segment.Sections.push_back({"__text", segment.Vmaddr, segment.Fileoff, Bytes(static_cast<std::size_t>(fileEnd), 0)});
        fileCursor += segment.Filesize;
        layout.Segments.push_back(segment);
    }
    const std::uint64_t guestEnd = layout.Segments.back().Vmaddr + layout.Segments.back().Vmsize;

    // Guest metadata for libc.prx and libkernel.prx.
    // Read-write: dyld binds the entry stub's exit slot here.
    Segment metadata{"__ANYPS5", guestEnd + Page, 0, fileCursor, 0, VmProtRead | VmProtWrite, {}, {}};
    const auto addSection = [&](const char* name, Bytes data) {
        while (metadata.Filesize % 16) { ++metadata.Filesize; }
        metadata.Sections.push_back({name, metadata.Vmaddr + metadata.Filesize, metadata.Fileoff + metadata.Filesize, std::move(data)});
        metadata.Filesize += metadata.Sections.back().Data.size();
    };
    if (processParameters != nullptr) {
        Bytes data; appendU64(data, GuestBase + processParameters->MappedAddress); appendU64(data, processParameters->FileSize);
        addSection("__procparam", std::move(data));
    }
    if (exceptionHeader != nullptr) {
        Bytes data; appendU64(data, GuestBase + exceptionHeader->MappedAddress);
        addSection("__ehframehdr", std::move(data));
    }
    if (publishTls) {
        Bytes data;
        appendU64(data, tls->FileSize != 0 ? GuestBase + tls->MappedAddress : 0);
        appendU64(data, tls->FileSize); appendU64(data, tls->MemorySize); appendU64(data, std::max<std::uint64_t>(tls->Alignment, 1)); appendU64(data, module ? moduleId : GuestTcbSlot);
        addSection(module ? "__modtls" : "__tls", std::move(data));
    }
    // Module initializers (DT_INIT, then DT_INIT_ARRAY) run as dyld initializers; the PS5 loader
    // runs them when the module is loaded. The executable's are left to its own startup code.
    // DT_INIT of a PS5 module is the SDK's `_init(args, argp, start)`: it runs the constructors and
    // tail-calls `start` when nonzero. dyld passes (argc, argv, envp, ...) to initializers, so a
    // trampoline in the header page clears the three registers before jumping to it.
    // DT_INIT of a PS5 module is the SDK's `_init(args, argp, start)`, which the console's loader
    // calls with the arguments of sceKernelLoadStartModule; it runs the constructors and then
    // module_start. dyld cannot pass those arguments, so the address is published in an `__init`
    // metadata slot and libkernel calls it after dlopen. DT_INIT_ARRAY entries stay dyld initializers.
    std::vector<std::uint64_t> initializers;
    std::uint64_t moduleInit = 0;
    if (module) {
        if (const auto init = tags.find(DtInit); init != tags.end() && init->second.size() == 1 && init->second.front() != 0) {
            moduleInit = init->second.front();
        }
        if (const auto array = tags.find(DtInitArray); array != tags.end() && array->second.size() == 1 && array->second.front() != 0) {
            const auto size = tags.count(DtInitArraySz) ? tags.at(DtInitArraySz).front() : 0;
            std::map<std::uint64_t, std::int64_t> relative;
            for (const auto& relocation : relocations) if (relocation.Type == RelocationRelative) relative[relocation.Address] = relocation.Addend;
            for (std::uint64_t offset = 0; offset < size; offset += 8) {
                const auto slot = array->second.front() + offset;
                const auto found = relative.find(slot);
                const auto value = found != relative.end() ? static_cast<std::uint64_t>(found->second) : Io::ReadU64(sourceElf, static_cast<std::size_t>(fileOffsetOf(slot, 8, loads)));
                if (value != 0 && value != ~std::uint64_t(0)) initializers.push_back(value);
            }
        }
    }
    std::uint64_t moduleInitVmaddr = 0;
    if (moduleInit != 0) {
        moduleInitVmaddr = metadata.Vmaddr + alignUp(metadata.Filesize, 16);
        Bytes data; appendU64(data, GuestBase + moduleInit);
        addSection("__init", std::move(data));
    }
    std::uint64_t initializerVmaddr = 0;
    if (!initializers.empty()) {
        initializerVmaddr = metadata.Vmaddr + alignUp(metadata.Filesize, 16);
        Bytes data;
        for (const auto initializer : initializers) appendU64(data, GuestBase + initializer);
        addSection("__mod_init_func", std::move(data));
    }
    // Slot bound to libSystem's exit for the entry stub, and, when the guest links libc.prx, a slot
    // bound to libc's guest-main runner: on macOS the guest's main runs on a secondary thread so
    // the process main thread stays free for Cocoa (window creation and event delivery).
    const bool hopMain = !module && std::find(libraries.begin(), libraries.end(), "libc.prx") != libraries.end();
    const std::uint64_t exitSlotVmaddr = metadata.Vmaddr + alignUp(metadata.Filesize, 16);
    if (!module) { Bytes data(8, 0); addSection("__exit", std::move(data)); }
    const std::uint64_t runMainSlotVmaddr = metadata.Vmaddr + alignUp(metadata.Filesize, 16);
    if (hopMain) { Bytes data(8, 0); addSection("__runmain", std::move(data)); }
    metadata.Filesize = alignUp(std::max<std::uint64_t>(metadata.Filesize, 16), Page);
    metadata.Vmsize = metadata.Filesize;
    fileCursor += metadata.Filesize;
    layout.Segments.push_back(std::move(metadata));

    // Entry stub inside __TEXT: main(argc, argv, ...) -> guest _start(&{argc, argv}, nullptr), then exit(eax).
    Bytes stub;
    const std::uint64_t stubVmaddr = HeaderVmaddr + 0x2000;
    const std::uint64_t guestEntry = GuestBase + entry;
    if (!module) {
        const std::uint8_t prologue[] = {0x55, 0x48, 0x89, 0xe5, 0x48, 0x83, 0xec, 0x30, 0x48, 0x83, 0xe4, 0xf0, 0x89, 0x3c, 0x24, 0x48, 0x89, 0x74, 0x24, 0x08, 0x48, 0x89, 0xe7, 0x31, 0xf6};
        stub.insert(stub.end(), prologue, prologue + sizeof(prologue));
        if (hopMain) {
            // lea guestEntry(%rip), %rsi; call *runmain(%rip): LibcRunGuestMain(args, entry)
            const auto leaNext = stubVmaddr + stub.size() + 7;
            const auto leaRel = static_cast<std::int64_t>(guestEntry) - static_cast<std::int64_t>(leaNext);
            if (leaRel < INT32_MIN || leaRel > INT32_MAX) throw Domain::RelinkerException("Guest entry point is out of call range", entry);
            stub.push_back(0x48); stub.push_back(0x8d); stub.push_back(0x35); appendU32(stub, static_cast<std::uint32_t>(static_cast<std::int32_t>(leaRel)));
            const auto runNext = stubVmaddr + stub.size() + 6;
            const auto runRel = static_cast<std::int64_t>(runMainSlotVmaddr) - static_cast<std::int64_t>(runNext);
            if (runRel < INT32_MIN || runRel > INT32_MAX) throw Domain::RelinkerException("Run-main slot is out of range");
            stub.push_back(0xff); stub.push_back(0x15); appendU32(stub, static_cast<std::uint32_t>(static_cast<std::int32_t>(runRel)));
        } else {
            const auto callNext = stubVmaddr + stub.size() + 5;
            const auto rel = static_cast<std::int64_t>(guestEntry) - static_cast<std::int64_t>(callNext);
            if (rel < INT32_MIN || rel > INT32_MAX) throw Domain::RelinkerException("Guest entry point is out of call range", entry);
            stub.push_back(0xe8); appendU32(stub, static_cast<std::uint32_t>(static_cast<std::int32_t>(rel)));
        }
        stub.push_back(0x89); stub.push_back(0xc7); // mov %eax, %edi
        const auto exitCallNext = stubVmaddr + stub.size() + 6;
        const auto exitRel = static_cast<std::int64_t>(exitSlotVmaddr) - static_cast<std::int64_t>(exitCallNext);
        if (exitRel < INT32_MIN || exitRel > INT32_MAX) throw Domain::RelinkerException("Exit slot is out of range");
        stub.push_back(0xff); stub.push_back(0x15); appendU32(stub, static_cast<std::uint32_t>(static_cast<std::int32_t>(exitRel))); // call *exit(%rip)
        stub.push_back(0x0f); stub.push_back(0x0b); // ud2
    }

    // Rebase and bind streams (dyld classic opcodes), sorted by address.
    Bytes rebase, bind;
    {
        std::vector<const Relocation*> rebases, binds;
        std::vector<Relocation> initializerRebases;
        for (std::size_t index = 0; index < initializers.size(); ++index) initializerRebases.push_back({initializerVmaddr + index * 8 - GuestBase, RelocationRelative, 0, {}});
        if (moduleInit != 0) initializerRebases.push_back({moduleInitVmaddr - GuestBase, RelocationRelative, 0, {}});
        for (const auto& relocation : relocations) {
            if (relocation.Type == RelocationTlsModule || relocation.Type == RelocationTlsOffset) continue; // resolved statically below
            (relocation.Type == RelocationRelative ? rebases : binds).push_back(&relocation);
        }
        for (const auto& relocation : initializerRebases) rebases.push_back(&relocation);
        const auto byAddress = [](const Relocation* a, const Relocation* b) { return a->Address < b->Address; };
        std::sort(rebases.begin(), rebases.end(), byAddress);
        std::sort(binds.begin(), binds.end(), byAddress);
        appendU8(rebase, 0x11); // REBASE_OPCODE_SET_TYPE_IMM | REBASE_TYPE_POINTER
        std::size_t currentSegment = SIZE_MAX;
        std::uint64_t nextOffset = 0;
        for (const auto* relocation : rebases) {
            const auto [segment, offset] = layout.Locate(GuestBase + relocation->Address);
            if (segment != currentSegment || offset != nextOffset) {
                appendU8(rebase, static_cast<std::uint8_t>(0x20 | segment)); appendUleb(rebase, offset); // SET_SEGMENT_AND_OFFSET_ULEB
                currentSegment = segment;
            }
            appendU8(rebase, 0x51); // DO_REBASE_IMM_TIMES 1
            nextOffset = offset + 8;
        }
        appendU8(rebase, 0);
        appendU8(bind, 0x3e); // BIND_OPCODE_SET_DYLIB_SPECIAL_IMM(BIND_SPECIAL_DYLIB_FLAT_LOOKUP)
        appendU8(bind, 0x51); // BIND_OPCODE_SET_TYPE_IMM(BIND_TYPE_POINTER)
        std::string currentSymbol;
        std::int64_t currentAddend = 0;
        bool first = true;
        for (const auto* relocation : binds) {
            const auto [segment, offset] = layout.Locate(GuestBase + relocation->Address);
            if (first || relocation->Symbol != currentSymbol) {
                appendU8(bind, 0x40); bind.push_back('_'); bind.insert(bind.end(), relocation->Symbol.begin(), relocation->Symbol.end()); appendU8(bind, 0); // SET_SYMBOL_TRAMPOLINE_FLAGS_IMM
                currentSymbol = relocation->Symbol;
            }
            const auto addend = relocation->Type == RelocationJumpSlot ? 0 : relocation->Addend;
            if (first || addend != currentAddend) { appendU8(bind, 0x60); appendSleb(bind, addend); currentAddend = addend; } // SET_ADDEND_SLEB
            appendU8(bind, static_cast<std::uint8_t>(0x70 | segment)); appendUleb(bind, offset); // SET_SEGMENT_AND_OFFSET_ULEB
            appendU8(bind, 0x90); // DO_BIND
            first = false;
        }
        // The stub's exit slot.
        if (!module) {
            const auto [segment, offset] = layout.Locate(exitSlotVmaddr);
            appendU8(bind, 0x40); const char name[] = "_exit"; bind.insert(bind.end(), name, name + sizeof(name));
            appendU8(bind, 0x60); appendSleb(bind, 0);
            appendU8(bind, static_cast<std::uint8_t>(0x70 | segment)); appendUleb(bind, offset);
            appendU8(bind, 0x90);
        }
        if (hopMain) {
            const auto [segment, offset] = layout.Locate(runMainSlotVmaddr);
            appendU8(bind, 0x40); const char name[] = "_LibcRunGuestMain_nid_no_patch"; bind.insert(bind.end(), name, name + sizeof(name));
            appendU8(bind, 0x60); appendSleb(bind, 0);
            appendU8(bind, static_cast<std::uint8_t>(0x70 | segment)); appendUleb(bind, offset);
            appendU8(bind, 0x90);
        }
        appendU8(bind, 0);
    }

    // __LINKEDIT: rebase, bind, symbol table (empty) and string table.
    Segment linkedit{"__LINKEDIT", 0, 0, fileCursor, 0, VmProtRead, {}, {}};
    const auto lastGuest = layout.Segments.back();
    linkedit.Vmaddr = lastGuest.Vmaddr + lastGuest.Vmsize;
    Bytes linkeditData;
    const auto rebaseOffset = linkedit.Fileoff + linkeditData.size(); linkeditData.insert(linkeditData.end(), rebase.begin(), rebase.end()); while (linkeditData.size() % 8) linkeditData.push_back(0);
    const auto bindOffset = linkedit.Fileoff + linkeditData.size(); linkeditData.insert(linkeditData.end(), bind.begin(), bind.end()); while (linkeditData.size() % 8) linkeditData.push_back(0);
    Bytes exportTrie;
    if (module) {
        std::vector<std::pair<std::string, std::uint64_t>> entries;
        for (const auto& item : exports) entries.emplace_back("_" + item.Name, GuestBase + item.Vaddr - HeaderVmaddr);
        exportTrie = ExportTrie(entries).Serialize();
    }
    const auto exportOffset = linkedit.Fileoff + linkeditData.size(); linkeditData.insert(linkeditData.end(), exportTrie.begin(), exportTrie.end()); while (linkeditData.size() % 8) linkeditData.push_back(0);
    const auto stringOffset = linkedit.Fileoff + linkeditData.size(); linkeditData.push_back(0); linkeditData.push_back(0); while (linkeditData.size() % 8) linkeditData.push_back(0);
    linkedit.Filesize = linkeditData.size();
    linkedit.Vmsize = alignUp(linkeditData.size(), Page);
    layout.Segments.push_back(linkedit);

    // Load commands.
    Bytes commands;
    std::uint32_t commandCount = 0;
    const auto segmentCommand = [&](const Segment& segment) {
        Bytes command;
        appendU32(command, LcSegment64); appendU32(command, 0);
        appendName16(command, segment.Name.c_str());
        appendU64(command, segment.Vmaddr); appendU64(command, segment.Vmsize); appendU64(command, segment.Fileoff); appendU64(command, segment.Filesize);
        appendU32(command, segment.Name == "__PAGEZERO" ? 0 : segment.Prot); appendU32(command, segment.Name == "__PAGEZERO" ? 0 : segment.Prot);
        appendU32(command, static_cast<std::uint32_t>(segment.Sections.size())); appendU32(command, 0);
        for (const auto& section : segment.Sections) {
            appendName16(command, section.Name.c_str()); appendName16(command, segment.Name.c_str());
            appendU64(command, section.Vmaddr); appendU64(command, section.Data.size());
            appendU32(command, static_cast<std::uint32_t>(section.Fileoff)); appendU32(command, 3); appendU32(command, 0); appendU32(command, 0);
            appendU32(command, section.Name == "__mod_init_func" ? SectionModInitFuncPointers : section.Name == "__text" ? 0x80000400u : SectionRegular); appendU32(command, 0); appendU32(command, 0); appendU32(command, 0);
        }
        Io::WriteU32(command, 4, static_cast<std::uint32_t>(command.size()));
        commands.insert(commands.end(), command.begin(), command.end());
        ++commandCount;
    };
    for (const auto& segment : layout.Segments) segmentCommand(segment);
    {
        Bytes command; appendU32(command, LcDyldInfoOnly); appendU32(command, 48);
        appendU32(command, static_cast<std::uint32_t>(rebaseOffset)); appendU32(command, static_cast<std::uint32_t>(rebase.size()));
        appendU32(command, static_cast<std::uint32_t>(bindOffset)); appendU32(command, static_cast<std::uint32_t>(bind.size()));
        appendU32(command, 0); appendU32(command, 0); appendU32(command, 0); appendU32(command, 0);
        appendU32(command, static_cast<std::uint32_t>(exportOffset)); appendU32(command, static_cast<std::uint32_t>(exportTrie.size()));
        commands.insert(commands.end(), command.begin(), command.end()); ++commandCount;
    }
    {
        Bytes command; appendU32(command, LcSymtab); appendU32(command, 24); appendU32(command, 0); appendU32(command, 0); appendU32(command, static_cast<std::uint32_t>(stringOffset)); appendU32(command, 2);
        commands.insert(commands.end(), command.begin(), command.end()); ++commandCount;
    }
    {
        Bytes command; appendU32(command, LcDysymtab); appendU32(command, 80); for (int i = 0; i < 18; ++i) appendU32(command, 0);
        commands.insert(commands.end(), command.begin(), command.end()); ++commandCount;
    }
    {
        Bytes command; appendU32(command, LcLoadDylinker); appendU32(command, 0); appendU32(command, 12); appendPadded(command, "/usr/lib/dyld", 8);
        Io::WriteU32(command, 4, static_cast<std::uint32_t>(command.size())); commands.insert(commands.end(), command.begin(), command.end()); ++commandCount;
    }
    {
        Bytes command; appendU32(command, LcBuildVersion); appendU32(command, 24); appendU32(command, 1); appendU32(command, 0x000b0000); appendU32(command, 0x000b0000); appendU32(command, 0);
        commands.insert(commands.end(), command.begin(), command.end()); ++commandCount;
    }
    if (module) {
        Bytes command; appendU32(command, LcIdDylib); appendU32(command, 0); appendU32(command, 24); appendU32(command, 1); appendU32(command, 0x10000); appendU32(command, 0x10000); appendPadded(command, "@rpath/" + _outputName, 8);
        Io::WriteU32(command, 4, static_cast<std::uint32_t>(command.size())); commands.insert(commands.end(), command.begin(), command.end()); ++commandCount;
    } else {
        Bytes command; appendU32(command, LcMain); appendU32(command, 24); appendU64(command, stubVmaddr - HeaderVmaddr); appendU64(command, 0);
        commands.insert(commands.end(), command.begin(), command.end()); ++commandCount;
    }
    std::vector<std::string> dylibs = {"/usr/lib/libSystem.B.dylib"};
    for (const auto& library : libraries) dylibs.push_back("@rpath/" + library);
    for (const auto& dylib : dylibs) {
        Bytes command; appendU32(command, LcLoadDylib); appendU32(command, 0); appendU32(command, 24); appendU32(command, 2); appendU32(command, 0); appendU32(command, 0); appendPadded(command, dylib, 8);
        Io::WriteU32(command, 4, static_cast<std::uint32_t>(command.size())); commands.insert(commands.end(), command.begin(), command.end()); ++commandCount;
    }
    {
        Bytes command; appendU32(command, LcRpath); appendU32(command, 0); appendU32(command, 12); appendPadded(command, rpath, 8);
        Io::WriteU32(command, 4, static_cast<std::uint32_t>(command.size())); commands.insert(commands.end(), command.begin(), command.end()); ++commandCount;
    }
    if (32 + commands.size() > stubVmaddr - HeaderVmaddr) throw Domain::RelinkerException("Mach-O load commands exceed the header page");

    // Assemble the file.
    Bytes out(static_cast<std::size_t>(fileCursor + linkedit.Filesize), 0);
    Bytes header;
    appendU32(header, MhMagic64); appendU32(header, CpuTypeX86_64); appendU32(header, CpuSubtypeX86_64All); appendU32(header, module ? MhDylib : MhExecute);
    appendU32(header, commandCount); appendU32(header, static_cast<std::uint32_t>(commands.size())); appendU32(header, module ? MhFlagsDylib : MhFlagsExecute); appendU32(header, 0);
    std::copy(header.begin(), header.end(), out.begin());
    std::copy(commands.begin(), commands.end(), out.begin() + 32);
    std::copy(stub.begin(), stub.end(), out.begin() + static_cast<std::ptrdiff_t>(stubVmaddr - HeaderVmaddr));
    for (const auto& segment : layout.Segments)
        for (const auto& member : segment.Members)
            std::copy_n(image.begin() + static_cast<std::ptrdiff_t>(member.Offset), static_cast<std::ptrdiff_t>(member.FileSize), out.begin() + static_cast<std::ptrdiff_t>(segment.Fileoff + (GuestBase + member.MappedAddress - segment.Vmaddr)));
    for (const auto& segment : layout.Segments)
        for (const auto& section : segment.Sections)
            if (section.Name != "__text") std::copy(section.Data.begin(), section.Data.end(), out.begin() + static_cast<std::ptrdiff_t>(section.Fileoff));
    std::copy(linkeditData.begin(), linkeditData.end(), out.begin() + static_cast<std::ptrdiff_t>(linkedit.Fileoff));

    // RELATIVE relocation targets hold GuestBase + addend before dyld adds the slide; TLS module
    // relocations hold the module id and the offset inside the module's thread block.
    for (const auto& relocation : relocations) {
        if (relocation.Type != RelocationRelative && relocation.Type != RelocationTlsModule && relocation.Type != RelocationTlsOffset) continue;
        const auto [segmentIndex, offset] = layout.Locate(GuestBase + relocation.Address);
        const auto& segment = layout.Segments[segmentIndex];
        if (offset + 8 > segment.Filesize) throw Domain::RelinkerException("Relocation outside the segment file contents", relocation.Address);
        std::uint64_t value = GuestBase + static_cast<std::uint64_t>(relocation.Addend);
        if (relocation.Type == RelocationTlsModule) {
            if (!publishTls || !module) throw Domain::RelinkerException("TLS module relocation in an image without module thread storage", relocation.Address);
            value = moduleId;
        } else if (relocation.Type == RelocationTlsOffset) {
            value = static_cast<std::uint64_t>(relocation.Addend);
        }
        Io::WriteU64(out, static_cast<std::size_t>(segment.Fileoff + offset), value);
    }
    (void)tlsRewrite;
    return out;
}

}

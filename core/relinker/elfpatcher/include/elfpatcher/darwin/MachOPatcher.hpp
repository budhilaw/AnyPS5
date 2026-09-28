#ifndef ELFPATCHER_DARWIN_MACHOPATCHER_HPP
#define ELFPATCHER_DARWIN_MACHOPATCHER_HPP

#include <elfpatcher/general/IElfPatcher.hpp>
#include <string>

namespace Elfpatcher::Darwin {

// Emits an x86-64 Mach-O executable for macOS (natively on Intel, under Rosetta 2 on Apple
// Silicon). Guest segments keep their relative layout above a 4 GiB __PAGEZERO, ELF RELATIVE
// relocations become dyld rebases, NID imports become flat-namespace dyld binds against the
// .prx dylibs named by DT_NEEDED, and guest metadata (process parameters, exception tables,
// thread-local storage) is published in an __ANYPS5 segment for libc.prx and libkernel.prx.
// A PS5 module (ET_SCE_DYNAMIC) becomes an MH_DYLIB named @rpath/<outputName>: its exported NIDs
// go into the export trie, its DT_INIT runs as a dyld initializer and its thread storage template
// is published as __modtls for libkernel's __tls_get_addr.
class MachOPatcher : public IElfPatcher {
public:
    explicit MachOPatcher(std::string outputName) : _outputName(std::move(outputName)) {}
    std::vector<std::uint8_t> Patch(
        const std::vector<std::uint8_t>& sourceElf,
        const std::vector<Domain::ProgramHeader>& originalHeaders,
        const Domain::SysVDynamicSection& dynamicSection,
        std::uint64_t originalPltGotVaddr,
        const std::string& runPath,
        bool lazyBinding,
        bool dependencyDiagnostics
    ) override;

private:
    std::string _outputName;
};

}

#endif

#ifndef ELFPATCHER_DARWIN_MACHOPATCHER_HPP
#define ELFPATCHER_DARWIN_MACHOPATCHER_HPP

#include <elfpatcher/general/IElfPatcher.hpp>
#include <string>

namespace Elfpatcher::Darwin {

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

#ifndef RELINKER_ANALYSIS_PLTTHUNKRESTORER_HPP
#define RELINKER_ANALYSIS_PLTTHUNKRESTORER_HPP

#include <relinker/domain/RelinkResult.hpp>
#include <string>
#include <vector>

namespace Relinker {

struct RestoredPltThunks {
    std::vector<RelinkPatch> Patches;
    std::vector<std::string> Nids;
};

RestoredPltThunks RestorePatchedPltThunks(const std::vector<NidReference>& references, const std::vector<std::uint8_t>& text, VirtualAddress textVaddr, FileByteOffset textOffset, FileByteOffset tableOffset);

}

#endif

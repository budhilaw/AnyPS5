#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_POSTEDWRITES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_POSTEDWRITES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <utility>

namespace AgcDriver {

class PostedWrites {
public:
    static constexpr unsigned RegionShift = 21;
    static constexpr std::uint64_t MaxRegions = 64;
    static constexpr std::uint64_t PruneInterval = 64;

    void Post(std::uint64_t job, std::uint64_t first, std::uint64_t last) {
        const auto begin = first >> RegionShift;
        const auto end = last > first ? (last - 1) >> RegionShift : begin;
        if (end - begin >= MaxRegions) {
            wideRanges[{first, last}] = job;
            return;
        }
        indexedRanges[{first, last}] = job;
        for (auto region = begin; region <= end; ++region) regionWriters[region] = job;
    }

    void PostUnknown(std::uint64_t job) {
        unknownWriter = job;
    }

    void Prune(std::uint64_t completed) {
        if (completed - prunedAt < PruneInterval) return;
        prunedAt = completed;
        const auto done = [completed](const auto& entry) { return entry.second <= completed; };
        std::erase_if(indexedRanges, done);
        std::erase_if(wideRanges, done);
        std::erase_if(regionWriters, done);
    }

    std::uint64_t Writer(std::uint64_t address, std::size_t bytes, std::uint64_t completed) const {
        const auto end = address + bytes;
        const auto writer = std::max(unknownWriter > completed ? unknownWriter : std::uint64_t{0}, pendingWriter(wideRanges, address, end, completed));
        if (end >= address) {
            const auto begin = address >> RegionShift;
            const auto last = bytes != 0 ? (end - 1) >> RegionShift : begin;
            if (last - begin < MaxRegions && !pendingRegion(begin, last, completed)) return writer;
        }
        return std::max(writer, pendingWriter(indexedRanges, address, end, completed));
    }

    std::size_t IndexedRegions() const {
        return regionWriters.size();
    }

    std::size_t WideRanges() const {
        return wideRanges.size();
    }

private:
    using Ranges = std::map<std::pair<std::uint64_t, std::uint64_t>, std::uint64_t>;

    static std::uint64_t pendingWriter(const Ranges& posted, std::uint64_t address, std::uint64_t end, std::uint64_t completed) {
        std::uint64_t writer = 0;
        for (auto it = posted.begin(), stop = posted.lower_bound({end, 0}); it != stop; ++it) {
            if (it->second > completed && address < it->first.second) writer = std::max(writer, it->second);
        }
        return writer;
    }

    bool pendingRegion(std::uint64_t begin, std::uint64_t last, std::uint64_t completed) const {
        for (auto region = begin; region <= last; ++region) {
            const auto found = regionWriters.find(region);
            if (found != regionWriters.end() && found->second > completed) return true;
        }
        return false;
    }

    Ranges indexedRanges;
    Ranges wideRanges;
    std::unordered_map<std::uint64_t, std::uint64_t> regionWriters;
    std::uint64_t unknownWriter = 0;
    std::uint64_t prunedAt = 0;
};

}

#endif

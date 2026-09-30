#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/LruCache.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using AgcDriver::Graphics::LruCache;
using AgcDriver::Graphics::Require;

auto recordInto(std::vector<std::string>& keys) {
    return [&keys](const auto& entry) { keys.push_back(entry.key); };
}

}

void RunLruCacheTests() {
    {
        LruCache<std::string, int> cache(3);
        std::vector<std::string> evicted;
        const auto record = recordInto(evicted);
        for (const auto* key : {"a", "b", "c"}) Require(cache.Insert(key, 1, record).second, "a new pipeline key was not inserted");
        Require(cache.Size() == 3 && evicted.empty(), "pipelines within the limit were evicted");
        Require(cache.Find("a") != cache.End(), "a cached pipeline was not found");
        Require(cache.Insert("d", 4, record).second, "a pipeline beyond the limit was not inserted");
        Require(evicted == std::vector<std::string>{"b"}, "inserting beyond the limit did not evict exactly the least recently used pipeline");
        Require(cache.Size() == 3 && cache.Contains("a") && cache.Contains("c") && cache.Contains("d") && !cache.Contains("b"), "the most recently used pipelines were not kept");
        Require(cache.Insert("e", 5, record).second && evicted == std::vector<std::string>{"b", "c"}, "the next insertion did not evict the next least recently used pipeline alone");
    }
    {
        LruCache<std::string, int> cache(2);
        std::vector<std::string> evicted;
        const auto record = recordInto(evicted);
        const auto first = cache.Insert("first", 1, record).first;
        cache.Insert("second", 2, record);
        cache.Touch(first);
        cache.Insert("third", 3, record);
        Require(evicted == std::vector<std::string>{"second"}, "touching a pipeline through its handle did not keep it");
        Require(cache.Size() == 2 && cache.Contains("first") && cache.Contains("third"), "touching a pipeline changed the cache contents");
    }
    {
        LruCache<std::string, int> cache(2);
        std::vector<std::string> evicted;
        const auto record = recordInto(evicted);
        cache.Insert("probe", 1, record);
        cache.Insert("other", 2, record);
        Require(cache.Contains("probe"), "a cached pipeline was reported missing");
        cache.Insert("next", 3, record);
        Require(evicted == std::vector<std::string>{"probe"}, "checking for a pipeline counted as a use");
    }
    {
        LruCache<std::string, int> cache(2);
        std::vector<std::string> evicted;
        const auto record = recordInto(evicted);
        const auto original = cache.Insert("same", 1, record).first;
        cache.Insert("other", 2, record);
        const auto [position, inserted] = cache.Insert("same", 9, record);
        Require(!inserted && position == original && position->value == 1, "a duplicate pipeline replaced the cached one");
        Require(evicted.empty() && cache.Size() == 2, "a duplicate pipeline evicted another one");
        cache.Insert("next", 3, record);
        Require(evicted == std::vector<std::string>{"other"}, "a duplicate insertion did not count as a use of the cached pipeline");
    }
    {
        constexpr std::size_t limit = 4;
        LruCache<std::string, int> cache(limit);
        std::vector<std::string> evicted;
        const auto record = recordInto(evicted);
        for (std::size_t i = 0; i < 100; ++i) {
            Require(cache.Insert(std::to_string(i), static_cast<int>(i), record).second, "a new pipeline key was not inserted");
            Require(evicted.size() == (i + 1 > limit ? i + 1 - limit : 0) && cache.Size() == std::min(i + 1, limit), "an insertion evicted more or fewer than one pipeline");
        }
        for (std::size_t i = 96; i < 100; ++i) Require(cache.Contains(std::to_string(i)), "the most recently inserted pipelines were not kept");
        Require(evicted.front() == "0" && evicted.back() == "95", "pipelines were not evicted oldest first");
    }
    {
        LruCache<std::string, int> cache(3);
        std::vector<std::string> evicted;
        const auto record = recordInto(evicted);
        for (int i = 0; i < 3; ++i) cache.Insert(std::to_string(i), i, record);
        cache.EraseIf([](const auto& entry) { return entry.value != 1; });
        Require(cache.Size() == 1 && cache.Contains("1") && evicted.empty(), "erasing abandoned pipelines removed the wrong entries or counted as evictions");
        cache.Insert("3", 3, record);
        cache.Insert("4", 4, record);
        Require(evicted.empty() && cache.Size() == 3, "erased pipelines still counted toward the limit");
    }
    {
        using Cache = LruCache<std::string, int>;
        Cache cache(2);
        std::unordered_map<int, Cache::Handle> byHash;
        const auto forget = [&byHash](const Cache::Entry& evicted) {
            if (const auto found = byHash.find(evicted.value); found != byHash.end() && &*found->second == &evicted) byHash.erase(found);
        };
        byHash.emplace(1, cache.Insert("one", 1, forget).first);
        cache.Insert("alias", 1, forget);
        cache.Touch(byHash.at(1));
        cache.Insert("three", 3, forget);
        Require(byHash.contains(1) && byHash.at(1)->key == "one" && !cache.Contains("alias"), "evicting a pipeline that shares a SPIR-V hash dropped the index entry of another pipeline");
        cache.Insert("four", 4, forget);
        Require(!byHash.contains(1) && !cache.Contains("one"), "evicting an indexed pipeline left its SPIR-V hash index entry behind");
    }
    {
        LruCache<std::string, std::shared_ptr<int>> cache(1);
        std::vector<std::string> evicted;
        const auto record = recordInto(evicted);
        auto queued = std::make_shared<int>(7);
        cache.Insert("queued", queued, record);
        cache.Insert("new", std::make_shared<int>(8), record);
        Require(evicted == std::vector<std::string>{"queued"} && queued.use_count() == 1 && *queued == 7, "an evicted pipeline that queued work still references was not left to that reference");
        cache.Clear();
        Require(cache.Size() == 0 && !cache.Contains("new"), "clearing the pipeline cache kept entries");
    }
    {
        LruCache<std::string, int> cache(0);
        std::vector<std::string> evicted;
        const auto [position, inserted] = cache.Insert("only", 1, recordInto(evicted));
        Require(inserted && position != cache.End() && position->key == "only" && cache.Size() == 1 && evicted.empty(), "a zero limit evicted the pipeline being inserted");
    }
    {
        LruCache<std::string, int> cache(3);
        std::vector<std::string> evicted;
        const auto record = recordInto(evicted);
        const auto first = cache.Insert("first", 1, record).first;
        cache.Insert("second", 2, record);
        cache.Erase(first);
        Require(cache.Size() == 1 && !cache.Contains("first") && cache.Contains("second") && evicted.empty(), "erasing a pipeline through its handle removed the wrong entry or counted as an eviction");
        Require(cache.Insert("first", 3, record).second && cache.Find("first")->value == 3, "an erased pipeline key could not be inserted again");
    }
}

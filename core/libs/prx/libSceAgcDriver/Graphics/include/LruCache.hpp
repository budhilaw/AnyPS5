#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_LRUCACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_LRUCACHE_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <unordered_map>
#include <utility>

namespace AgcDriver::Graphics {

template<typename TKey, typename TValue, typename THash = std::hash<TKey>>
class LruCache {
public:
    struct Entry {
        TKey key;
        TValue value;
    };
    using Handle = typename std::list<Entry>::iterator;

    explicit LruCache(std::size_t capacity) : capacity(std::max<std::size_t>(capacity, 1)) {}
    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    std::size_t Size() const { return entries.size(); }
    bool Contains(const TKey& key) const { return lookup.contains(key); }
    Handle End() { return entries.end(); }

    Handle Find(const TKey& key) {
        const auto found = lookup.find(key);
        if (found == lookup.end()) return entries.end();
        Touch(found->second);
        return found->second;
    }

    void Touch(Handle entry) { entries.splice(entries.end(), entries, entry); }

    template<typename TEvicted>
    std::pair<Handle, bool> Insert(TKey key, TValue value, TEvicted&& evicted) {
        if (const auto found = Find(key); found != entries.end()) return {found, false};
        entries.push_back(Entry{std::move(key), std::move(value)});
        const auto inserted = std::prev(entries.end());
        try {
            lookup.emplace(inserted->key, inserted);
        } catch (...) {
            entries.pop_back();
            throw;
        }
        while (entries.size() > capacity) {
            evicted(entries.front());
            lookup.erase(entries.front().key);
            entries.pop_front();
        }
        return {inserted, true};
    }

    void Erase(Handle entry) {
        lookup.erase(entry->key);
        entries.erase(entry);
    }

    template<typename TPredicate>
    void EraseIf(TPredicate&& predicate) {
        for (auto entry = entries.begin(); entry != entries.end();) {
            if (!predicate(*entry)) {
                ++entry;
                continue;
            }
            lookup.erase(entry->key);
            entry = entries.erase(entry);
        }
    }

    void Clear() {
        lookup.clear();
        entries.clear();
    }

private:
    std::size_t capacity;
    std::list<Entry> entries;
    std::unordered_map<TKey, Handle, THash> lookup;
};

}

#endif

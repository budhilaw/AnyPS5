#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_QUEUESTATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_QUEUESTATE_HPP

#include <cstdint>
#include <cstddef>
#include <array>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <utility>
#include <vector>

namespace AgcDriver {

// Register values by offset, with the map interface the decoders use: draws read hundreds of
// registers, so each is a direct index. A slot holds a register when its key equals its index.
class Registers {
public:
    using Entry = std::pair<std::uint32_t, std::uint32_t>;
    template<typename TEntry>
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Entry;
        using difference_type = std::ptrdiff_t;
        using pointer = TEntry*;
        using reference = TEntry&;
        Iterator() = default;
        Iterator(TEntry* base, TEntry* at, TEntry* last) : base(base), at(at), last(last) { skip(); }
        operator Iterator<const Entry>() const { return {base, at, last}; }
        TEntry& operator*() const { return *at; }
        TEntry* operator->() const { return at; }
        Iterator& operator++() { ++at; skip(); return *this; }
        Iterator operator++(int) { auto copy = *this; ++*this; return copy; }
        bool operator==(const Iterator& other) const { return at == other.at; }

    private:
        void skip() { while (at != last && at->first != static_cast<std::uint32_t>(at - base)) ++at; }
        TEntry* base = nullptr;
        TEntry* at = nullptr;
        TEntry* last = nullptr;
    };
    using iterator = Iterator<Entry>;
    using const_iterator = Iterator<const Entry>;

    Registers() = default;
    Registers(std::initializer_list<Entry> entries) { for (const auto& [key, value] : entries) emplace(key, value); }
    iterator begin() { return {slots.data(), slots.data(), slots.data() + slots.size()}; }
    iterator end() { return {slots.data(), slots.data() + slots.size(), slots.data() + slots.size()}; }
    const_iterator begin() const { return {slots.data(), slots.data(), slots.data() + slots.size()}; }
    const_iterator end() const { return {slots.data(), slots.data() + slots.size(), slots.data() + slots.size()}; }
    iterator find(std::uint32_t key) { return has(key) ? iterator{slots.data(), slots.data() + key, slots.data() + slots.size()} : end(); }
    const_iterator find(std::uint32_t key) const { return has(key) ? const_iterator{slots.data(), slots.data() + key, slots.data() + slots.size()} : end(); }
    bool contains(std::uint32_t key) const { return has(key); }
    std::size_t count(std::uint32_t key) const { return has(key) ? 1u : 0u; }
    std::size_t size() const { return used; }
    bool empty() const { return used == 0; }
    std::uint32_t& at(std::uint32_t key) {
        if (!has(key)) throw std::out_of_range("register is not set");
        return slots[key].second;
    }
    std::uint32_t at(std::uint32_t key) const {
        if (!has(key)) throw std::out_of_range("register is not set");
        return slots[key].second;
    }
    std::uint32_t& operator[](std::uint32_t key) { return slot(key).second; }
    std::pair<iterator, bool> insert_or_assign(std::uint32_t key, std::uint32_t value) {
        const bool added = !has(key);
        slot(key).second = value;
        return {find(key), added};
    }
    std::pair<iterator, bool> emplace(std::uint32_t key, std::uint32_t value) {
        if (has(key)) return {find(key), false};
        slot(key).second = value;
        return {find(key), true};
    }
    std::size_t erase(std::uint32_t key) {
        if (!has(key)) return 0;
        slots[key] = {Absent, 0};
        --used;
        return 1;
    }
    void clear() { slots.clear(); used = 0; }
    bool operator==(const Registers& other) const {
        if (used != other.used) return false;
        for (const auto& [key, value] : *this) {
            if (!other.has(key) || other.slots[key].second != value) return false;
        }
        return true;
    }

private:
    static constexpr std::uint32_t Absent = ~0u;
    static constexpr std::uint32_t Limit = 0x20000;
    bool has(std::uint32_t key) const { return key < slots.size() && slots[key].first == key; }
    Entry& slot(std::uint32_t key) {
        if (key >= slots.size()) {
            if (key >= Limit) throw std::out_of_range("register offset is out of range");
            slots.resize(std::max<std::size_t>(key + 1, slots.size() * 2), Entry{Absent, 0});
        }
        if (slots[key].first != key) {
            slots[key] = {key, 0};
            ++used;
        }
        return slots[key];
    }
    std::vector<Entry> slots;
    std::size_t used = 0;
};

inline const Registers& InitialContextRegisters() {
    static const Registers initial = [] {
        Registers result{
            {0x200, 0}, {0x201, 0}, {0x202, 0xcc0010}, {0x203, 0},
            {0x204, 0}, {0x205, 0}, {0x206, 1087}, {0x207, 0},
            {0x0, 0}, {0x2, 0}, {0x3, 0}, {0x4, 0}, {0x8, 0}, {0x9, 0x3f800000}, {0xa, 0}, {0xb, 0},
            {0x80, 0}, {0x83, 0xffff}, {0x8c, 0xaa99aaaa}, {0x8d, 0}, {0x8e, 0}, {0x8f, 0},
            {0xc, 0}, {0xd, 0x40004000}, {0x81, 0x80000000}, {0x82, 0x40004000},
            {0x90, 0x80000000}, {0x91, 0x40004000},
            {0x105, 0}, {0x106, 0}, {0x107, 0}, {0x108, 0},
            // SPI_VS_OUT_CONFIG, SPI_PS_INPUT_ENA/ADDR (the cleared state enables the perspective
            // center inputs), SPI_INTERP_CONTROL_0, SPI_PS_IN_CONTROL, SPI_BARYC_CNTL, ...
            {0x1b1, 0}, {0x1b3, 0x2}, {0x1b4, 0x2}, {0x1b5, 0}, {0x1b6, 0}, {0x1b8, 0}, {0x1c3, 0}, {0x1c4, 0}, {0x1c5, 0},
            {0x1ff, 0}, {0x292, 2}, {0x293, 0}, {0x29b, 0},
            {0x2ce, 0}, {0x2d3, 0}, {0x2d5, 0}, {0x2d6, 0}, {0x2db, 0},
            {0x2dc, 0xaa00}, {0x2e4, 0}, {0x2f8, 0}, {0x2f9, 0x2d},
            {0x30e, 0xffffffff}, {0x30f, 0xffffffff}, {0x313, 0x6000},
            {0x318, 0}, {0x31b, 0}, {0x31c, 0}, {0x31d, 0},
            {0x390, 0}, {0x3b0, 0}, {0x3b8, 0}
        };
        for (std::uint32_t i = 0; i < 8; ++i) result.emplace(0x1e0 + i, 0x20010001);
        for (std::uint32_t i = 0; i < 32; ++i) result.emplace(0x191 + i, 0); // SPI_PS_INPUT_CNTL_n
        for (std::uint32_t i = 0; i < 16; ++i) {
            result.emplace(0x94 + 2 * i, 0x80000000);
            result.emplace(0x95 + 2 * i, 0x40004000);
            result.emplace(0xb4 + 2 * i, 0);
            result.emplace(0xb5 + 2 * i, 0);
            for (std::uint32_t j = 0; j < 6; ++j) result.emplace(0x10f + 6 * i + j, j % 2 == 0 ? 0x3f800000 : 0);
        }
        return result;
    }();
    return initial;
}

struct QueueState {
    std::uint32_t id = 0;  // the queue of the submission being executed (diagnostics)
    Registers shader;
    Registers context = InitialContextRegisters();
    Registers userConfig{{0x24a, 0}, {0x24b, 0}};
    std::optional<Registers> savedContext;
    std::array<std::uint32_t, 0x3000> constantRam{};
    std::uint64_t indexBase = 0;
    std::uint64_t drawIndirectBase = 0;
    std::uint64_t dispatchIndirectBase = 0;
    std::uint32_t indexBufferSize = 0;
    std::uint32_t indexType = 0;
    std::uint32_t instanceCount = 1;
    std::vector<std::string> markers;

    void ClearContext() {
        context = InitialContextRegisters();
    }
};

}

#endif

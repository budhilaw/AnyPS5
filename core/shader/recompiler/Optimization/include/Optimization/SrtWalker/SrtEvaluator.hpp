#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTEVALUATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTEVALUATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace ShaderRecompiler::Detail {

class EvaluatedValues {
public:
    const std::uint64_t* Find(const IrValue* key) const {
        const auto mask = capacity() - 1;
        for (auto i = slot(key); ; i = (i + 1) & mask) {
            const auto* current = keyAt(i);
            if (current == key) return &valueAt(i);
            if (current == nullptr) return nullptr;
        }
    }

    void Insert(const IrValue* key, std::uint64_t value) {
        if ((count + 1) * 2 > capacity()) grow();
        const auto mask = capacity() - 1;
        for (auto i = slot(key); ; i = (i + 1) & mask) {
            if (keyAt(i) == nullptr) {
                keyAt(i) = key;
                valueAt(i) = value;
                ++count;
                return;
            }
            if (keyAt(i) == key) {
                valueAt(i) = value;
                return;
            }
        }
    }

private:
    static constexpr std::size_t InlineSlots = 128;
    std::size_t capacity() const { return heapKeys.empty() ? InlineSlots : heapKeys.size(); }
    std::size_t slot(const IrValue* key) const { return static_cast<std::size_t>((reinterpret_cast<std::uintptr_t>(key) >> 4u) * 0x9e3779b97f4a7c15ull >> 40u) & (capacity() - 1); }
    const IrValue*& keyAt(std::size_t index) { return heapKeys.empty() ? inlineKeys[index] : heapKeys[index]; }
    const IrValue* keyAt(std::size_t index) const { return heapKeys.empty() ? inlineKeys[index] : heapKeys[index]; }
    std::uint64_t& valueAt(std::size_t index) { return heapKeys.empty() ? inlineValues[index] : heapValues[index]; }
    const std::uint64_t& valueAt(std::size_t index) const { return heapKeys.empty() ? inlineValues[index] : heapValues[index]; }
    void grow() {
        std::vector<std::pair<const IrValue*, std::uint64_t>> entries;
        entries.reserve(count);
        for (std::size_t i = 0; i < capacity(); ++i) {
            if (keyAt(i) != nullptr) entries.emplace_back(keyAt(i), valueAt(i));
        }
        const auto next = capacity() * 2;
        heapKeys.assign(next, nullptr);
        heapValues.assign(next, 0);
        count = 0;
        for (const auto& [key, value] : entries) Insert(key, value);
    }

    std::array<const IrValue*, InlineSlots> inlineKeys{};
    std::array<std::uint64_t, InlineSlots> inlineValues{};
    std::vector<const IrValue*> heapKeys;
    std::vector<std::uint64_t> heapValues;
    std::size_t count = 0;
};

class Evaluator {
public:
    Evaluator(const IrResourcePlan& program, const SrtRuntime& runtime, std::span<const std::uint8_t> cleanFlatSlots = {}, Evaluator* cleanEvaluator = nullptr, IrValue* activeMask = nullptr) : _program(program), _runtime(runtime), _cleanFlatSlots(cleanFlatSlots), _cleanEvaluator(cleanEvaluator), _activeMask(activeMask != nullptr ? activeMask->Resolve() : nullptr) {}

    bool Evaluate(IrValue* value, std::uint32_t& result);
    bool EvaluateWide(IrValue* raw, std::uint64_t& result);

private:
    static float Float32(std::uint64_t bits);
    static std::uint64_t Float32Bits(float value);

    bool Arg(IrValue& inst, std::size_t index, std::uint64_t& result);
    bool EvaluatePhi(IrValue& inst, std::uint64_t& result);
    bool EvaluateExtract(IrValue& inst, std::uint64_t& result);
    bool EvaluateRawRead(IrValue& inst, std::uint64_t& result);
    bool EvaluateInst(IrValue& inst, std::uint64_t& result);

    const IrResourcePlan& _program;
    const SrtRuntime& _runtime;
    std::span<const std::uint8_t> _cleanFlatSlots;
    Evaluator* _cleanEvaluator = nullptr;
    IrValue* _activeMask = nullptr;
    EvaluatedValues _cache;
    std::vector<IrValue*> _visiting;
};

}

#endif

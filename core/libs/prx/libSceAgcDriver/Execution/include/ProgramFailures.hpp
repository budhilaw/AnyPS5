#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PROGRAMFAILURES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PROGRAMFAILURES_HPP

#include "CacheKey.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Translation/ShaderInputInfoBuilder.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <unordered_map>
#include <utility>
#include <vector>

namespace AgcDriver {

class ProgramFailures {
public:
    enum class Stage {
        Plan,
        Capture,
        Compile,
        Validate
    };

    struct Key {
        Stage stage = Stage::Capture;
        std::uint64_t codeHash = 0;
        std::vector<std::uint64_t> source;
        ShaderRecompiler::BindingLayout layout{};
        ShaderRecompiler::ResourceSpecialization specialization;
    };

    static Key PlanKey(const ShaderRecompiler::RecompileRequest& request) {
        Key key{Stage::Plan, request.shader.codeHash, {}, {}, {}};
        if (!keyedSource(request, key.source)) key.stage = Stage::Capture;
        return key;
    }

    static Key CompileKey(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::ResourceSpecialization& specialization) {
        Key key{Stage::Compile, request.shader.codeHash, {}, request.layout, specialization};
        if (!keyedSource(request, key.source)) {
            key.stage = Stage::Capture;
            key.specialization = {};
        }
        return key;
    }

    static Key ProgramKey(Stage stage, std::uint64_t codeHash) {
        return {stage, codeHash, {}, {}, {}};
    }

    static bool Skipping(const Key& key) {
        return key.stage == Stage::Plan || key.stage == Stage::Compile;
    }

    bool Skips(const ShaderRecompiler::RecompileRequest& request) {
        return matches(request, nullptr);
    }

    bool Skips(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::ResourceSpecialization& specialization) {
        return matches(request, &specialization);
    }

    bool Contains(const Key& key) const {
        const auto found = byCode.find(key.codeHash);
        return found != byCode.end() && std::any_of(found->second.begin(), found->second.end(), [&](const Key& entry) { return same(entry, key); });
    }

    bool Record(Key key) {
        if (Contains(key)) return false;
        if (Skipping(key)) ++skipping;
        byCode[key.codeHash].push_back(std::move(key));
        return true;
    }

private:
    static bool buildSource(const ShaderRecompiler::RecompileRequest& request, std::vector<std::uint64_t>& source) {
        try {
            ShaderRecompiler::RecompileCacheKey::Build(request, source);
            return true;
        } catch (const std::exception&) {
            source.clear();
            return false;
        }
    }

    static bool vertexInputRejected(const ShaderRecompiler::RecompileRequest& request) {
        using ShaderRecompiler::ShaderStage;
        using ShaderRecompiler::ShaderStageKind;
        if (!request.context.vertex) return false;
        ShaderStageKind kind;
        switch (request.shader.stage) {
        case ShaderStage::Vertex: kind = ShaderStageKind::Vertex; break;
        case ShaderStage::Local: kind = ShaderStageKind::Local; break;
        case ShaderStage::TessellationControl: kind = ShaderStageKind::TessellationControl; break;
        case ShaderStage::TessellationEvaluation: kind = ShaderStageKind::TessellationEvaluation; break;
        case ShaderStage::Mesh: kind = ShaderStageKind::Mesh; break;
        default: return false;
        }
        try {
            static_cast<void>(ShaderRecompiler::BuildShaderStageInputInfo(kind, request.context, request.target.subgroupSize));
            return false;
        } catch (const std::exception&) {
            return true;
        }
    }

    static bool keyedSource(const ShaderRecompiler::RecompileRequest& request, std::vector<std::uint64_t>& source) {
        if (buildSource(request, source) && !vertexInputRejected(request)) return true;
        source.clear();
        return false;
    }

    static bool sameLayout(const ShaderRecompiler::BindingLayout& left, const ShaderRecompiler::BindingLayout& right) {
        return left.descriptorSet == right.descriptorSet && left.firstBinding == right.firstBinding && left.pushConstantOffsetBytes == right.pushConstantOffsetBytes && left.pushConstantSizeBytes == right.pushConstantSizeBytes;
    }

    static bool same(const Key& left, const Key& right) {
        if (left.stage != right.stage || left.codeHash != right.codeHash || left.source != right.source) return false;
        return left.stage != Stage::Compile || (sameLayout(left.layout, right.layout) && left.specialization == right.specialization);
    }

    bool matches(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::ResourceSpecialization* specialization) {
        if (skipping == 0) return false;
        const auto found = byCode.find(request.shader.codeHash);
        if (found == byCode.end()) return false;
        const auto stage = specialization == nullptr ? Stage::Plan : Stage::Compile;
        bool built = false;
        for (const auto& entry : found->second) {
            if (entry.stage != stage) continue;
            if (!built) {
                if (!buildSource(request, scratch)) return false;
                built = true;
            }
            if (entry.source != scratch) continue;
            if (specialization == nullptr || (sameLayout(entry.layout, request.layout) && entry.specialization == *specialization)) return true;
        }
        return false;
    }

    std::unordered_map<std::uint64_t, std::vector<Key>> byCode;
    std::vector<std::uint64_t> scratch;
    std::size_t skipping = 0;
};

}

#endif

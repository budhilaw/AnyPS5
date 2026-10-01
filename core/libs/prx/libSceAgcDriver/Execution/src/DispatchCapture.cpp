#include "prx/libSceAgcDriver/Execution/include/DispatchCapture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuTimestamps.hpp"
#include "prx/libc/include/General.hpp"
#include <charconv>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>

namespace AgcDriver {

namespace {

constexpr std::uint32_t MaxCapturesPerSelector = 1000;
constexpr std::uint32_t MaxCostliestPrograms = 20;

std::string_view trimmed(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

bool parseHex(std::string_view text, std::uint64_t& value) {
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text.remove_prefix(2);
    if (text.empty() || text.size() > 16) return false;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    return error == std::errc() && end == text.data() + text.size();
}

std::string hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

struct SelectionState {
    std::mutex mutex;
    DispatchCaptureSettings settings;
    std::vector<std::uint32_t> taken;
    bool announced = false;
    bool ranked = false;
    std::optional<std::uint64_t> firstReport;
};

SelectionState& selection() {
    static auto* state = [] {
        auto* created = new SelectionState();
        created->settings = ParseDispatchCaptureSettings(std::getenv("ANYPS5_CAPTURE_DISPATCH"), std::getenv("ANYPS5_CAPTURE_DIR"), std::getenv("ANYPS5_CAPTURE_AFTER"));
        created->taken.assign(created->settings.selectors.size(), 0);
        return created;
    }();
    return *state;
}

std::string replayTool() {
#if defined(_WIN32)
    constexpr const char* name = "dispatch_replay.exe";
#else
    constexpr const char* name = "dispatch_replay";
#endif
    std::error_code error;
    const auto directory = (std::filesystem::current_path(error) / ".." / "core" / "libs" / "prx" / "libSceAgcDriver").lexically_normal();
    if (!error && std::filesystem::is_directory(directory, error)) return (directory / name).generic_string();
    return name;
}

void announce(const DispatchCaptureSettings& settings) {
    std::string selectors;
    for (const auto& selector : settings.selectors) selectors += " " + hex(selector.value) + (selector.count == 1 ? std::string() : " x" + std::to_string(selector.count));
    if (settings.costliest != 0) selectors += " and the " + std::to_string(settings.costliest) + " costliest programs of the first GPU time report that starts after then";
    APS5_LOG_OUT("[capture] capturing the dispatches of program address or code hash%s from %.1f s into %s; replay a capture with %s <capture directory>", selectors.c_str(), settings.after, settings.root.generic_string().c_str(), replayTool().c_str());
    for (const auto& error : settings.errors) APS5_LOG_OUT("[capture] ignored %s", error.c_str());
    if (settings.costliest != 0 && !Graphics::TraceGpuPasses()) APS5_LOG_OUT("[capture] the costliest programs are ranked by the GPU time report, which needs %s; nothing will be ranked", "ANYPS5_TRACE_GPU_PASSES=1");
}

void takeCostliest(SelectionState& state) {
    if (state.ranked || state.settings.costliest == 0) return;
    const auto ranking = Graphics::CostliestGpuPrograms();
    if (!state.firstReport) {
        state.firstReport = ranking.report;
        return;
    }
    if (ranking.report <= *state.firstReport) return;
    state.ranked = true;
    std::string chosen;
    for (std::size_t index = 0; index < ranking.programs.size() && index < state.settings.costliest; ++index) {
        state.settings.selectors.push_back({ranking.programs[index], 1});
        state.taken.push_back(0);
        chosen += " " + hex(ranking.programs[index]);
    }
    APS5_LOG_OUT("[capture] capturing the costliest programs of GPU time report %llu:%s", static_cast<unsigned long long>(ranking.report), chosen.empty() ? " none were timed" : chosen.c_str());
}

}

DispatchCaptureSettings ParseDispatchCaptureSettings(const char* selection, const char* directory, const char* after) {
    DispatchCaptureSettings settings;
    std::string_view text = selection != nullptr ? selection : "";
    while (!text.empty()) {
        const auto comma = text.find(',');
        const auto entry = trimmed(text.substr(0, comma));
        text.remove_prefix(comma == std::string_view::npos ? text.size() : comma + 1);
        if (entry.empty()) continue;
        if (entry.size() > 3 && entry.substr(0, 3) == "top") {
            const auto digits = entry.substr(3);
            std::uint32_t count = 0;
            const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), count);
            if (error != std::errc() || end != digits.data() + digits.size() || count == 0 || count > MaxCostliestPrograms) {
                settings.errors.push_back("ANYPS5_CAPTURE_DISPATCH entry '" + std::string(entry) + "': top needs a number of programs from 1 to " + std::to_string(MaxCostliestPrograms));
                continue;
            }
            settings.costliest = count;
            continue;
        }
        const auto colon = entry.find(':');
        DispatchCaptureSelector selector;
        if (!parseHex(trimmed(entry.substr(0, colon)), selector.value)) {
            settings.errors.push_back("ANYPS5_CAPTURE_DISPATCH entry '" + std::string(entry) + "': not a hexadecimal program address or code hash");
            continue;
        }
        if (colon != std::string_view::npos) {
            const auto count = trimmed(entry.substr(colon + 1));
            const auto [end, error] = std::from_chars(count.data(), count.data() + count.size(), selector.count);
            if (error != std::errc() || end != count.data() + count.size() || selector.count == 0 || selector.count > MaxCapturesPerSelector) {
                settings.errors.push_back("ANYPS5_CAPTURE_DISPATCH entry '" + std::string(entry) + "': the count must be a number from 1 to " + std::to_string(MaxCapturesPerSelector));
                continue;
            }
        }
        settings.selectors.push_back(selector);
    }
    std::error_code error;
    const auto root = directory != nullptr && *directory != '\0' ? std::filesystem::path(directory) : std::filesystem::current_path(error) / "dispatch-captures";
    settings.root = std::filesystem::absolute(root, error).lexically_normal();
    if (error) settings.root = root.lexically_normal();
    if (after != nullptr && *after != '\0') {
        char* end = nullptr;
        const auto seconds = std::strtod(after, &end);
        if (end == after || *end != '\0' || !(seconds >= 0.0)) settings.errors.push_back("ANYPS5_CAPTURE_AFTER '" + std::string(after) + "': not a number of seconds");
        else settings.after = seconds;
    }
    return settings;
}

bool DispatchCaptureEnabled() {
    const auto& settings = selection().settings;
    return settings.costliest != 0 || !settings.selectors.empty();
}

std::shared_ptr<const Graphics::CaptureTarget> SelectDispatchCapture(std::uint64_t program, std::uint64_t codeHash, const std::function<std::string()>& request) {
    auto& state = selection();
    if (state.settings.costliest == 0 && state.settings.selectors.empty()) return nullptr;
    std::lock_guard lock(state.mutex);
    if (!state.announced) {
        state.announced = true;
        announce(state.settings);
    }
    if (Aps5LogSeconds_nid_no_patch() < state.settings.after) return nullptr;
    takeCostliest(state);
    for (std::size_t index = 0; index < state.settings.selectors.size(); ++index) {
        const auto& selector = state.settings.selectors[index];
        if ((selector.value != program && selector.value != codeHash) || state.taken[index] >= selector.count) continue;
        auto target = std::make_shared<Graphics::CaptureTarget>();
        target->root = state.settings.root;
        target->program = program;
        target->codeHash = codeHash;
        target->index = ++state.taken[index];
        try {
            target->request = request();
        } catch (const std::exception& error) {
            APS5_LOG_OUT("[capture] the recompile request of program %s cannot be serialized (%s); its capture replays only the captured SPIR-V", hex(program).c_str(), error.what());
        }
        return target;
    }
    return nullptr;
}

std::string DispatchReplayCommand(const std::filesystem::path& capture) {
    return "\"" + replayTool() + "\" \"" + capture.generic_string() + "\"";
}

}

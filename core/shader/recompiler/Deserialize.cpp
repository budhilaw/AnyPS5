#include "Recompiler.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace ShaderRecompiler;

namespace {

constexpr std::string_view CacheHeader = "anyps5-shader-requests 1";
constexpr std::string_view BaselineHeader = "anyps5-recompile-baseline 1";
constexpr std::string_view RequestMarker = "RecompileRequest:";
constexpr const char* InputsVariable = "ANYPS5_RECOMPILE_REQUESTS";
constexpr std::size_t SlowestReported = 10;

#if defined(_WIN32)
constexpr char PathListSeparator = ';';
constexpr const char* ValidatorName = "spirv-val.exe";
#else
constexpr char PathListSeparator = ':';
constexpr const char* ValidatorName = "spirv-val";
#endif

struct ReplayOptions {
    std::vector<std::string> inputs;
    std::string baselinePath;
    std::string writeBaselinePath;
    std::string validator;
    std::string disassemblyDirectory;
    std::string spirvDirectory;
    double budgetMilliseconds = 0.0;
};

struct BaselineEntry {
    std::uint64_t spirvHash = 0;
    std::uint64_t metadataHash = 0;
    std::size_t spirvWords = 0;
    std::string program;
};

struct TimedEntry {
    double milliseconds;
    std::string label;
};

struct ReplayState {
    ReplayOptions options;
    std::map<std::uint64_t, BaselineEntry> baseline;
    std::unordered_set<std::uint64_t> baselineModules;
    std::unordered_set<std::uint64_t> replayed;
    std::unordered_map<std::uint64_t, std::string> validations;
    std::vector<std::pair<std::uint64_t, BaselineEntry>> compiled;
    std::vector<TimedEntry> timings;
    std::vector<std::string> changes;
    std::string runId;
    std::size_t requests = 0;
    std::size_t failures = 0;
    std::size_t rejected = 0;
    std::size_t knownRejections = 0;
    std::size_t duplicates = 0;
    std::size_t unchanged = 0;
    double milliseconds = 0.0;
};

struct TemporaryFile {
    std::filesystem::path path;

    ~TemporaryFile() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

class Hasher {
public:
    void Add(std::uint64_t value) {
        hash = (hash ^ value) * 0xff51afd7ed558ccdull;
        hash ^= hash >> 32u;
    }

    template <typename TRange>
    void AddAll(const TRange& values) {
        Add(values.size());
        for (const auto value : values) {
            Add(static_cast<std::uint64_t>(value));
        }
    }

    std::uint64_t Value() const {
        return hash;
    }

private:
    std::uint64_t hash = 0x9e3779b97f4a7c15ull;
};

std::uint64_t CodeHash(std::span<const std::uint32_t> code) {
    std::uint64_t hash = 0x9e3779b97f4a7c15ull ^ code.size();
    for (const auto word : code) {
        hash = (hash ^ word) * 0xff51afd7ed558ccdull;
        hash ^= hash >> 32u;
    }
    return hash != 0 ? hash : 1;
}

std::uint64_t EntryKey(std::string_view payload) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const char c : payload) {
        hash = (hash ^ static_cast<unsigned char>(c)) * 0x100000001b3ull;
    }
    return hash;
}

std::uint64_t HashMetadata(const RecompileResult& result) {
    const auto& [spirv, spirvHash, bindings, pushConstants, bdaAbiVersion, vertexAttributes, vertexOffsetSgpr, instanceOffsetSgpr, parameterExports, fragmentParameters, unresolvedImages, cacheHit] = result;
    Hasher hash;
    hash.Add(bindings.size());
    for (const auto& [kind, role, descriptorSet, binding, count, guestDescriptor, readOnly, elementWritten, elementOptional, elementRead, imageShape, samplerDepthCompare, imageDepthCompare] : bindings) {
        hash.Add(static_cast<std::uint64_t>(kind));
        hash.Add(static_cast<std::uint64_t>(role));
        hash.Add(descriptorSet);
        hash.Add(binding);
        hash.Add(count);
        hash.AddAll(guestDescriptor);
        hash.Add(readOnly);
        hash.AddAll(elementWritten);
        hash.AddAll(elementOptional);
        hash.AddAll(elementRead);
        hash.Add(imageShape.has_value() ? static_cast<std::uint64_t>(*imageShape) + 1u : 0u);
        hash.AddAll(samplerDepthCompare);
        hash.Add(imageDepthCompare);
    }
    hash.AddAll(pushConstants);
    hash.Add(bdaAbiVersion);
    hash.Add(vertexAttributes.size());
    for (const auto& [location, components, resource, fetchIndex] : vertexAttributes) {
        const auto& [fields] = resource;
        hash.Add(location);
        hash.Add(components);
        hash.AddAll(fields);
        hash.Add(fetchIndex);
    }
    hash.Add(static_cast<std::uint32_t>(vertexOffsetSgpr));
    hash.Add(static_cast<std::uint32_t>(instanceOffsetSgpr));
    hash.AddAll(parameterExports);
    hash.Add(fragmentParameters.size());
    for (const auto& [location, sourceLocation, flat, perVertex] : fragmentParameters) {
        hash.Add(location);
        hash.Add(sourceLocation);
        hash.Add(flat);
        hash.Add(perVertex);
    }
    hash.Add(unresolvedImages);
    return hash.Value();
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

const char* StageName(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Compute:
        return "compute";
    case ShaderStage::Vertex:
        return "vertex";
    case ShaderStage::TessellationControl:
        return "hull";
    case ShaderStage::TessellationEvaluation:
        return "domain";
    case ShaderStage::Geometry:
        return "geometry";
    case ShaderStage::Fragment:
        return "pixel";
    case ShaderStage::Local:
        return "local";
    case ShaderStage::Mesh:
        return "mesh";
    }
    return "unknown";
}

bool IsPayload(std::string_view line) {
    if (line.size() < 16u || line.size() % 4u != 0u) {
        return false;
    }
    for (const char c : line) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=')) {
            return false;
        }
    }
    return true;
}

std::string FailureReason(std::string_view message) {
    return std::string(message.substr(0, message.find("\nRecompileRequest:")));
}

void TrimLineEnd(std::string& line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
        line.pop_back();
    }
}

double MillisecondsSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

void WriteFile(const std::string& path, const void* data, std::size_t size) {
    std::ofstream file(path, std::ios::binary);
    file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    if (!file) {
        throw std::runtime_error("failed to write " + path);
    }
}

std::vector<std::string> SplitPathList(std::string_view list) {
    std::vector<std::string> paths;
    while (!list.empty()) {
        const auto end = list.find(PathListSeparator);
        if (end != 0u) {
            paths.emplace_back(list.substr(0, end));
        }
        list.remove_prefix(end == std::string_view::npos ? list.size() : end + 1u);
    }
    return paths;
}

std::string FindValidator() {
    const char* path = std::getenv("PATH");
    for (const auto& directory : SplitPathList(path != nullptr ? path : "")) {
        const auto candidate = std::filesystem::path(directory) / ValidatorName;
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate.string();
        }
    }
    return {};
}

const char* ValidatorEnvironment(const SpirvTarget& target) {
    switch (target.vulkanVersion & ~0xfffu) {
    case 0x00400000u:
        return "vulkan1.0";
    case 0x00401000u:
        return target.spirvVersion == 0x00010400u ? "vulkan1.1spv1.4" : "vulkan1.1";
    case 0x00402000u:
        return "vulkan1.2";
    case 0x00403000u:
        return "vulkan1.3";
    case 0x00404000u:
        return "vulkan1.4";
    }
    return nullptr;
}

#if defined(_WIN32)
FILE* OpenPipe(const std::string& command) {
    return _popen(("\"" + command + "\"").c_str(), "r");
}

int ClosePipe(FILE* pipe) {
    return _pclose(pipe);
}
#else
FILE* OpenPipe(const std::string& command) {
    return popen(command.c_str(), "r");
}

int ClosePipe(FILE* pipe) {
    return pclose(pipe);
}
#endif

std::string RunValidator(const std::string& validator, const char* environment, const std::filesystem::path& module) {
    FILE* pipe = OpenPipe("\"" + validator + "\" --target-env " + environment + " \"" + module.string() + "\" 2>&1");
    if (pipe == nullptr) {
        throw std::runtime_error("failed to run " + validator);
    }
    std::string output;
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    const int status = ClosePipe(pipe);
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    if (status == 0) {
        return {};
    }
    return output.empty() ? "exit status " + std::to_string(status) : output;
}

std::string Validate(ReplayState& state, const RecompileRequest& request, const RecompileResult& result) {
    if (state.options.validator.empty()) {
        return {};
    }
    if (const auto found = state.validations.find(result.spirvHash); found != state.validations.end()) {
        return found->second;
    }
    std::string failure;
    if (const char* environment = ValidatorEnvironment(request.target); environment == nullptr) {
        failure = "spirv-val has no target environment for Vulkan version 0x" + Hex(request.target.vulkanVersion);
    } else {
        const TemporaryFile module{std::filesystem::temp_directory_path() / ("recompile_replay_" + state.runId + "_" + Hex(result.spirvHash) + ".spv")};
        WriteFile(module.path.string(), result.spirv.data(), result.spirv.size() * sizeof(std::uint32_t));
        failure = RunValidator(state.options.validator, environment, module.path);
    }
    state.validations.emplace(result.spirvHash, failure);
    return failure;
}

void Compare(ReplayState& state, std::uint64_t key, const BaselineEntry& entry, const std::string& label) {
    const auto found = state.baseline.find(key);
    if (found == state.baseline.end()) {
        return;
    }
    const auto& before = found->second;
    if (before.spirvHash == entry.spirvHash && before.metadataHash == entry.metadataHash) {
        ++state.unchanged;
        return;
    }
    std::string change = label + ":";
    if (before.spirvHash != entry.spirvHash) {
        change += " SPIR-V hash 0x" + Hex(before.spirvHash) + " (" + std::to_string(before.spirvWords) + " words) -> 0x" + Hex(entry.spirvHash) + " (" + std::to_string(entry.spirvWords) + " words)";
    }
    if (before.metadataHash != entry.metadataHash) {
        change += " metadata hash 0x" + Hex(before.metadataHash) + " -> 0x" + Hex(entry.metadataHash);
    }
    state.changes.push_back(std::move(change));
}

void Replay(std::string_view payload, ReplayState& state) {
    const auto key = EntryKey(payload);
    if (!state.replayed.insert(key).second) {
        ++state.duplicates;
        return;
    }
    const auto index = ++state.requests;
    std::string label = "[" + std::to_string(index) + "] key " + Hex(key);
    std::printf("%s ", label.c_str());
    auto start = std::chrono::steady_clock::now();
    bool compiling = false;
    try {
        auto deserialized = RequestSerializer{}.Deserialize(payload);
        auto& request = deserialized.request;
        request.shader.codeHash = CodeHash(request.shader.code);
        char program[96];
        std::snprintf(program, sizeof(program), "%s program 0x%llx hash 0x%016llx", StageName(request.shader.stage), static_cast<unsigned long long>(request.shader.codeAddress), static_cast<unsigned long long>(request.shader.codeHash));
        label += ' ';
        label += program;
        std::printf("%s, %zu code words, %zu user data, %zu memory regions: ", program, request.shader.code.size(), request.context.userData.size(), request.context.memory.size());
        std::fflush(stdout);
        char name[48];
        std::snprintf(name, sizeof(name), "%zu_%llx", index, static_cast<unsigned long long>(request.shader.codeAddress));
        if (!state.options.disassemblyDirectory.empty()) {
            const auto text = RdnaProgramToString(RdnaInstructionDecoder{}.Decode(request.shader.code));
            WriteFile(state.options.disassemblyDirectory + "/" + name + ".rdna.txt", text.data(), text.size());
        }
        compiling = true;
        start = std::chrono::steady_clock::now();
        const auto result = Recompile(request);
        const double milliseconds = MillisecondsSince(start);
        state.milliseconds += milliseconds;
        compiling = false;
        const BaselineEntry entry{result.spirvHash, HashMetadata(result), result.spirv.size(), program};
        if (!state.options.spirvDirectory.empty()) {
            WriteFile(state.options.spirvDirectory + "/" + name + ".spv", result.spirv.data(), result.spirv.size() * sizeof(std::uint32_t));
        }
        const auto rejection = Validate(state, request, result);
        std::printf("ok, %zu SPIR-V words, SPIR-V hash 0x%016llx, metadata hash 0x%016llx, %zu bindings, %.1f ms%s\n", result.spirv.size(), static_cast<unsigned long long>(entry.spirvHash), static_cast<unsigned long long>(entry.metadataHash), result.bindings.size(), milliseconds, result.cacheHit ? ", cache hit" : "");
        Compare(state, key, entry, label);
        if (!rejection.empty()) {
            const bool known = state.baselineModules.contains(entry.spirvHash);
            ++state.rejected;
            state.knownRejections += known ? 1u : 0u;
            std::printf("%s: spirv-val rejected the SPIR-V%s:\n%s\n", label.c_str(), known ? ", which the baseline already has" : "", rejection.c_str());
        }
        state.timings.push_back({milliseconds, label});
        state.compiled.emplace_back(key, entry);
    } catch (const std::exception& error) {
        const double milliseconds = compiling ? MillisecondsSince(start) : 0.0;
        state.milliseconds += milliseconds;
        state.failures++;
        std::printf("FAILED after %.1f ms: %s\n", milliseconds, FailureReason(error.what()).c_str());
    }
    std::fflush(stdout);
}

void ReplayFile(const std::string& path, ReplayState& state) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("failed to open " + path);
    }
    std::vector<std::string> lines;
    for (std::string line; std::getline(file, line);) {
        TrimLineEnd(line);
        lines.push_back(std::move(line));
    }
    const bool cache = !lines.empty() && lines.front() == CacheHeader;
    const bool marked = std::any_of(lines.begin(), lines.end(), [](const std::string& line) { return line.starts_with(RequestMarker); });
    for (std::size_t i = cache ? 1u : 0u; i < lines.size(); ++i) {
        const bool payload = cache ? !lines[i].empty() : marked ? i != 0u && lines[i - 1u].starts_with(RequestMarker) : IsPayload(lines[i]);
        if (payload) {
            Replay(lines[i], state);
        }
    }
}

std::map<std::uint64_t, BaselineEntry> ReadBaseline(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("failed to open " + path);
    }
    std::string line;
    std::getline(file, line);
    TrimLineEnd(line);
    if (line != BaselineHeader) {
        throw std::runtime_error(path + " is not a recompile baseline");
    }
    std::map<std::uint64_t, BaselineEntry> baseline;
    while (std::getline(file, line)) {
        TrimLineEnd(line);
        if (line.empty()) {
            continue;
        }
        std::istringstream fields(line);
        std::uint64_t key = 0;
        BaselineEntry entry;
        fields >> std::hex >> key >> entry.spirvHash >> entry.metadataHash >> std::dec >> entry.spirvWords;
        if (!fields) {
            throw std::runtime_error(path + " has a malformed line: " + line);
        }
        baseline.insert_or_assign(key, std::move(entry));
    }
    return baseline;
}

void WriteBaseline(const std::string& path, const std::vector<std::pair<std::uint64_t, BaselineEntry>>& entries) {
    std::ofstream file(path, std::ios::binary);
    file << BaselineHeader << '\n';
    for (const auto& [key, entry] : entries) {
        file << Hex(key) << ' ' << Hex(entry.spirvHash) << ' ' << Hex(entry.metadataHash) << ' ' << entry.spirvWords << ' ' << entry.program << '\n';
    }
    if (!file) {
        throw std::runtime_error("failed to write " + path);
    }
}

int Report(ReplayState& state, double totalMilliseconds) {
    const auto& options = state.options;
    if (!state.changes.empty()) {
        std::printf("%zu request(s) changed from the baseline:\n", state.changes.size());
        for (const auto& change : state.changes) {
            std::printf("  %s\n", change.c_str());
        }
    }
    std::sort(state.timings.begin(), state.timings.end(), [](const TimedEntry& left, const TimedEntry& right) { return left.milliseconds > right.milliseconds; });
    std::size_t overBudget = 0;
    while (options.budgetMilliseconds > 0.0 && overBudget < state.timings.size() && state.timings[overBudget].milliseconds > options.budgetMilliseconds) {
        ++overBudget;
    }
    if (overBudget != 0) {
        std::printf("%zu request(s) took longer than the %.1f ms budget:\n", overBudget, options.budgetMilliseconds);
        for (std::size_t i = 0; i < overBudget; ++i) {
            std::printf("  %s: %.1f ms\n", state.timings[i].label.c_str(), state.timings[i].milliseconds);
        }
    }
    const auto slowest = std::min(SlowestReported, state.timings.size());
    if (slowest != 0) {
        std::printf("slowest compiles:\n");
        for (std::size_t i = 0; i < slowest; ++i) {
            std::printf("  %s: %.1f ms\n", state.timings[i].label.c_str(), state.timings[i].milliseconds);
        }
    }
#if ANYPS5_ENABLE_SPIRV_TOOLS
    std::printf("Recompile validated and optimized every newly compiled module with SPIRV-Tools\n");
#endif
    if (options.validator.empty()) {
        std::printf("SPIR-V validation skipped: spirv-val was not found on PATH and --spirv-val was not given\n");
    } else {
        std::printf("SPIR-V validation: %s checked %zu distinct module(s); it rejected %zu request(s), %zu of them with SPIR-V the baseline already has\n", options.validator.c_str(), state.validations.size(), state.rejected, state.knownRejections);
    }
    bool baselineUnrelated = false;
    if (!options.baselinePath.empty()) {
        const auto shared = static_cast<std::size_t>(std::count_if(state.replayed.begin(), state.replayed.end(), [&state](std::uint64_t key) { return state.baseline.contains(key); }));
        std::printf("baseline %s: %zu unchanged, %zu changed, %zu not in the baseline, %zu baseline entries not replayed\n", options.baselinePath.c_str(), state.unchanged, state.changes.size(), state.replayed.size() - shared, state.baseline.size() - shared);
        baselineUnrelated = shared == 0;
        if (baselineUnrelated) {
            std::printf("the baseline has none of the replayed requests\n");
        }
    }
    if (!options.writeBaselinePath.empty()) {
        WriteBaseline(options.writeBaselinePath, state.compiled);
        std::printf("wrote the hashes of %zu request(s) to %s\n", state.compiled.size(), options.writeBaselinePath.c_str());
    }
    const auto newRejections = state.rejected - state.knownRejections;
    std::printf("%zu requests: %zu compiled, %zu failed, %zu newly rejected by spirv-val, %zu changed, %zu over budget, %zu duplicates skipped; %.1f ms compiling, %.1f s in total\n", state.requests, state.requests - state.failures, state.failures, newRejections, state.changes.size(), overBudget, state.duplicates, state.milliseconds, totalMilliseconds / 1000.0);
    const bool passed = state.failures == 0 && newRejections == 0 && state.changes.empty() && overBudget == 0 && !baselineUnrelated;
    return passed ? 0 : 2;
}

void PrintUsage() {
    std::fprintf(stderr,
        "usage: recompile_replay [--baseline <file>] [--write-baseline <file>] [--budget <ms>] [--spirv-val <executable>]\n"
        "                        [--disasm <directory>] [--spirv <directory>] [<file>...]\n"
        "Runs ShaderRecompiler::Recompile on every serialized RecompileRequest in the files: shader-requests.cache files,\n"
        "driver logs whose RecompileRequest: lines are followed by payloads, or files holding one payload per line.\n"
        "Without file arguments the files come from %s, a list of paths separated by '%c'.\n"
        "Each request is reported with its key (a hash of the payload) and either its failure or its SPIR-V word count,\n"
        "SPIR-V hash, metadata hash (bindings, push constants and stage interface) and compile time.\n"
        "Check mode (the default) requires every request to compile. --baseline compares the hashes with a baseline file\n"
        "and lists the requests whose hashes changed; --budget lists the requests that took longer than <ms> to compile.\n"
        "Baseline mode (--write-baseline) writes the key and hashes of every compiled request to the file.\n"
        "The SPIR-V is validated with --spirv-val, or with the spirv-val found on PATH; without one validation is skipped.\n"
        "A module spirv-val rejects fails the check unless its SPIR-V hash is already in the baseline.\n"
        "--disasm writes the RDNA disassembly and --spirv the SPIR-V of each request into the directory.\n"
        "The exit code is 0 when every request compiles, no module is newly rejected, no hash changed, no compile\n"
        "exceeded the budget and a given baseline shares a request with the files, 2 otherwise and 1 on usage or input errors.\n"
        "Regression check: recompile_replay --write-baseline baseline.txt build/game/shader-requests.cache before a change,\n"
        "then recompile_replay --baseline baseline.txt --budget 500 build/game/shader-requests.cache after it.\n",
        InputsVariable, PathListSeparator);
}

}

int main(int argc, char** argv) {
    ReplayState state;
    auto& options = state.options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        const bool hasValue = i + 1 < argc;
        if (argument == "--baseline" && hasValue) {
            options.baselinePath = argv[++i];
        } else if (argument == "--write-baseline" && hasValue) {
            options.writeBaselinePath = argv[++i];
        } else if (argument == "--spirv-val" && hasValue) {
            options.validator = argv[++i];
        } else if ((argument == "--disasm" || argument == "--spirv") && hasValue) {
            (argument == "--disasm" ? options.disassemblyDirectory : options.spirvDirectory) = argv[++i];
        } else if (argument == "--budget" && hasValue) {
            char* end = nullptr;
            options.budgetMilliseconds = std::strtod(argv[++i], &end);
            if (*end != '\0' || !(options.budgetMilliseconds > 0.0)) {
                PrintUsage();
                return 1;
            }
        } else if (argument.starts_with("--")) {
            PrintUsage();
            return 1;
        } else {
            options.inputs.emplace_back(argument);
        }
    }
    if (options.inputs.empty()) {
        const char* inputs = std::getenv(InputsVariable);
        options.inputs = SplitPathList(inputs != nullptr ? inputs : "");
    }
    if (options.inputs.empty()) {
        PrintUsage();
        return 1;
    }

    try {
        std::error_code error;
        if (!options.validator.empty() && !std::filesystem::is_regular_file(options.validator, error)) {
            throw std::runtime_error("spirv-val not found: " + options.validator);
        }
        if (options.validator.empty()) {
            options.validator = FindValidator();
        }
        if (!options.baselinePath.empty()) {
            state.baseline = ReadBaseline(options.baselinePath);
            for (const auto& [key, entry] : state.baseline) {
                state.baselineModules.insert(entry.spirvHash);
            }
        }
        const auto start = std::chrono::steady_clock::now();
        state.runId = Hex(static_cast<std::uint64_t>(start.time_since_epoch().count()));
        for (const auto& input : options.inputs) {
            ReplayFile(input, state);
        }
        if (state.requests == 0) {
            throw std::runtime_error("no serialized recompile requests found");
        }
        return Report(state, MillisecondsSince(start));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

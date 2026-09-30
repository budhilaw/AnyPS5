#include "Recompiler.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct ReplayOptions {
    std::vector<std::string> inputs;
    std::string disassemblyDirectory;
    std::string spirvDirectory;
};

struct ReplaySummary {
    std::size_t requests = 0;
    std::size_t failures = 0;
    double milliseconds = 0.0;
};

std::uint64_t CodeHash(std::span<const std::uint32_t> code) {
    std::uint64_t hash = 0x9e3779b97f4a7c15ull ^ code.size();
    for (const auto word : code) {
        hash = (hash ^ word) * 0xff51afd7ed558ccdull;
        hash ^= hash >> 32u;
    }
    return hash != 0 ? hash : 1;
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

void Replay(std::string_view payload, const ReplayOptions& options, ReplaySummary& summary) {
    const auto index = ++summary.requests;
    std::printf("[%zu] ", index);
    auto start = std::chrono::steady_clock::now();
    bool compiling = false;
    try {
        auto deserialized = RequestSerializer{}.Deserialize(payload);
        auto& request = deserialized.request;
        request.shader.codeHash = CodeHash(request.shader.code);
        std::printf("%s program 0x%llx hash 0x%016llx, %zu code words, %zu user data, %zu memory regions: ", StageName(request.shader.stage), static_cast<unsigned long long>(request.shader.codeAddress), static_cast<unsigned long long>(request.shader.codeHash), request.shader.code.size(), request.context.userData.size(), request.context.memory.size());
        std::fflush(stdout);
        char name[48];
        std::snprintf(name, sizeof(name), "%zu_%llx", index, static_cast<unsigned long long>(request.shader.codeAddress));
        if (!options.disassemblyDirectory.empty()) {
            const auto text = RdnaProgramToString(RdnaInstructionDecoder{}.Decode(request.shader.code));
            WriteFile(options.disassemblyDirectory + "/" + name + ".rdna.txt", text.data(), text.size());
        }
        compiling = true;
        start = std::chrono::steady_clock::now();
        const auto result = Recompile(request);
        const double milliseconds = MillisecondsSince(start);
        summary.milliseconds += milliseconds;
        compiling = false;
        std::printf("ok, %zu SPIR-V words, SPIR-V hash 0x%016llx, %zu bindings, %.1f ms\n", result.spirv.size(), static_cast<unsigned long long>(result.spirvHash), result.bindings.size(), milliseconds);
        if (!options.spirvDirectory.empty()) {
            WriteFile(options.spirvDirectory + "/" + name + ".spv", result.spirv.data(), result.spirv.size() * sizeof(std::uint32_t));
        }
    } catch (const std::exception& error) {
        const double milliseconds = compiling ? MillisecondsSince(start) : 0.0;
        summary.milliseconds += milliseconds;
        summary.failures++;
        std::printf("FAILED after %.1f ms: %s\n", milliseconds, FailureReason(error.what()).c_str());
    }
    std::fflush(stdout);
}

void PrintUsage() {
    std::fprintf(stderr,
        "usage: recompile_replay [--disasm <directory>] [--spirv <directory>] <file>...\n"
        "Runs ShaderRecompiler::Recompile on every serialized RecompileRequest in the files: a file holding one payload,\n"
        "a driver log whose RecompileRequest: lines are followed by payloads, or a shader-requests.cache.\n"
        "--disasm writes the RDNA disassembly and --spirv the SPIR-V of each request into the directory.\n"
        "The exit code is 0 when every request compiles, 2 when any fails and 1 on usage or input errors.\n");
}

}

int main(int argc, char** argv) {
    ReplayOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if ((argument == "--disasm" || argument == "--spirv") && i + 1 < argc) {
            (argument == "--disasm" ? options.disassemblyDirectory : options.spirvDirectory) = argv[++i];
        } else if (argument.starts_with("--")) {
            PrintUsage();
            return 1;
        } else {
            options.inputs.emplace_back(argument);
        }
    }
    if (options.inputs.empty()) {
        PrintUsage();
        return 1;
    }

    ReplaySummary summary;
    for (const auto& input : options.inputs) {
        std::ifstream file(input, std::ios::binary);
        if (!file) {
            std::fprintf(stderr, "failed to open %s\n", input.c_str());
            return 1;
        }
        std::string line;
        while (std::getline(file, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
                line.pop_back();
            }
            if (IsPayload(line)) {
                Replay(line, options, summary);
            }
        }
    }
    if (summary.requests == 0) {
        std::fprintf(stderr, "no serialized recompile requests found\n");
        return 1;
    }
    std::printf("%zu requests: %zu compiled, %zu failed, %.1f ms compiling\n", summary.requests, summary.requests - summary.failures, summary.failures, summary.milliseconds);
    return summary.failures == 0 ? 0 : 2;
}

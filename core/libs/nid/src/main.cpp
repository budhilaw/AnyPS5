#include <nid/BinaryPatcherFactory.hpp>
#include <nid/ExportExclusions.hpp>
#include <nid/NidResolver.hpp>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> _readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

void _writeFile(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

}

std::vector<std::string> _readNames(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open: " + path);
    std::vector<std::string> names;
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        // Mach-O user symbols carry a leading underscore that is not part of the exported name.
        if (line.front() == '_') line.erase(0, 1);
        names.push_back(line);
    }
    return names;
}

// Writes an ld64 -alias_list file mapping each Mach-O export to its NID name, so the library
// exports both. Mach-O binaries are not patched in place (see docs/TechnicalDebt.md).
int _writeAliasList(const std::string& libraryName, const std::string& namesPath, const std::string& outputPath, const std::unordered_set<std::string>& excludedExports) {
    const auto names = _readNames(namesPath);
    const auto resolved = Nid::ResolveNids(names, libraryName, excludedExports);
    std::ofstream out(outputPath, std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write: " + outputPath);
    std::size_t aliases = 0;
    for (const auto& name : names) {
        const auto& target = resolved.at(name);
        if (target == name) continue;
        out << '_' << name << " _" << target << '\n';
        ++aliases;
    }
    if (!out) throw std::runtime_error("cannot write: " + outputPath);
    std::cout << "OK: " << outputPath << " (" << aliases << " NID aliases)\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: nid_patcher <library_name> [--preserve-exports <unpatched-library>] <file> [file2 ...]\n"
                     "       nid_patcher <library_name> --alias-list <exported-names.txt> <aliases.txt> [--preserve-export-names <names.txt>]\n";
        return 1;
    }

    const std::string libraryName = argv[1];
    if (std::string(argv[2]) == "--alias-list") {
        try {
            if (argc != 5 && argc != 7) throw std::runtime_error("--alias-list takes <exported-names.txt> <aliases.txt> [--preserve-export-names <names.txt>]");
            std::unordered_set<std::string> excluded;
            if (argc == 7) {
                if (std::string(argv[5]) != "--preserve-export-names") throw std::runtime_error("unknown option: " + std::string(argv[5]));
                for (const auto& name : _readNames(argv[6])) excluded.insert(Nid::NormalizeExportName(name));
            }
            return _writeAliasList(libraryName, argv[3], argv[4], excluded);
        } catch (const std::exception& e) {
            std::cerr << "FAIL: " << e.what() << '\n';
            return 2;
        }
    }
    std::unordered_set<std::string> excludedExports;
    std::vector<std::string> paths;
    try {
        bool hasReference = false;
        for (int i = 2; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--preserve-exports") {
                if (hasReference || i + 1 == argc) throw std::runtime_error("--preserve-exports requires exactly one reference library");
                hasReference = true;
                excludedExports = Nid::ReadExportExclusions(argv[++i]);
            } else {
                if (argument.starts_with("--")) throw std::runtime_error("unknown option: " + argument);
                paths.push_back(argument);
            }
        }
        if (paths.empty()) throw std::runtime_error("no files to patch");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 2;
    }

    for (const std::string& path : paths) {
        try {
            auto binary = _readFile(path);
            const auto patcher = Nid::MakePatcher(binary);
            patcher->PatchNids(binary, libraryName, excludedExports);
            _writeFile(path, binary);
            std::cout << "OK: " << path << "\n";
        } catch (const std::exception& e) {
            std::cerr << "FAIL: " << path << ": " << e.what() << "\n";
            return 2;
        }
    }

    return 0;
}

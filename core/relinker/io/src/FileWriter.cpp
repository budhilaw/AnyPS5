#include <io/FileWriter.hpp>
#include <domain/Types.hpp>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace Io {

void FileWriter::Write(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f)
        throw Domain::RelinkerException("Cannot open output file: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f)
        throw Domain::RelinkerException("Failed to write file: " + path);
    f.close();
#if !defined(_WIN32)
    std::error_code error;
    std::filesystem::permissions(path, std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec, std::filesystem::perm_options::add, error);
    if (error)
        throw Domain::RelinkerException("Cannot mark the output file executable: " + path);
#endif
}

void FileWriter::Write(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f)
        throw Domain::RelinkerException("Cannot open output file: " + path);
    f << content;
    if (!f)
        throw Domain::RelinkerException("Failed to write file: " + path);
}

}

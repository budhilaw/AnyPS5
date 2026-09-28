#include <string_view>
#include "prx/libc/include/specifics/darwin/GuestImage.hpp"
#include <cstring>
#include <stdexcept>
#include <string>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>

namespace GuestImage {
namespace {

constexpr const char* Segment = "__ANYPS5";

const mach_header_64* mainHeader() {
    const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(0));
    if (header == nullptr || header->magic != MH_MAGIC_64) throw std::runtime_error("guest image: main executable header is unavailable");
    return header;
}

std::uintptr_t slide() { return static_cast<std::uintptr_t>(_dyld_get_image_vmaddr_slide(0)); }

// Returns the section contents or nullptr when the section is absent.
const std::uint64_t* fields(const char* section, std::size_t count) {
    unsigned long size = 0;
    const auto* data = getsectiondata(mainHeader(), Segment, section, &size);
    if (data == nullptr) return nullptr;
    if (size != count * sizeof(std::uint64_t)) throw std::runtime_error(std::string("guest image: malformed ") + section + " metadata");
    return reinterpret_cast<const std::uint64_t*>(data);
}

// Checks that [address, address + size) lies inside one mapped segment of the main image.
void requireMapped(std::uintptr_t address, std::size_t size, const char* what) {
    const auto* header = mainHeader();
    const auto* command = reinterpret_cast<const load_command*>(header + 1);
    for (std::uint32_t index = 0; index < header->ncmds; ++index) {
        if (command->cmd == LC_SEGMENT_64) {
            const auto* segment = reinterpret_cast<const segment_command_64*>(command);
            const auto first = static_cast<std::uintptr_t>(segment->vmaddr) + slide();
            if (segment->vmsize != 0 && address >= first && size <= segment->vmsize - (address - first)) return;
        }
        command = reinterpret_cast<const load_command*>(reinterpret_cast<const char*>(command) + command->cmdsize);
    }
    throw std::runtime_error(std::string("guest image: ") + what + " lies outside the executable");
}

}

void ForEachModuleThreadStorage(void (*visit)(const ModuleThreadStorage&, void*), void* context) {
    const auto count = _dyld_image_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(index));
        if (header == nullptr || header->magic != MH_MAGIC_64) continue;
        unsigned long size = 0;
        const auto* data = getsectiondata(header, Segment, "__modtls", &size);
        if (data == nullptr) continue;
        if (size != 5 * sizeof(std::uint64_t)) throw std::runtime_error("guest image: malformed __modtls metadata");
        const auto* values = reinterpret_cast<const std::uint64_t*>(data);
        const auto imageSlide = static_cast<std::uintptr_t>(_dyld_get_image_vmaddr_slide(index));
        ModuleThreadStorage storage{values[4], nullptr, static_cast<std::size_t>(values[1]), static_cast<std::size_t>(values[2]), static_cast<std::size_t>(values[3])};
        if (storage.fileSize > storage.memorySize || storage.alignment == 0 || (storage.alignment & (storage.alignment - 1)) != 0 || storage.module == 0) throw std::runtime_error("guest image: invalid module thread storage metadata");
        if (storage.fileSize != 0) storage.image = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(values[0]) + imageSlide);
        visit(storage, context);
    }
}

bool IsGuestExecutable() {
    unsigned long size = 0;
    return getsegmentdata(mainHeader(), Segment, &size) != nullptr;
}

std::optional<ProcessParameters> FindProcessParameters() {
    const auto* values = fields("__procparam", 2);
    if (values == nullptr) return std::nullopt;
    const auto address = static_cast<std::uintptr_t>(values[0]) + slide();
    const auto size = static_cast<std::size_t>(values[1]);
    if (size < 0x40) throw std::runtime_error("guest image: process parameters are too small");
    requireMapped(address, size, "process parameters");
    return ProcessParameters{reinterpret_cast<const void*>(address), size};
}

std::optional<ExceptionHeader> FindExceptionHeader() {
    const auto* values = fields("__ehframehdr", 1);
    if (values == nullptr) return std::nullopt;
    const auto address = static_cast<std::uintptr_t>(values[0]) + slide();
    requireMapped(address, 4, "exception frame header");
    return ExceptionHeader{reinterpret_cast<const unsigned char*>(address)};
}

std::optional<ThreadStorage> FindThreadStorage() {
    const auto* values = fields("__tls", 5);
    if (values == nullptr) return std::nullopt;
    ThreadStorage storage{nullptr, static_cast<std::size_t>(values[1]), static_cast<std::size_t>(values[2]), static_cast<std::size_t>(values[3]), static_cast<std::uint32_t>(values[4])};
    if (storage.fileSize > storage.memorySize || storage.alignment == 0 || (storage.alignment & (storage.alignment - 1)) != 0) throw std::runtime_error("guest image: invalid thread storage metadata");
    if (storage.fileSize != 0) {
        const auto address = static_cast<std::uintptr_t>(values[0]) + slide();
        requireMapped(address, storage.fileSize, "thread storage image");
        storage.image = reinterpret_cast<const void*>(address);
    }
    return storage;
}


void* FindModuleInit(const char* path) {
    if (path == nullptr) return nullptr;
    const std::string_view wanted(path);
    const auto wantedName = wanted.substr(wanted.find_last_of('/') + 1);
    const auto count = _dyld_image_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        const char* name = _dyld_get_image_name(index);
        if (name == nullptr) continue;
        const std::string_view loaded(name);
        if (loaded.substr(loaded.find_last_of('/') + 1) != wantedName) continue;
        const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(index));
        unsigned long size = 0;
        const auto* data = getsectiondata(header, Segment, "__init", &size);
        if (data == nullptr) return nullptr;
        if (size != sizeof(std::uint64_t)) throw std::runtime_error("guest image: malformed __init metadata");
        // The slot is a dyld rebase, so it already carries the image slide.
        std::uint64_t value = 0;
        std::memcpy(&value, data, sizeof(value));
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));
    }
    return nullptr;
}

}


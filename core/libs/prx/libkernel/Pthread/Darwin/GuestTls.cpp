#include "../include/GuestTls.hpp"
#include "prx/libc/include/specifics/darwin/GuestImage.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <pthread.h>

namespace GuestTls {
namespace {

// Slots below this belong to the system and libSystem; dynamic keys grow upward from there.
constexpr std::uint32_t FirstDynamicSlot = 256;
constexpr std::uint32_t SlotCount = 768;

struct Layout {
    GuestImage::ThreadStorage storage;
    std::size_t blockSize;   // static TLS rounded up to its alignment; the TCB follows it
    pthread_key_t cleanup;   // dynamic key whose destructor frees the block at thread exit
};

std::optional<Layout> layout;
std::once_flag once;

void release(void* block) { std::free(block); }

void resolve() {
    const auto storage = GuestImage::FindThreadStorage();
    if (!storage) return;
    if (storage->slot < FirstDynamicSlot || storage->slot >= SlotCount) throw std::runtime_error("guest TLS: metadata names an invalid TSD slot");
    Layout result{*storage, (storage->memorySize + storage->alignment - 1) & ~(storage->alignment - 1), 0};
    if (pthread_key_create(&result.cleanup, release) != 0) throw std::runtime_error("guest TLS: cannot create the cleanup key");
    layout = result;
}

}

void InstallCurrentThread() {
    std::call_once(once, resolve);
    if (!layout) return;
    const auto alignment = std::max<std::size_t>(layout->storage.alignment, 16);
    const auto total = layout->blockSize + 16; // TCB: self pointer and (unused) dynamic thread vector
    void* block = nullptr;
    if (posix_memalign(&block, alignment, total) != 0 || block == nullptr) throw std::runtime_error("guest TLS: cannot allocate the thread block");
    std::memset(block, 0, total);
    if (layout->storage.fileSize != 0) std::memcpy(static_cast<char*>(block) + (layout->blockSize - ((layout->storage.memorySize + layout->storage.alignment - 1) & ~(layout->storage.alignment - 1))), layout->storage.image, layout->storage.fileSize);
    auto* tcb = reinterpret_cast<void**>(static_cast<char*>(block) + layout->blockSize);
    tcb[0] = tcb;
    tcb[1] = nullptr;
    if (pthread_setspecific(layout->cleanup, block) != 0) throw std::runtime_error("guest TLS: cannot register the thread block");
    // Guest code was relinked to read %gs:slot*8 where it used to read %fs:0.
    const auto offset = static_cast<std::uint64_t>(layout->storage.slot) * sizeof(void*);
#if defined(__x86_64__)
    __asm__ volatile("movq %0, %%gs:(%1)" : : "r"(tcb), "r"(offset) : "memory");
#else
    (void)offset; // guest x86-64 code cannot run on this host; the block only keeps the layout valid
#endif
}

}

// The main thread runs guest code from the entry stub before any libkernel call.
__attribute__((constructor)) static void InstallMainThread() { GuestTls::InstallCurrentThread(); }

// Installs guest thread storage on a host thread libkernel did not create (the guest main
// thread that libc starts on macOS).
extern "C" void KernelInstallGuestThread_nid_no_patch() { GuestTls::InstallCurrentThread(); }

// Dynamic thread storage of guest modules: __tls_get_addr({module, offset}) returns the calling
// thread's block for that module, created from the module's template on first use.
namespace {

struct ModuleStorage { GuestImage::ModuleThreadStorage storage; };
std::mutex modulesMutex;
std::unordered_map<std::uint64_t, ModuleStorage> modules;

void registerModules() {
    GuestImage::ForEachModuleThreadStorage([](const GuestImage::ModuleThreadStorage& storage, void*) { modules.emplace(storage.module, ModuleStorage{storage}); }, nullptr);
}

struct ThreadBlocks {
    std::unordered_map<std::uint64_t, void*> blocks;
    ~ThreadBlocks() { for (auto& [module, block] : blocks) std::free(block); }
};
thread_local ThreadBlocks threadBlocks;

struct TlsIndex { std::uint64_t module; std::uint64_t offset; };

}

extern "C" void* APS5_VABI __tls_get_addr_nid_postfix(const TlsIndex* index) {
    if (index == nullptr) throw std::runtime_error("__tls_get_addr: null index");
    if (const auto found = threadBlocks.blocks.find(index->module); found != threadBlocks.blocks.end()) return static_cast<char*>(found->second) + index->offset;
    ModuleStorage module;
    {
        std::lock_guard lock(modulesMutex);
        if (modules.empty() || !modules.contains(index->module)) registerModules();
        const auto found = modules.find(index->module);
        if (found == modules.end()) throw std::runtime_error("__tls_get_addr: unknown guest module " + std::to_string(index->module));
        module = found->second;
    }
    const auto& storage = module.storage;
    if (index->offset >= storage.memorySize) throw std::runtime_error("__tls_get_addr: offset exceeds the module's thread storage");
    const auto alignment = std::max<std::size_t>(storage.alignment, 16);
    const auto size = (storage.memorySize + alignment - 1) & ~(alignment - 1);
    void* block = nullptr;
    if (posix_memalign(&block, alignment, size) != 0 || block == nullptr) throw std::runtime_error("__tls_get_addr: cannot allocate the module thread block");
    std::memset(block, 0, size);
    if (storage.fileSize != 0) std::memcpy(block, storage.image, storage.fileSize);
    threadBlocks.blocks.emplace(index->module, block);
    return static_cast<char*>(block) + index->offset;
}

#include "prx/libSceAgcDriver/Execution/include/GpuJournal.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <cxxabi.h>
#ifdef __APPLE__
#include <pthread/qos.h>
#endif
#include <cstdio>
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderWarmup.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAgcDriver/Execution/include/MemoryAccessScope.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <bit>
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include "prx/libc/include/General.hpp"
#include <cstdlib>
#include <chrono>
#include <set>
#include <exception>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <unordered_map>

namespace AgcDriver {
namespace {

void require(bool condition, const char* reason) {
    if (!condition) {
        throw std::runtime_error(std::string("AGC driver: ") + reason);
    }
}

struct ShaderSnapshot {
    std::uint64_t codeAddress;
    std::uint64_t headerAddress;
    std::uint8_t type;
    std::vector<std::uint32_t> code;
    std::vector<std::byte> header;

    struct CodeHashes {
        std::mutex mutex;
        std::unordered_map<std::size_t, std::uint64_t> byOffset;
    };
    std::shared_ptr<CodeHashes> hashes = std::make_shared<CodeHashes>();

    std::uint64_t CodeHash(std::size_t offset) const {
        std::lock_guard lock(hashes->mutex);
        if (const auto found = hashes->byOffset.find(offset); found != hashes->byOffset.end()) return found->second;
        const auto hash = ShaderCodeHash(std::span(code).subspan(offset));
        hashes->byOffset.emplace(offset, hash);
        return hash;
    }
};

struct Submission {
    std::uint64_t serial;
    std::uint32_t queue;
    std::vector<std::uint32_t> commands;
    std::deque<std::vector<std::uint32_t>> registerLists;
    std::map<std::uint64_t, std::shared_ptr<const ShaderSnapshot>> shaders;
    std::map<std::size_t, std::shared_ptr<IFlipRequest>> flips;
    std::map<std::size_t, std::shared_ptr<IRenderingWait>> renderingWaits;
    bool suspend = false;
    FrameTiming::Clock::time_point received;
    FrameTiming::Clock::time_point copied;
    FrameTiming::Clock::time_point validated;
    FrameTiming::Clock::time_point enqueued;
    FrameTiming::Clock::time_point dequeued;
};

void preferPerformanceCores() {
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

std::uint32_t readRegister(const Registers& registers, std::uint32_t offset) {
    const auto it = registers.find(offset);
    if (it == registers.end()) {
        std::string message = "required shader register 0x" + std::to_string(offset) + " has not been written; written neighbours:";
        for (const auto& [key, value] : registers) {
            if (key + 16 < offset || key > offset + 16) continue;
            char text[40];
            std::snprintf(text, sizeof(text), " 0x%x=0x%x", key, value);
            message += text;
        }
        require(false, message.c_str());
    }
    return it->second;
}

std::uint32_t readUserData(const Registers& registers, std::uint32_t offset) {
    const auto it = registers.find(offset);
    if (it != registers.end()) return it->second;
    static std::once_flag once;
    std::call_once(once, [&] { APS5_LOG_OUT("user-data register 0x%x was never written; reading zero", offset); });
    return 0;
}

struct ProgramError : std::runtime_error {
    ProgramError(const std::string& message, std::uint64_t program, std::uint64_t hash) : std::runtime_error(message), address(program), codeHash(hash) {}
    std::uint64_t address;
    std::uint64_t codeHash;
};

bool SkipFailedPrograms() {
    static const bool skip = std::getenv("ANYPS5_SKIP_FAILED_PROGRAMS") != nullptr;
    return skip;
}

std::mutex failedProgramsMutex;
std::set<std::pair<std::uint64_t, std::uint64_t>> failedPrograms;

bool KnownFailedProgram(std::uint64_t address, std::uint64_t codeHash) {
    if (!SkipFailedPrograms()) return false;
    std::lock_guard lock(failedProgramsMutex);
    return failedPrograms.contains({address, codeHash});
}

ProgramError ProgramFailure(const char* kind, const ShaderRecompiler::ShaderBinary& shader, const std::exception& error) {
    if (const char* dumpDirectory = std::getenv("ANYPS5_DUMP_GDS_SHADERS"); dumpDirectory != nullptr) {
        char name[64];
        std::snprintf(name, sizeof(name), "/failed_%s_%llx.bin", kind, static_cast<unsigned long long>(shader.codeAddress));
        if (FILE* file = std::fopen((std::string(dumpDirectory) + name).c_str(), "wb")) {
            std::fwrite(shader.code.data(), sizeof(std::uint32_t), shader.code.size(), file);
            std::fclose(file);
        }
        try {
            constexpr ShaderRecompiler::RdnaInstructionDecoder decoder;
            const auto text = ShaderRecompiler::RdnaProgramToString(decoder.Decode(shader.code));
            std::snprintf(name, sizeof(name), "/failed_%s_%llx.rdna.txt", kind, static_cast<unsigned long long>(shader.codeAddress));
            if (FILE* file = std::fopen((std::string(dumpDirectory) + name).c_str(), "wb")) {
                std::fwrite(text.data(), 1, text.size(), file);
                std::fclose(file);
            }
        } catch (const std::exception&) {
        }
    }
    char where[96];
    std::snprintf(where, sizeof(where), "%s program 0x%llx (stage %u): ", kind, static_cast<unsigned long long>(shader.codeAddress), static_cast<unsigned>(shader.stage));
    return ProgramError(where + std::string(error.what()), shader.codeAddress, shader.codeHash);
}

void ReportUnresolvedImages(const char* kind, std::uint64_t address, const ShaderRecompiler::RecompileResult& result) {
    if (result.unresolvedImages == 0) return;
    static std::mutex reportedMutex;
    static std::set<std::uint64_t> reported;
    std::lock_guard lock(reportedMutex);
    if (reported.insert(address).second) std::fprintf(stderr, "AGC driver: %s program 0x%llx samples %u image(s) whose descriptors are selected at run time; they read as null textures\n", kind, static_cast<unsigned long long>(address), result.unresolvedImages);
}

bool StorageImageWrites(const ShaderRecompiler::DescriptorBinding& binding, std::vector<std::pair<std::uint64_t, std::uint64_t>>& writes) {
    if (binding.count == 0 || binding.guestDescriptor.size() != static_cast<std::size_t>(binding.count) * 8) return false;
    for (std::size_t element = 0; element < binding.count; ++element) {
        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(element * 8, 8);
        if (words[0] == 0 && (words[1] & 0xffu) == 0) continue;
        try {
            const auto resource = Graphics::DecodeTextureResource(words);
            if (resource.baseAddress == 0) continue;
            const auto mips = Graphics::ComputeMipLayout(resource.tileMode, resource.format, resource.width, resource.height, resource.mipCount);
            const auto bytes = Graphics::ComputeSurfaceSize(mips, Graphics::FullArrayLayers(resource));
            if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - resource.baseAddress) return false;
            writes.emplace_back(resource.baseAddress, resource.baseAddress + bytes);
        } catch (const std::exception&) {
            return false;
        }
    }
    return true;
}

template<typename Work>
void SkippingFailedPrograms(Work&& work) {
    if (!SkipFailedPrograms()) {
        work();
        return;
    }
    try {
        work();
    } catch (const ProgramError& error) {
        std::lock_guard lock(failedProgramsMutex);
        if (failedPrograms.insert({error.address, error.codeHash}).second) std::fprintf(stderr, "AGC driver: skipping work of a program that cannot be prepared: %s\n", error.what());
    }
}

class Driver {
    struct GraphicsJob {
        std::function<void()> run;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
        bool writesUnknown = false;
    };

public:
    static Driver& Get() {
        static Driver driver;
        return driver;
    }

    ~Driver() {
        stop();
    }

    void Shutdown() {
        stop();
        CheckFailure();
    }

private:
    void stop() {
        require(std::this_thread::get_id() != worker.get_id(), "worker cannot stop itself");
        std::lock_guard shutdownLock(shutdownMutex);
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_all();
        if (worker.joinable()) worker.join();
        {
            std::lock_guard lock(graphicsMutex);
            graphicsStopping = true;
        }
        graphicsChanged.notify_all();
        if (graphicsThread.joinable()) graphicsThread.join();
        warmup.Stop();
    }

    void runGraphics() noexcept {
        preferPerformanceCores();
        std::unique_lock lock(graphicsMutex);
        while (true) {
            graphicsChanged.wait(lock, [&] { return graphicsStopping || !graphicsJobs.empty(); });
            if (graphicsJobs.empty()) return;
            auto run = std::move(graphicsJobs.front().run);
            graphicsBusy = true;
            lock.unlock();
            try {
                run();
            } catch (...) {
                ReportFailure(std::current_exception());
            }
            lock.lock();
            graphicsJobs.pop_front();
            graphicsBusy = false;
            graphicsCompleted.fetch_add(1, std::memory_order_release);
            graphicsChanged.notify_all();
        }
    }

    void postGraphics(GraphicsJob job) {
        if (!job.writes.empty() || job.writesUnknown) postedWrites.push_back({graphicsPosted + 1, job.writes, job.writesUnknown});
        ++graphicsPosted;
        std::unique_lock lock(graphicsMutex);
        graphicsChanged.wait(lock, [&] { return graphicsJobs.size() < MaxGraphicsJobs || graphicsStopping; });
        require(!graphicsStopping, "graphics work after shutdown");
        graphicsJobs.push_back(std::move(job));
        graphicsChanged.notify_all();
    }

    void drainGraphics() {
        PerformanceTimer timing("Driver.GraphicsDrain");
        std::unique_lock lock(graphicsMutex);
        graphicsChanged.wait(lock, [&] { return graphicsJobs.empty(); });
    }

    bool graphicsPending() const {
        return graphicsCompleted.load(std::memory_order_acquire) != graphicsPosted;
    }

    bool graphicsMayWrite(std::uint64_t address, std::size_t bytes) {
        const auto completed = graphicsCompleted.load(std::memory_order_acquire);
        while (!postedWrites.empty() && postedWrites.front().number <= completed) postedWrites.pop_front();
        const auto end = address + bytes;
        for (const auto& job : postedWrites) {
            if (job.writesUnknown) return true;
            for (const auto& [first, last] : job.writes) if (first < end && address < last) return true;
        }
        return false;
    }

    static void resolveForHost(void* context, std::uint64_t address, std::size_t bytes, bool writable) {
        auto& self = *static_cast<Driver*>(context);
        auto* current = self.graphicsDevice.get();
        if (current == nullptr) return;
        const bool queuedWrite = self.graphicsMayWrite(address, bytes);
        const bool recorded = !queuedWrite && current->NeedsResolve(address, bytes);
        if (!writable && !queuedWrite && !recorded) return;
        if (static const bool trace = std::getenv("ANYPS5_TRACE_DRAINS") != nullptr; trace) {
            static int reported = 0;
            if (reported++ < 400) {
                std::string ranges;
                {
                    std::lock_guard lock(self.graphicsMutex);
                    for (const auto& job : self.graphicsJobs) {
                        if (job.writesUnknown) { ranges += " unknown"; continue; }
                        for (const auto& [first, last] : job.writes) if (first < address + bytes && address < last) { char item[64]; std::snprintf(item, sizeof(item), " 0x%llx+0x%llx", static_cast<unsigned long long>(first), static_cast<unsigned long long>(last - first)); ranges += item; }
                    }
                }
                std::fprintf(stderr, "[drain] 0x%llx+0x%zx %s queued %d recorded %d:%s\n", static_cast<unsigned long long>(address), bytes, writable ? "write" : "read", queuedWrite ? 1 : 0, recorded ? 1 : 0, ranges.c_str());
            }
        }
        self.drainGraphics();
        current->ResolveMemory(address, bytes, writable);
    }
    static bool quietForHost(void* context, std::uint64_t address, std::size_t bytes) {
        auto& self = *static_cast<Driver*>(context);
        return self.graphicsDevice == nullptr || (!self.graphicsMayWrite(address, bytes) && !self.graphicsDevice->NeedsResolve(address, bytes));
    }
    std::shared_ptr<VulkanDevice> graphicsDevice;
    std::mutex deviceMutex;

    std::shared_ptr<VulkanDevice> currentDevice(bool create = false) {
        std::lock_guard lock(deviceMutex);
        if (device == nullptr && create) device = std::make_shared<VulkanDevice>();
        return device;
    }

public:

    void Submit(const Packet* packets, std::uint32_t count, std::uint32_t queue) {
        const auto received = FrameTiming::Clock::now();
        CheckFailure();
        require(queue == 0 || (queue >= 0x20 && queue < 0x58), "unsupported compute queue");
        require(count != 0, "empty submission");
        GuestMemory::CheckRange(packets, sizeof(Packet) * count, alignof(Packet));
        Submission submission{};
        submission.queue = queue;
        submission.received = received;
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto descriptor = packets[index];
            require(descriptor.flags == 0, "nonzero submission flags are not implemented");
            if (descriptor.dw_num == 0) continue;
            require(descriptor.dw_num <= std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t), "command size overflow");
            GuestMemory::CheckRange(descriptor.addr, static_cast<std::size_t>(descriptor.dw_num) * sizeof(std::uint32_t), alignof(std::uint32_t));
            appendCommands(submission.commands, submission.registerLists, descriptor.addr, descriptor.dw_num, 0);
        }
        submission.copied = FrameTiming::Clock::now();
        validate(submission.commands, queue);
        submission.validated = FrameTiming::Clock::now();
        {
            std::lock_guard lock(mutex);
            rethrowFailure();
            require(!stopping, "submission during shutdown");
            require(accepted != std::numeric_limits<std::uint64_t>::max(), "submission serial overflow");
            for (std::size_t cursor = 0; cursor < submission.commands.size();) {
                const auto* words = submission.commands.data() + cursor;
                if (words[0] == RenderingWaitPacketHeader) {
                    const auto output = outputs.find(words[1]);
                    require(output != outputs.end(), "rendering wait references an unregistered video output");
                    auto wait = output->second->CaptureRenderingWait(words[2]);
                    require(wait != nullptr, "video output returned a null rendering wait");
                    submission.renderingWaits.emplace(cursor, std::move(wait));
                }
                if (words[0] == FlipPacketHeader) {
                    const auto output = outputs.find(words[1]);
                    require(output != outputs.end(), "flip references an unregistered video output");
                    const FlipInfo info{words[1], std::bit_cast<std::int32_t>(words[2]), words[3], std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(words[4]) | (static_cast<std::uint64_t>(words[5]) << 32u))};
                    auto request = output->second->Reserve(info);
                    require(request != nullptr, "video output returned a null flip reservation");
                    submission.flips.emplace(cursor, std::move(request));
                }
                cursor += static_cast<std::size_t>((words[0] >> 16u) & 0x3fffu) + 2;
            }
            submission.shaders = shaders;
            submission.serial = accepted + 1;
            submission.enqueued = FrameTiming::Clock::now();
            pending.push_back(std::move(submission));
            ++accepted;
        }
        changed.notify_all();
    }

    void WaitIdle() {
        SuspendPoint();
    }

    void SuspendPoint() {
        const auto received = FrameTiming::Clock::now();
        require(std::this_thread::get_id() != worker.get_id(), "worker cannot suspend itself");
        std::unique_lock lock(mutex);
        rethrowFailure();
        require(!stopping, "suspend during shutdown");
        require(accepted != std::numeric_limits<std::uint64_t>::max(), "submission serial overflow");
        Submission boundary{};
        boundary.serial = accepted + 1;
        boundary.suspend = true;
        boundary.received = received;
        boundary.copied = received;
        boundary.validated = received;
        boundary.enqueued = FrameTiming::Clock::now();
        pending.push_back(std::move(boundary));
        const auto target = ++accepted;
        changed.notify_all();
        changed.wait(lock, [&] { return failure != nullptr || completed >= target; });
        rethrowFailure();
    }

    void RegisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
        require(output != nullptr, "null video output");
        std::lock_guard lock(mutex);
        rethrowFailure();
        require(!stopping, "video output registration during shutdown");
        require(outputs.emplace(handle, output).second, "video output already registered");
    }

    void UnregisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
        std::lock_guard lock(mutex);
        const auto it = outputs.find(handle);
        require(it != outputs.end() && it->second == output, "video output registration mismatch");
        outputs.erase(it);
    }

    void CheckFailure() {
        std::lock_guard lock(mutex);
        rethrowFailure();
    }

    void ReportFailure(std::exception_ptr error) {
        try { if (error) std::rethrow_exception(error); } catch (const std::exception& e) { std::fprintf(stderr, "AGC driver failed: %s\n", e.what()); } catch (...) { const auto* type = abi::__cxa_current_exception_type(); std::fprintf(stderr, "AGC driver failed: exception of type %s\n", type ? type->name() : "?"); }
        GpuJournal::Dump("AGC driver: last GPU work before the failure, oldest first:");
        require(error != nullptr, "null asynchronous failure");
        {
            std::lock_guard lock(mutex);
            if (!failure) failure = error;
            for (const auto& [handle, output] : outputs) output->Fail(failure);
            for (const auto& item : pending) {
                for (const auto& [offset, flip] : item.flips) flip->Fail(failure);
            }
            pending.clear();
        }
        changed.notify_all();
    }

    void Present(const PresentationWindow& window, const DisplayBuffer* buffer, bool opaque, void (*gpuReady)(void*), void* context) {
        PerformanceContext timingContext(window.timing.get());
        PerformanceTimer timing("Driver.Present");
        CheckFailure();
        require(gpuReady != nullptr && context != nullptr, "missing GPU completion callback");
        require(window.getDrawableSize != nullptr, "missing window drawable size query");
        std::shared_ptr<VulkanDevice> presenting;
        timing.Mark("validate");
        try {
            {
                if (const auto existing = currentDevice(); existing == nullptr || existing->Window() == nullptr) drainGraphics();
                std::unique_lock lock(gpuMutex);
                timing.Mark("gpu_mutex_wait");
                if (device == nullptr || device->Window() == nullptr) {
                    if (device) device->WaitIdle();
                    auto replacement = std::make_shared<VulkanDevice>(&window);
                    std::lock_guard deviceLock(deviceMutex);
                    device = std::move(replacement);
                }
                require(device->Window() == window.context, "presentation window does not match device surface");
                presenting = device;
                timing.Mark("device_setup");
                std::uint32_t drawableWidth = 0;
                std::uint32_t drawableHeight = 0;
                window.getDrawableSize(window.context, &drawableWidth, &drawableHeight);
                presenting->Resize(drawableWidth, drawableHeight);
                timing.Mark("resize");
                if (presenting->Presentable()) {
                    if (buffer != nullptr) {
                        require(buffer->width == window.width && buffer->height == window.height, "display buffer extent differs from output");
                        if (AsyncFlips()) {
                            const auto ticket = presenting->SubmitTicket();
                            lock.unlock();
                            presenting->WaitTicket(ticket);
                            lock.lock();
                        } else {
                            presenting->WaitDraws();
                        }
                        timing.Mark("draw_wait");
                        presenting->PresentDisplayBuffer(*buffer);
                        timing.Mark("present_display_buffer");
                    } else {
                        presenting->PresentClear(window.width, window.height, opaque);
                        timing.Mark("present_clear");
                    }
                }
            }
            gpuReady(context);
            timing.Mark("release_and_callback");
            CheckFailure();
        } catch (...) {
            ReportFailure(std::current_exception());
            throw;
        }
    }

    void ReleaseWindow(void* window) {
        drainGraphics();
        std::lock_guard lock(gpuMutex);
        std::lock_guard deviceLock(deviceMutex);
        if (device && device->Window() == window) device.reset();
    }

    void RegisterShader(const Shader* shader) {
        CheckFailure();
        GuestMemory::CheckRange(shader, sizeof(Shader), 4);
        require(shader->file_header == 0x34333231u && shader->version == 0x18u, "invalid shader header");
        require(shader->header_size >= sizeof(Shader), "shader header is smaller than its fixed fields");
        require(shader->shader_size != 0 && (shader->shader_size & 3u) == 0, "invalid shader size");
        GuestMemory::CheckRange(shader, shader->header_size, 4);
        const auto* code = const_cast<const void*>(shader->code);
        GuestMemory::CheckRange(code, shader->shader_size, 256);
        ShaderSnapshot snapshot{reinterpret_cast<std::uintptr_t>(code), reinterpret_cast<std::uintptr_t>(shader), shader->type, {}, {}};
        snapshot.code.resize(shader->shader_size / sizeof(std::uint32_t));
        std::memcpy(snapshot.code.data(), code, shader->shader_size);
        snapshot.header.resize(shader->header_size);
        std::memcpy(snapshot.header.data(), shader, shader->header_size);
        std::lock_guard lock(mutex);
        rethrowFailure();
        const auto address = snapshot.codeAddress;
        shaders.insert_or_assign(address, std::make_shared<const ShaderSnapshot>(std::move(snapshot)));
    }

private:
    std::mutex mutex;
    std::mutex shutdownMutex;
    std::condition_variable changed;
    std::deque<Submission> pending;
    std::map<std::uint64_t, std::shared_ptr<const ShaderSnapshot>> shaders;
    std::map<std::uint32_t, QueueState> queues;
    std::map<std::uint32_t, std::shared_ptr<IVideoOutput>> outputs;
    std::recursive_mutex& gpuMutex = GuestMemoryTracking::GuestMemoryTrackingMutex_nid_postfix();
    std::shared_ptr<VulkanDevice> device;
    std::uint64_t accepted = 0;
    std::uint64_t completed = 0;
    std::set<std::uint64_t> inFlight;
    std::uint64_t dequeuedSerial = 0;
    std::exception_ptr failure;
    bool stopping = false;
    std::shared_ptr<FrameTiming> frameTiming;
    std::uint64_t frameSerial = 0;

    static constexpr std::size_t MaxGraphicsJobs = 256;
    std::mutex graphicsMutex;
    std::condition_variable graphicsChanged;
    std::deque<GraphicsJob> graphicsJobs;
    bool graphicsBusy = false;
    struct PostedWrites {
        std::uint64_t number;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
        bool writesUnknown;
    };
    std::deque<PostedWrites> postedWrites;
    std::uint64_t graphicsPosted = 0;
    std::atomic<std::uint64_t> graphicsCompleted{0};
    bool graphicsStopping = false;
    ShaderWarmup warmup;

    Driver() : graphicsThread([this] { runGraphics(); }), worker([this] { run(); }) {
        try {
            LibcRegisterShutdown_nid_postfix([] { Driver::Get().Shutdown(); });
        } catch (...) {
            stop();
            throw;
        }
    }

    void rethrowFailure() const {
        if (failure != nullptr) {
            std::rethrow_exception(failure);
        }
    }

    static void appendCommands(std::vector<std::uint32_t>& out, std::deque<std::vector<std::uint32_t>>& registerLists, const std::uint32_t* words, std::uint32_t count, unsigned depth) {
        require(depth <= 32, "indirect buffer nesting is too deep");
        std::uint32_t chainLinks = 0;
        for (std::uint32_t cursor = 0; cursor < count;) {
            const auto header = words[cursor];
            if (header == HeaderOnlyNopPacket) {
                ++cursor;
                continue;
            }
            require((header & 0xc0000000u) == 0xc0000000u, "unsupported PM4 packet type");
            const auto size = ((header >> 16u) & 0x3fffu) + 2u;
            require(size <= count - cursor, "truncated PM4 packet");
            if (((header >> 8u) & 0xffu) == 0x3f) {
                require(size == 4, "invalid INDIRECT_BUFFER packet size");
                const auto target = static_cast<std::uint64_t>(words[cursor + 1]) | (static_cast<std::uint64_t>(words[cursor + 2]) << 32u);
                const auto targetCount = words[cursor + 3] & 0xfffffu;
                const bool chain = (words[cursor + 3] & 0x100000u) != 0;
                require(target != 0 && target % 4 == 0 && targetCount != 0, "invalid indirect buffer target");
                try {
                    GuestMemory::CheckRange(reinterpret_cast<const void*>(target), static_cast<std::size_t>(targetCount) * sizeof(std::uint32_t), alignof(std::uint32_t));
                } catch (const std::exception& error) {
                    char text[240];
                    std::snprintf(text, sizeof(text), "%s: INDIRECT_BUFFER {%08x %08x %08x %08x} at DWORD %u of the buffer at %p (%u DWORDs, depth %u)", error.what(), words[cursor], words[cursor + 1], words[cursor + 2], words[cursor + 3], cursor, static_cast<const void*>(words), count, depth);
                    throw std::runtime_error(text);
                }
                require(out.size() <= (std::size_t{1} << 26) - targetCount, "command stream is too large");
                if (chain) {
                    require(++chainLinks <= (1u << 16), "indirect buffer chain is too long");
                    words = reinterpret_cast<const std::uint32_t*>(target);
                    count = targetCount;
                    cursor = 0;
                    continue;
                }
                appendCommands(out, registerLists, reinterpret_cast<const std::uint32_t*>(target), targetCount, depth + 1);
            } else {
                const auto opcode = (header >> 8u) & 0xffu;
                out.insert(out.end(), words + cursor, words + cursor + size);
                if ((opcode == 0x63 || opcode == 0x64 || opcode == 0x9f) && size == 5 && words[cursor + 3] == 0x80000000u && words[cursor + 4] != 0) {
                    const auto source = static_cast<std::uint64_t>(words[cursor + 1]) | (static_cast<std::uint64_t>(words[cursor + 2]) << 32u);
                    const auto pairs = words[cursor + 4] & 0x3fffu;
                    if (source != 0 && source % 4 == 0) {
                        registerLists.emplace_back(static_cast<std::size_t>(pairs) * 2);
                        auto& copy = registerLists.back();
                        try {
                            GuestMemory::Read(source, std::as_writable_bytes(std::span(copy)), 4);
                            const auto host = reinterpret_cast<std::uintptr_t>(copy.data());
                            out[out.size() - 4] = static_cast<std::uint32_t>(host);
                            out[out.size() - 3] = static_cast<std::uint32_t>(host >> 32u);
                        } catch (const std::exception&) {
                            registerLists.pop_back();
                        }
                    }
                }
            }
            cursor += size;
        }
    }

    static void validate(std::span<const std::uint32_t> commands, std::uint32_t queue) {
        for (std::size_t cursor = 0; cursor < commands.size();) {
            const auto header = commands[cursor];
            require((header & 0xc0000000u) == 0xc0000000u, "unsupported PM4 packet type");
            const auto count = static_cast<std::size_t>((header >> 16u) & 0x3fffu) + 2;
            require(count <= commands.size() - cursor, "truncated PM4 packet");
            try {
                Pm4::Validate(commands.subspan(cursor, count), queue);
            } catch (const std::exception& error) {
                std::string words;
                for (std::size_t index = 0; index < std::min<std::size_t>(count, 8); ++index) {
                    char word[12];
                    std::snprintf(word, sizeof(word), " %08x", commands[cursor + index]);
                    words += word;
                }
                throw std::runtime_error("AGC driver: " + Pm4::Name(header) + " at DWORD " + std::to_string(cursor) + ": " + error.what() + " (packet" + words + ")");
            }
            cursor += count;
        }
    }

    void dispatch(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission) {
        PerformanceTimer timing("Driver.Dispatch");
        const auto address = (static_cast<std::uint64_t>(readRegister(queue.shader, 0x20c)) << 8u) | (static_cast<std::uint64_t>(readRegister(queue.shader, 0x20d) & 0xffu) << 40u);
        auto it = submission.shaders.upper_bound(address);
        require(it != submission.shaders.begin(), "compute program does not belong to a registered shader");
        --it;
        const auto& snapshot = *it->second;
        require(address - snapshot.codeAddress < snapshot.code.size() * sizeof(std::uint32_t), "compute program is outside registered shader code");
        require(snapshot.type == 0, "compute program refers to a non-compute shader");
        if (KnownFailedProgram(address, snapshot.CodeHash((address - snapshot.codeAddress) / sizeof(std::uint32_t)))) return;
        const auto userCount = (readRegister(queue.shader, 0x213) >> 1u) & 0x1fu;
        std::vector<std::uint32_t> userData;
        for (std::uint32_t i = 0; i < userCount; ++i) {
            userData.push_back(readUserData(queue.shader, 0x240 + i));
        }
        static const bool traceDispatch = std::getenv("ANYPS5_TRACE_INDIRECT") != nullptr;
        if (traceDispatch) {
            std::string text;
            for (std::size_t i = 0; i < userData.size() && i < 16; ++i) { char item[12]; std::snprintf(item, sizeof(item), " %08x", userData[i]); text += item; }
            std::fprintf(stderr, "[dispatch] program 0x%llx groups %ux%ux%u user data:%s\n", static_cast<unsigned long long>(address), packet[1], packet[2], packet[3], text.c_str());
        }
        const auto compute = Graphics::DecodeComputeStageInfo(queue.shader);
        const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}};
        const auto current = currentDevice(true);
        graphicsDevice = current;
        const GuestMemory::MemoryAccessScope memoryScope(this, &Driver::resolveForHost, &Driver::quietForHost);
        const auto codeOffset = static_cast<std::size_t>((address - snapshot.codeAddress) / sizeof(std::uint32_t));
        ShaderRecompiler::RecompileRequest request{
            {ShaderRecompiler::ShaderStage::Compute, address, std::span(snapshot.code).subspan(codeOffset), snapshot.headerAddress, snapshot.header, snapshot.CodeHash(codeOffset)},
            {(packet[4] & 0x8000u) != 0 ? 32u : 64u, 0, userData, compute, std::nullopt, std::nullopt, memory},
            current->Target(),
            {0, 0, 0, 128}
        };
        auto shaderMemoryOwner = std::make_unique<ShaderMemory>(memory, std::vector<std::shared_ptr<const void>>{it->second});
        auto& shaderMemory = *shaderMemoryOwner;
        timing.Mark("prepare");
        std::remove_cvref_t<decltype(shaderMemory.Regions())> captured;
        auto compiled = [&] {
            try {
                shaderMemory.Capture(request);
                timing.Mark("shader_memory_capture");
                captured = shaderMemory.Regions();
                request.context.memory = captured;
                request.materializedSnapshot = &shaderMemory.Snapshot();
                request.materializedSpecialization = &shaderMemory.Specialization();
                request.source = shaderMemory.Source();
                timing.Mark("request_memory");
                return ShaderRecompiler::Recompile(request);
            } catch (const std::exception& error) {
                throw ProgramFailure("compute", request.shader, error);
            }
        }();
        if (!compiled.cacheHit) warmup.Record(request);
        ReportUnresolvedImages("compute", address, compiled);
        timing.Mark(compiled.cacheHit ? "shader_cache_hit" : "shader_compile");
        if (static const char* spirvDirectory = std::getenv("ANYPS5_DUMP_COMPUTE_SPIRV"); spirvDirectory != nullptr) {
            static std::set<std::uint64_t> dumpedPrograms;
            if (dumpedPrograms.insert(address).second) {
                char name[64];
                std::snprintf(name, sizeof(name), "/cs_%llx.spv", static_cast<unsigned long long>(address));
                if (FILE* file = std::fopen((std::string(spirvDirectory) + name).c_str(), "wb")) {
                    std::fwrite(compiled.spirv.data(), sizeof(std::uint32_t), compiled.spirv.size(), file);
                    std::fclose(file);
                }
                std::snprintf(name, sizeof(name), "/cs_%llx.bin", static_cast<unsigned long long>(address));
                if (FILE* file = std::fopen((std::string(spirvDirectory) + name).c_str(), "wb")) {
                    std::fwrite(request.shader.code.data(), sizeof(std::uint32_t), request.shader.code.size(), file);
                    std::fclose(file);
                }
            }
        }
        if (traceDispatch) {
            bool gds = false;
            for (const auto& binding : compiled.bindings) gds = gds || binding.role == ShaderRecompiler::DescriptorRole::Gds;
            static const bool traceAllBuffers = std::getenv("ANYPS5_TRACE_DISPATCH_BUFFERS") != nullptr;
            if (gds || traceAllBuffers) {
                std::string buffers;
                for (const auto& binding : compiled.bindings) {
                    if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
                    for (std::size_t element = 0; element + 4 <= binding.guestDescriptor.size(); element += 4) {
                        const auto& d = binding.guestDescriptor;
                        char item[96];
                        const auto base = d[element] | (static_cast<std::uint64_t>(d[element + 1] & 0xffffu) << 32u);
                        const auto stride = (d[element + 1] >> 16u) & 0x3fffu;
                        std::snprintf(item, sizeof(item), " [base 0x%llx stride %u records %u%s", static_cast<unsigned long long>(base), stride, d[element + 2], element / 4 < binding.elementWritten.size() && binding.elementWritten[element / 4] ? " written" : "");
                        buffers += item;
                        if (traceAllBuffers && static_cast<std::uint64_t>(std::max(stride, 1u)) * d[element + 2] <= 4096 && base != 0) {
                            std::array<std::uint32_t, 8> words{};
                            try {
                                GuestMemory::Read(base, std::as_writable_bytes(std::span(words)), 1);
                                buffers += " =";
                                for (const auto word : words) { std::snprintf(item, sizeof(item), " %08x", word); buffers += item; }
                            } catch (...) {
                                buffers += " (unreadable)";
                            }
                        }
                        buffers += "]";
                    }
                }
                std::fprintf(stderr, "[dispatch] program 0x%llx groups %ux%ux%u %s; %zu user dwords; buffers:%s\n", static_cast<unsigned long long>(address), packet[1], packet[2], packet[3], gds ? "binds GDS" : "no GDS", userData.size(), buffers.c_str());
                if (static const char* dumpDirectory = std::getenv("ANYPS5_DUMP_GDS_SHADERS"); dumpDirectory != nullptr && (gds || traceAllBuffers)) {
                    static std::set<std::uint64_t> dumped;
                    if (dumped.insert(address).second) {
                        char name[64];
                        std::snprintf(name, sizeof(name), "/gds_%llx.rdna.txt", static_cast<unsigned long long>(address));
                        if (FILE* file = std::fopen((std::string(dumpDirectory) + name).c_str(), "wb")) {
                            constexpr ShaderRecompiler::RdnaInstructionDecoder decoder;
                            const auto text = ShaderRecompiler::RdnaProgramToString(decoder.Decode(request.shader.code));
                            std::fwrite(text.data(), 1, text.size(), file);
                            std::fclose(file);
                        }
                    }
                }
            }
        }
        std::vector<Graphics::GuestMemorySnapshot> snapshots;
        snapshots.reserve(captured.size());
        for (const auto& region : captured) snapshots.push_back({region.guestAddress, region.bytes});
        timing.Mark("snapshots");
        {
            char text[160];
            std::snprintf(text, sizeof(text), "dispatch program 0x%llx groups %ux%ux%u wave%u%s", static_cast<unsigned long long>(address), packet[1], packet[2], packet[3], (packet[4] & 0x8000u) != 0 ? 32u : 64u, journalIndirect ? " (indirect)" : "");
            GpuJournal::Record(text);
        }
        GraphicsJob job;
        for (const auto& binding : compiled.bindings) {
            if (binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers) {
                for (std::size_t element = 0; element < binding.elementWritten.size() && element * 4 + 4 <= binding.guestDescriptor.size(); ++element) {
                    if (!binding.elementWritten[element]) continue;
                    const auto* words = binding.guestDescriptor.data() + element * 4;
                    const auto base = (words[0] | (static_cast<std::uint64_t>(words[1]) << 32u)) & 0xffffffffffffull;
                    const auto stride = (words[1] >> 16u) & 0x3fffu;
                    const auto bytes = stride == 0 ? static_cast<std::uint64_t>(words[2]) : static_cast<std::uint64_t>(stride) * words[2];
                    if (base == 0 || bytes == 0 || (element < binding.elementOptional.size() && binding.elementOptional[element] && bytes > (64ull << 20u))) continue;
                    job.writes.emplace_back(base, base + bytes);
                }
            } else if (!binding.readOnly && binding.kind == ShaderRecompiler::DescriptorKind::StorageImage && !StorageImageWrites(binding, job.writes)) {
                job.writesUnknown = true;
            }
        }
        struct DispatchWork {
            std::shared_ptr<VulkanDevice> device;
            std::shared_ptr<FrameTiming> timing;
            std::unique_ptr<ShaderMemory> memory;
            ShaderRecompiler::RecompileResult compiled;
            std::array<std::uint32_t, 3> groups;
            std::vector<Graphics::GuestMemorySnapshot> snapshots;
            std::uint64_t program;
        };
        auto work = std::make_shared<DispatchWork>(DispatchWork{current, frameTiming, std::move(shaderMemoryOwner), std::move(compiled), {packet[1], packet[2], packet[3]}, std::move(snapshots), address});
        job.run = [work] {
            PerformanceContext timingContext(work->timing.get());
            GpuJournal::CurrentProgram = work->program;
            work->device->Dispatch(work->compiled, work->groups[0], work->groups[1], work->groups[2], work->snapshots);
        };
        postGraphics(std::move(job));
        timing.Mark("post");
    }

    void draw(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission) {
        PerformanceTimer timing("Driver.Draw");
        if (const auto colorMode = (readRegister(queue.context, 0x202) >> 4u) & 7u; colorMode == 2 || colorMode >= 4) {
            static std::array<std::once_flag, 8> reported;
            std::call_once(reported[colorMode], [colorMode] { std::fprintf(stderr, "AGC driver: skipping color metadata passes (CB_COLOR_CONTROL mode %u); compression metadata is not emulated\n", colorMode); });
            return;
        }
        auto drawParameters = Pm4::ResolveDraw(packet, queue);
        if (!drawParameters.indexed && (drawParameters.indexCount == 0 || drawParameters.instanceCount == 0)) return;
        const auto graphics = Graphics::DecodeState(queue);
        if (graphics.emptyViewport) {
            static std::once_flag reported;
            std::call_once(reported, [] { std::fprintf(stderr, "AGC driver: skipping draws with a zero-sized viewport\n"); });
            return;
        }
        struct Program {
            ShaderRecompiler::ShaderBinary binary;
            std::uint32_t userDataBase;
            std::uint32_t firstUserSgpr = 8;
            std::vector<std::uint32_t> userData;
            std::array<ShaderRecompiler::MemoryRegion, 2> memory;
            std::shared_ptr<const ShaderSnapshot> owner;
        };
        const auto programAddress = [&](std::uint32_t base) {
            const auto high = readRegister(queue.shader, base + 1);
            require((high & ~0xffu) == 0, "reserved graphics program address bits are set");
            return (static_cast<std::uint64_t>(readRegister(queue.shader, base)) << 8u) | (static_cast<std::uint64_t>(high) << 40u);
        };
        const auto prepare = [&](std::uint64_t address, std::uint8_t type, ShaderRecompiler::ShaderStage stage, std::uint32_t rsrc2, std::uint32_t userDataBase) {
            auto it = submission.shaders.upper_bound(address);
            require(it != submission.shaders.begin(), "graphics program does not belong to a registered shader");
            --it;
            const auto& snapshot = *it->second;
            require(address - snapshot.codeAddress < snapshot.code.size() * sizeof(std::uint32_t), "graphics program is outside registered shader code");
            require(snapshot.type == type, "graphics program refers to an incompatible shader binary type");
            const auto resources = readRegister(queue.shader, rsrc2);
            const auto userCount = ((resources >> 1u) & 0x1fu) | (((resources >> 27u) & 1u) << 5u);
            require(userCount <= 32, "graphics user SGPR count exceeds the register bank");
            const auto codeOffset = static_cast<std::size_t>((address - snapshot.codeAddress) / sizeof(std::uint32_t));
            Program result{
                {stage, address, std::span(snapshot.code).subspan(codeOffset), snapshot.headerAddress, snapshot.header, snapshot.CodeHash(codeOffset)},
                userDataBase,
                8,
                {},
                {{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}},
                it->second
            };
            for (std::uint32_t i = 0; i < userCount; ++i) result.userData.push_back(readUserData(queue.shader, userDataBase + i));
            return result;
        };
        using Stage = ShaderRecompiler::ShaderStage;
        using Role = ShaderRecompiler::ProgramRole;
        std::vector<Program> programs;
        std::vector<Role> roles;
        programs.reserve(5);
        roles.reserve(5);
        const auto append = [&](std::uint32_t base, std::uint8_t type, Stage stage, std::uint32_t resources, std::uint32_t users, Role role) {
            programs.push_back(prepare(programAddress(base), type, stage, resources, users));
            roles.push_back(role);
        };
        const auto initializeMerged = [&](Program& program, std::uint32_t pointerBase, bool pointerRequired) {
            program.firstUserSgpr = 0;
            program.userData.insert(program.userData.begin(), 8, 0);
            if (pointerRequired) {
                const auto low = readRegister(queue.shader, pointerBase);
                const auto high = readRegister(queue.shader, pointerBase + 1);
                const auto address = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32u);
                require(address != 0, "merged shader user-data address is null");
                GuestMemory::CheckRange(reinterpret_cast<const void*>(address), 8, 4);
                program.userData[0] = low;
                program.userData[1] = high;
            }
        };
        if (graphics.stages.path == Graphics::ShaderPath::Tessellation) {
            append(0x148, 5, Stage::Local, 0x10b, 0x10c, Role::Local);
            append(0x108, 7, Stage::TessellationControl, 0x10b, 0x10c, Role::Hull);
            initializeMerged(programs.back(), 0x102, true);
            append(0x0c8, 2, Stage::TessellationEvaluation, 0x08b, 0x08c, Role::Domain);
        } else if (graphics.stages.path == Graphics::ShaderPath::Geometry) {
            const auto frontAddress = programAddress(0xc8);
            auto snapshot = submission.shaders.upper_bound(frontAddress);
            require(snapshot != submission.shaders.begin(), "geometry front program is not registered");
            --snapshot;
            const auto type = snapshot->second->type;
            require(type == 2 || type == 4, "invalid geometry front binary type");
            append(0xc8, type, Stage::Mesh, 0x8b, 0x8c, Role::Main);
            initializeMerged(programs.back(), 0x82, type == 4);
            if (type == 4) append(0x88, 6, Stage::Mesh, 0x8b, 0x8c, Role::GeometryBack);
        } else {
            append(0xc8, 2, Stage::Vertex, 0x8b, 0x8c, Role::Main);
        }
        append(0x008, 1, Stage::Fragment, 0x00b, 0x00c, Role::Fragment);
        programs.back().firstUserSgpr = 0;
        if (static const char* dumpDirectory = std::getenv("ANYPS5_DUMP_GDS_SHADERS"); dumpDirectory != nullptr && std::getenv("ANYPS5_DEBUG_DRAW_TARGETS") != nullptr) {
            static std::set<std::uint64_t> dumped;
            for (const auto& program : programs) {
                if (!dumped.insert(program.binary.codeAddress).second) continue;
                char name[64];
                std::snprintf(name, sizeof(name), "/gfx_%llx.rdna.txt", static_cast<unsigned long long>(program.binary.codeAddress));
                if (FILE* file = std::fopen((std::string(dumpDirectory) + name).c_str(), "wb")) {
                    constexpr ShaderRecompiler::RdnaInstructionDecoder decoder;
                    const auto text = ShaderRecompiler::RdnaProgramToString(decoder.Decode(program.binary.code));
                    std::fwrite(text.data(), 1, text.size(), file);
                    std::string users = "user data:";
                    for (const auto word : program.userData) { char item[16]; std::snprintf(item, sizeof(item), " %08x", word); users += item; }
                    users += "\n";
                    std::fwrite(users.data(), 1, users.size(), file);
                    std::fclose(file);
                }
            }
        }
        auto pixel = Graphics::DecodePixelStageInfo(queue.context, graphics.hasColorTarget, graphics.color.componentMapping);
        std::vector<ShaderRecompiler::MemoryRegion> memory;
        std::vector<std::shared_ptr<const void>> memoryOwners;
        std::vector<ShaderRecompiler::LinkedProgram> linked;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            const auto& program = programs[i];
            memory.insert(memory.end(), program.memory.begin(), program.memory.end());
            memoryOwners.push_back(program.owner);
            linked.push_back({roles[i], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
        }
        if (static const bool journalDraws = std::getenv("ANYPS5_GPU_JOURNAL_DRAWS") != nullptr; journalDraws) {
            char text[512];
            std::snprintf(text, sizeof(text), "draw programs 0x%llx/0x%llx %s count %u instances %u target 0x%llx %ux%u format %u%s mask 0x%x blend %u vp %.0fx%.0f@%.0f,%.0f sc %ux%u@%d,%d depth %s%s%s", static_cast<unsigned long long>(programs.front().binary.codeAddress), static_cast<unsigned long long>(programs.back().binary.codeAddress), drawParameters.indexed ? "indexed" : "auto", drawParameters.indexCount, drawParameters.instanceCount, graphics.hasColorTarget ? static_cast<unsigned long long>(graphics.color.address) : 0ull, graphics.renderExtent.width, graphics.renderExtent.height, graphics.hasColorTarget ? static_cast<unsigned>(graphics.color.format) : 0u, graphics.hasDepthTarget ? " depth" : "", graphics.blend.colorWriteMask, graphics.blend.blendEnable, graphics.viewport.width, graphics.viewport.height, graphics.viewport.x, graphics.viewport.y, graphics.scissor.extent.width, graphics.scissor.extent.height, graphics.scissor.offset.x, graphics.scissor.offset.y, graphics.depthState.test ? "test" : "-", graphics.depthState.write ? "+write" : "", graphics.hasColorTarget ? (graphics.color.gpuOnly ? " gpuonly" : "") : "");
            if (graphics.blend.blendEnable) {
                char blend[96];
                std::snprintf(blend, sizeof(blend), " blend(%u,%u,%u a %u,%u,%u)", static_cast<unsigned>(graphics.blend.srcColorBlendFactor), static_cast<unsigned>(graphics.blend.dstColorBlendFactor), static_cast<unsigned>(graphics.blend.colorBlendOp), static_cast<unsigned>(graphics.blend.srcAlphaBlendFactor), static_cast<unsigned>(graphics.blend.dstAlphaBlendFactor), static_cast<unsigned>(graphics.blend.alphaBlendOp));
                std::strncat(text, blend, sizeof(text) - std::strlen(text) - 1);
            }
            if (graphics.hasDepthTarget) {
                char depth[96];
                std::snprintf(depth, sizeof(depth), " zcmp %u%s%s zclear %g", static_cast<unsigned>(graphics.depthState.compare), graphics.depthState.clearDepth ? " cleardepth" : "", graphics.depthState.clearStencil ? " clearstencil" : "", graphics.depthState.depthClear);
                std::strncat(text, depth, sizeof(text) - std::strlen(text) - 1);
            }
            if (graphics.depthState.stencilTest) {
                char stencil[224];
                std::snprintf(stencil, sizeof(stencil), " stencil(ref %u cmp %u rmask 0x%x wmask 0x%x ops %u/%u/%u z 0x%llx s 0x%llx zw 0x%llx sw 0x%llx)", graphics.depthState.front.reference, static_cast<unsigned>(graphics.depthState.front.compareOp), graphics.depthState.front.compareMask, graphics.depthState.front.writeMask, static_cast<unsigned>(graphics.depthState.front.failOp), static_cast<unsigned>(graphics.depthState.front.passOp), static_cast<unsigned>(graphics.depthState.front.depthFailOp), static_cast<unsigned long long>(graphics.depth.address), static_cast<unsigned long long>(graphics.depth.stencilAddress), static_cast<unsigned long long>(graphics.depth.writeAddress), static_cast<unsigned long long>(graphics.depth.stencilWriteAddress));
                std::strncat(text, stencil, sizeof(text) - std::strlen(text) - 1);
            }
            GpuJournal::Record(text);
        }
        timing.Mark("prepare");
        const auto current = currentDevice(true);
        timing.Mark("device_setup");
        graphicsDevice = current;
        const GuestMemory::MemoryAccessScope memoryScope(this, &Driver::resolveForHost, &Driver::quietForHost);
        auto shaderMemoryOwner = std::make_unique<ShaderMemory>(memory, std::move(memoryOwners));
        auto& shaderMemory = *shaderMemoryOwner;
        std::vector<ShaderRecompiler::RecompileResult> results;
        std::vector<Graphics::CompiledShader> stages;
        results.reserve(programs.size() + (graphics.rectList ? 2u : 0u));
        stages.reserve(programs.size());
        std::uint32_t pushCursorBytes = 0;
        for (const auto& program : programs) {
            if (KnownFailedProgram(program.binary.codeAddress, program.binary.codeHash)) return;
        }
        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (roles[i] == Role::GeometryBack) continue;
            const auto& program = programs[i];
            const auto waveSize = program.binary.stage == Stage::Fragment ? graphics.stages.fragmentWaveSize : graphics.stages.vertexWaveSize;
            if (program.binary.stage == Stage::Fragment && !results.empty()) {
                const auto& exported = results.back().parameterExports;
                for (std::uint32_t input = 0; input < pixel.interpolatorCount && input < pixel.interpolatorSettings.size(); ++input) {
                    auto& setting = pixel.interpolatorSettings[input];
                    if ((setting & 0x20u) == 0u && std::find(exported.begin(), exported.end(), setting & 0x1fu) == exported.end()) setting |= 0x20u;
                }
            }
            ShaderRecompiler::RecompileRequest request{
                program.binary,
                {waveSize, program.firstUserSgpr, program.userData, std::nullopt, program.binary.stage == Stage::Fragment ? std::optional(pixel) : std::nullopt, program.binary.stage == Stage::Fragment ? std::nullopt : std::optional(Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData)), memory},
                current->Target(),
                {0, 0, pushCursorBytes, Graphics::PipelinePushConstantBytes - pushCursorBytes},
                ShaderRecompiler::GraphicsCompileContext{program.firstUserSgpr, linked, graphics.stages.mesh, graphics.stages.tessellation, {drawParameters.indexAddress, drawParameters.indexCount, drawParameters.indexSize, drawParameters.instanceCount}}
            };
            PerformanceTimer shaderTiming("Driver.GraphicsShader");
            try {
                shaderMemory.Capture(request);
                shaderTiming.Mark("memory_capture");
                memory = shaderMemory.Regions();
                request.context.memory = memory;
                request.materializedSnapshot = &shaderMemory.Snapshot();
                request.materializedSpecialization = &shaderMemory.Specialization();
                request.source = shaderMemory.Source();
                shaderTiming.Mark("request_memory");
                results.push_back(ShaderRecompiler::Recompile(request));
            } catch (const std::exception& error) {
                throw ProgramFailure("graphics", request.shader, error);
            }
            if (!results.back().cacheHit) warmup.Record(request);
            ReportUnresolvedImages("graphics", program.binary.codeAddress, results.back());
            shaderTiming.Mark(results.back().cacheHit ? "cache_hit" : "compile");
            const auto& result = results.back();
            static const bool indexedOffsets = std::getenv("ANYPS5_NO_INDEXED_BASE_VERTEX") == nullptr;
            if (i == 0 && !graphics.stages.mesh && (indexedOffsets || !drawParameters.indexed)) {
                const auto offsetValue = [&](std::int32_t sgpr) {
                    require(sgpr >= 0 && static_cast<std::uint32_t>(sgpr) >= program.firstUserSgpr, "invalid draw offset SGPR");
                    const auto index = static_cast<std::uint32_t>(sgpr) - program.firstUserSgpr;
                    require(index < program.userData.size(), "draw offset SGPR exceeds user data");
                    return program.userData[index];
                };
                if (drawParameters.firstVertex == 0 && result.vertexOffsetSgpr >= 0) drawParameters.firstVertex = offsetValue(result.vertexOffsetSgpr);
                if (result.instanceOffsetSgpr >= 0) drawParameters.firstInstance = offsetValue(result.instanceOffsetSgpr);
            }
            require(result.pushConstants.size() <= Graphics::PipelinePushConstantBytes - pushCursorBytes, "stage push constants exceed the pipeline push constant block");
            stages.push_back({program.binary.stage, &result, result.pushConstants.empty() ? 0u : pushCursorBytes});
            pushCursorBytes += static_cast<std::uint32_t>(result.pushConstants.size());
        }
        timing.Mark("shaders");
        if (graphics.rectList) {
            require(stages.size() == 2, "rect-list requires vertex and fragment programs");
            auto rectangle = ShaderRecompiler::BuildRectListShaders(results[0], results[1], current->Target());
            results.push_back(std::move(rectangle.control));
            results.push_back(std::move(rectangle.evaluation));
            stages.insert(stages.begin() + 1, {{Stage::TessellationControl, &results[2], 0}, {Stage::TessellationEvaluation, &results[3], 0}});
        }
        try {
            current->ValidateDraw(graphics, stages);
        } catch (const std::exception& error) {
            for (std::size_t i = 0; i + 1 < programs.size(); ++i) static_cast<void>(ProgramFailure("graphics", programs[i].binary, error));
            throw ProgramFailure("graphics", programs.back().binary, error);
        }
        std::vector<Graphics::GuestMemorySnapshot> snapshots;
        snapshots.reserve(memory.size());
        for (const auto& region : memory) snapshots.push_back({region.guestAddress, region.bytes});
        timing.Mark("post_compile_prepare");
        GraphicsJob job;
        if (graphics.hasColorTarget) {
            job.writes.emplace_back(graphics.color.address, graphics.color.address + graphics.color.bytes);
            for (const auto& extra : graphics.extraColors) job.writes.emplace_back(extra.address, extra.address + extra.bytes);
        }
        for (const auto& result : results) {
            for (const auto& binding : result.bindings) {
                if (binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers) {
                    for (std::size_t element = 0; element < binding.elementWritten.size() && element * 4 + 4 <= binding.guestDescriptor.size(); ++element) {
                        if (!binding.elementWritten[element]) continue;
                        const auto* words = binding.guestDescriptor.data() + element * 4;
                        const auto base = (words[0] | (static_cast<std::uint64_t>(words[1]) << 32u)) & 0xffffffffffffull;
                        const auto stride = (words[1] >> 16u) & 0x3fffu;
                        const auto bytes = stride == 0 ? static_cast<std::uint64_t>(words[2]) : static_cast<std::uint64_t>(stride) * words[2];
                        if (base == 0 || bytes == 0 || (element < binding.elementOptional.size() && binding.elementOptional[element] && bytes > (64ull << 20u))) continue;
                        job.writes.emplace_back(base, base + bytes);
                    }
                } else if (!binding.readOnly && binding.kind == ShaderRecompiler::DescriptorKind::StorageImage && !StorageImageWrites(binding, job.writes)) {
                    job.writesUnknown = true;
                }
            }
        }
        struct DrawWork {
            std::shared_ptr<VulkanDevice> device;
            std::shared_ptr<FrameTiming> timing;
            Graphics::State graphics;
            Pm4::DrawParameters draw;
            std::unique_ptr<ShaderMemory> memory;
            std::vector<ShaderRecompiler::RecompileResult> results;
            std::vector<Graphics::CompiledShader> stages;
            std::vector<Graphics::GuestMemorySnapshot> snapshots;
            std::uint64_t program;
        };
        auto work = std::make_shared<DrawWork>(DrawWork{current, frameTiming, graphics, drawParameters, std::move(shaderMemoryOwner), std::move(results), std::move(stages), std::move(snapshots), programs.back().binary.codeAddress});
        job.run = [work] {
            PerformanceContext timingContext(work->timing.get());
            PerformanceTimer timing("Driver.GraphicsJob");
            GpuJournal::CurrentProgram = work->program;
            work->device->EnqueueDraw(work->graphics, work->draw, work->stages, work->snapshots);
            timing.Mark("draw");
        };
        postGraphics(std::move(job));
        timing.Mark("post");
    }

    void includeSubmission(const Submission& submission, bool firstSegment) {
        if (frameTiming == nullptr) {
            require(frameSerial != std::numeric_limits<std::uint64_t>::max(), "frame serial overflow");
            frameTiming = std::make_shared<FrameTiming>(++frameSerial);
        }
        const auto dequeued = firstSegment ? submission.dequeued : FrameTiming::Clock::now();
        frameTiming->IncludeSubmission(submission.serial, submission.received, submission.enqueued, dequeued, firstSegment);
        if (firstSegment) {
            frameTiming->Add(frameTiming->Get("Submission", "copy"), submission.copied - submission.received, submission.commands.size() * sizeof(std::uint32_t));
            frameTiming->Add(frameTiming->Get("Submission", "validate"), submission.validated - submission.copied);
            frameTiming->Add(frameTiming->Get("Submission", "reserve_enqueue"), submission.enqueued - submission.validated);
        }
    }

    struct Execution {
        Submission submission;
        std::size_t cursor = 0;
        bool started = false;
        bool blocked = false;
        bool waitTraced = false;
        FrameTiming::Clock::time_point blockedSince{};
    };
    enum class Step { Progressed, Blocked, Finished };
    std::map<std::uint32_t, std::deque<Execution>> queued;

    struct DeferredWrite {
        std::uint32_t bytes;
        std::uint64_t value;
        bool known;
        std::uint64_t id;
    };
    bool journalIndirect = false;
    std::mutex deferredMutex;
    std::map<std::uint64_t, DeferredWrite> deferred;
    std::uint64_t deferredSerial = 0;

    bool lookupDeferred(std::uint64_t address, std::uint32_t, std::uint64_t& value) {
        std::lock_guard lock(deferredMutex);
        const auto it = deferred.find(address);
        if (it == deferred.end() || !it->second.known) return false;
        value = it->second.value;
        return true;
    }

    Step step(Execution& execution) {
        const Submission& submission = execution.submission;
        if (!execution.started) {
            execution.started = true;
            includeSubmission(submission, true);
        }
        if (submission.suspend) {
            PerformanceContext timingContext(frameTiming.get());
            PerformanceTimer timing("Driver.Suspend");
            drainGraphics();
            std::lock_guard gpuLock(gpuMutex);
            timing.Mark("gpu_mutex_wait");
            if (device != nullptr) device->WaitIdle();
            timing.Mark("device_idle_wait");
            return Step::Finished;
        }
        if (execution.cursor >= submission.commands.size()) return Step::Finished;
        auto& queue = queues[submission.queue];
        queue.id = submission.queue;
        static const bool traceLabels = std::getenv("ANYPS5_TRACE_LABELS") != nullptr;
        if (traceLabels && execution.cursor == 0) std::fprintf(stderr, "[submit] executing serial %llu queue 0x%x (%zu dwords)\n", static_cast<unsigned long long>(submission.serial), submission.queue, submission.commands.size());
        {
            const auto cursor = execution.cursor;
            if (frameTiming == nullptr) includeSubmission(submission, false);
            const auto header = submission.commands[cursor];
            const auto count = static_cast<std::size_t>((header >> 16u) & 0x3fffu) + 2;
            const auto packet = std::span(submission.commands).subspan(cursor, count);
            const auto opcode = (header >> 8u) & 0xffu;
            {
                PerformanceContext timingContext(frameTiming.get());
                PerformanceTimer timing("Driver.Packet");
                CheckFailure();
                timing.Mark("failure_check");
                std::uint64_t writeAddress = 0, writeValue = 0;
                std::uint32_t writeBytes = 0;
                bool writeKnown = false;
                const auto current = currentDevice();
                if (current != nullptr && (opcode == 0x37 || opcode == 0x40 || opcode == 0x50 || opcode == 0x49) && Pm4::DeferrableWrite(packet, writeAddress, writeBytes, writeValue, writeKnown) && (graphicsPending() || current->HasPendingWork())) {
                    const auto id = ++deferredSerial;
                    {
                        std::lock_guard lock(deferredMutex);
                        deferred[writeAddress] = {writeBytes, writeValue, writeKnown, id};
                    }
                    std::vector<std::uint32_t> copy(packet.begin(), packet.end());
                    const auto interrupt = opcode == 0x49 ? (packet[2] >> 24u) & 7u : 0u;
                    const auto interruptId = opcode == 0x49 && packet.size() > 7 ? packet[7] & 0x7ffffffu : 0u;
                    QueueState* queuePointer = &queue;
                    std::function<void()> write = [this, copy = std::move(copy), queuePointer, id, writeAddress, interrupt, interruptId] {
                        Pm4::Execute(std::span<const std::uint32_t>(copy), *queuePointer);
                        if (interrupt != 0) AgcDriver::Eq::Trigger(interruptId);
                        std::lock_guard lock(deferredMutex);
                        const auto it = deferred.find(writeAddress);
                        if (it != deferred.end() && it->second.id == id) deferred.erase(it);
                    };
                    postGraphics({[current, write] { current->Defer(write); }, {}, false});
                    timing.Mark("deferred_write");
                    execution.cursor += count;
                    return Step::Progressed;
                }
                if (opcode == 0x37 || opcode == 0x40 || opcode == 0x50 || opcode == 0x42 || opcode == 0x46 || opcode == 0x49 || opcode == 0x58 || header == FlipPacketHeader) {
                    const auto memoryTransfer = opcode == 0x37 || opcode == 0x40 || opcode == 0x50;
                    if (current != nullptr) {
                        if (opcode == 0x58) {
                            postGraphics({[current] { current->AcquireGpuMemory(); }, {}, false});
                            timing.Mark("gpu_cache_barrier");
                        } else if (opcode == 0x42 || opcode == 0x46) {
                            postGraphics({[current] { current->RecordBarrier(); }, {}, false});
                            timing.Mark("gpu_barrier");
                        } else if (memoryTransfer) {
                            drainGraphics();
                            std::lock_guard gpuLock(gpuMutex);
                            std::uint64_t destination = 0, source = 0;
                            std::size_t destinationBytes = 0, sourceBytes = 0;
                            Pm4::TransferRanges(packet, destination, destinationBytes, source, sourceBytes);
                            current->ResolveGpuWrites(destination, destinationBytes);
                            current->ResolveGpuWrites(source, sourceBytes);
                            timing.Mark("transfer_resolve");
                        } else if (header == FlipPacketHeader && AsyncFlips()) {
                        } else {
                            const auto scope = header == FlipPacketHeader ? "Driver.FlipWait" : "Driver.ReleaseWait";
                            PerformanceTimer waitTiming(scope);
                            drainGraphics();
                            std::lock_guard gpuLock(gpuMutex);
                            current->WaitIdle();
                            timing.Mark("device_idle_wait");
                        }
                    }
                }
                if (header == RenderingWaitPacketHeader) {
                    submission.renderingWaits.at(cursor)->Wait();
                    timing.Mark("rendering_wait");
                } else if (header == FlipPacketHeader) {
                    CheckFailure();
                    timing.Mark("flip_prepare");
                } else if (opcode == 0x3c || opcode == 0x93) {
                    if (traceLabels && !execution.waitTraced) {
                        execution.waitTraced = true;
                        std::fprintf(stderr, "[label] wait 0x%llx function %u reference 0x%llx (queue 0x%x)\n", static_cast<unsigned long long>(static_cast<std::uint64_t>(packet[2]) | (static_cast<std::uint64_t>(packet[3]) << 32u)), packet[1] & 7u, static_cast<unsigned long long>(((packet[0] >> 8u) & 0xffu) == 0x93 ? (static_cast<std::uint64_t>(packet[4]) | (static_cast<std::uint64_t>(packet[5]) << 32u)) : packet[4]), submission.queue);
                    }
                    if (!Pm4::TryWait(packet, [this](std::uint64_t address, std::uint32_t bytes, std::uint64_t& value) { return lookupDeferred(address, bytes, value); })) return Step::Blocked;
                    execution.waitTraced = false;
                    timing.Mark("label_wait");
                } else if (opcode == 0x15) {
                    SkippingFailedPrograms([&] { dispatch(queue, packet, submission); });
                } else if (opcode == 0x16) {
                    std::array<std::uint32_t, 5> direct;
                    {
                        drainGraphics();
                        std::lock_guard gpuLock(gpuMutex);
                        const GuestMemory::MemoryAccessScope memoryScope(device.get(), [](void* context, std::uint64_t address, std::size_t bytes, bool writable) {
                            if (context) static_cast<VulkanDevice*>(context)->ResolveMemory(address, bytes, writable);
                        });
                        direct = Pm4::ResolveDispatch(packet, queue);
                    }
                    journalIndirect = true;
                    SkippingFailedPrograms([&] { dispatch(queue, direct, submission); });
                    journalIndirect = false;
                } else if (opcode == 0x35 || opcode == 0x2d || opcode == 0x27 || opcode == 0x24 || opcode == 0x25) {
                    SkippingFailedPrograms([&] { draw(queue, packet, submission); });
                } else if (opcode == 0x50 && (Pm4::DmaGdsDestination(packet) || Pm4::DmaGdsSource(packet))) {
                    drainGraphics();
                    std::lock_guard gpuLock(gpuMutex);
                    require(device != nullptr, "GDS transfer without a device");
                    const GuestMemory::MemoryAccessScope memoryScope(device.get(), [](void* context, std::uint64_t address, std::size_t bytes, bool writable) {
                        if (context) static_cast<VulkanDevice*>(context)->ResolveMemory(address, bytes, writable);
                    });
                    device->GdsTransfer(packet);
                    timing.Mark("gds_transfer");
                } else if (opcode != 0x42 && opcode != 0x58 && (opcode != 0x46 || Pm4::EventWritesMemory(packet))) {
                    graphicsDevice = currentDevice();
                    const GuestMemory::MemoryAccessScope memoryScope(this, &Driver::resolveForHost, &Driver::quietForHost);
                    Pm4::Execute(packet, queue);
                    timing.Mark("pm4_execute");
                    if (opcode == 0x49 && ((packet[2] >> 24u) & 7u) != 0) {
                        AgcDriver::Eq::Trigger(packet[7] & 0x7ffffffu);
                        timing.Mark("release_interrupt");
                    }
                }
            }
            if (header == FlipPacketHeader) {
                frameTiming->SetFlip(submission.serial, cursor, submission.received, FrameTiming::Clock::now());
                const auto completedFrame = std::exchange(frameTiming, nullptr);
                auto flip = submission.flips.at(cursor);
                if (const auto current = currentDevice(); current != nullptr && AsyncFlips()) {
                    postGraphics({[current, flip, completedFrame] {
                        current->FlushDraws();
                        flip->GpuReady(completedFrame);
                    }, {}, false});
                } else {
                    flip->GpuReady(completedFrame);
                }
            }
            execution.cursor += count;
        }
        return execution.cursor >= submission.commands.size() ? Step::Finished : Step::Progressed;
    }

    void complete(const Submission& submission) {
        {
            static const bool traceTiming = std::getenv("ANYPS5_TRACE_TIMING") != nullptr;
            static auto lastReport = FrameTiming::Clock::now();
            const auto now = FrameTiming::Clock::now();
            if (traceTiming && frameTiming != nullptr && now - lastReport > std::chrono::seconds(3)) {
                lastReport = now;
                frameTiming->PrintPartial("no flip for 3 s");
            }
        }
        {
            PerformanceContext timingContext(frameTiming.get());
            PerformanceTimer timing("Driver.Completion");
            std::lock_guard lock(mutex);
            timing.Mark("mutex_wait");
            rethrowFailure();
            inFlight.erase(submission.serial);
            completed = inFlight.empty() ? dequeuedSerial : *inFlight.begin() - 1;
        }
        changed.notify_all();
    }

    bool gpuPending = false;
    std::thread graphicsThread;
    std::thread worker;

    void run() noexcept {
        preferPerformanceCores();
        try {
            for (;;) {
                {
                    PerformanceContext timingContext(frameTiming.get());
                    PerformanceTimer timing("Driver.Worker");
                    std::unique_lock lock(mutex);
                    timing.Mark("queue_mutex_wait");
                    const bool haveWork = std::any_of(queued.begin(), queued.end(), [](const auto& entry) { return !entry.second.empty(); });
                    if (!haveWork) {
                        if (gpuPending) changed.wait_for(lock, std::chrono::microseconds(500), [&] { return failure || stopping || !pending.empty(); });
                        else changed.wait(lock, [&] { return failure || stopping || !pending.empty(); });
                    }
                    timing.Mark("wait_for_submission");
                    rethrowFailure();
                    if (!haveWork && pending.empty() && (stopping || failure)) break;
                    while (!pending.empty()) {
                        Execution execution{std::move(pending.front())};
                        pending.pop_front();
                        execution.submission.dequeued = FrameTiming::Clock::now();
                        inFlight.insert(execution.submission.serial);
                        dequeuedSerial = std::max(dequeuedSerial, execution.submission.serial);
                        queued[execution.submission.queue].push_back(std::move(execution));
                    }
                }
                {
                    std::unique_lock gpuLock(gpuMutex, std::try_to_lock);
                    const auto current = currentDevice();
                    if (current == nullptr) gpuPending = false;
                    else if (gpuLock.owns_lock()) {
                        current->Collect();
                        gpuPending = graphicsPending() || current->HasPendingWork();
                    } else gpuPending = true;
                }
                bool progressed = false;
                bool anyQueued = false;
                FrameTiming::Clock::time_point oldestBlock = FrameTiming::Clock::time_point::max();
                for (auto& [id, fifo] : queued) {
                    if (fifo.empty()) continue;
                    anyQueued = true;
                    auto& execution = fifo.front();
                    const auto result = step(execution);
                    if (result == Step::Finished) {
                        if (const auto current = currentDevice(); current != nullptr && graphicsPosted != 0) postGraphics({[current] { current->FlushDraws(); }, {}, false});
                        Submission finished = std::move(execution.submission);
                        fifo.pop_front();
                        complete(finished);
                        progressed = true;
                    } else if (result == Step::Progressed) {
                        execution.blocked = false;
                        progressed = true;
                    } else {
                        if (!execution.blocked) {
                            execution.blocked = true;
                            execution.blockedSince = FrameTiming::Clock::now();
                        }
                        oldestBlock = std::min(oldestBlock, execution.blockedSince);
                    }
                }
                if (anyQueued && !progressed) {
                    require(FrameTiming::Clock::now() - oldestBlock < std::chrono::seconds(30), "WAIT_REG_MEM did not complete within 30 seconds (no queue or title thread releases the label)");
                    std::this_thread::sleep_for(std::chrono::microseconds(20));
                }
            }
            drainGraphics();
            std::lock_guard gpuLock(gpuMutex);
            std::lock_guard deviceLock(deviceMutex);
            device.reset();
            graphicsDevice.reset();
        } catch (...) {
            const auto error = std::current_exception();
            for (auto& [id, fifo] : queued) for (auto& execution : fifo) for (const auto& [offset, flip] : execution.submission.flips) flip->Fail(error);
            ReportFailure(error);
            {
                drainGraphics();
                std::lock_guard gpuLock(gpuMutex);
                std::lock_guard deviceLock(deviceMutex);
                device.reset();
                graphicsDevice.reset();
            }
        }
    }
};

}

void Submit(const Packet* packet, std::uint32_t queue) {
    Submit(packet, 1, queue);
}

void Submit(const Packet* packets, std::uint32_t count, std::uint32_t queue) {
    try {
        Driver::Get().Submit(packets, count, queue);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "AGC driver: submission failed on the title's thread: %s\n", error.what());
        GpuJournal::Dump("AGC driver: last GPU work before the failure, oldest first:");
        throw;
    }
}

void WaitIdle() {
    Driver::Get().WaitIdle();
}

void RegisterShader(const Shader* shader) {
    Driver::Get().RegisterShader(shader);
}

void SuspendPoint() {
    Driver::Get().SuspendPoint();
}

void RegisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    Driver::Get().RegisterVideoOutput(handle, output);
}

void UnregisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    Driver::Get().UnregisterVideoOutput(handle, output);
}

void PresentClear(const PresentationWindow& window, bool opaque, void (*gpuReady)(void*), void* context) {
    Driver::Get().Present(window, nullptr, opaque, gpuReady, context);
}

void PresentBuffer(const PresentationWindow& window, const DisplayBuffer& buffer, void (*gpuReady)(void*), void* context) {
    Driver::Get().Present(window, &buffer, true, gpuReady, context);
}

void ReleaseWindow(void* window) {
    Driver::Get().ReleaseWindow(window);
}

void ReportFailure(std::exception_ptr error) {
    Driver::Get().ReportFailure(error);
}

}

extern "C" void AgcDriverWaitIdle_nid_postfix() {
    AgcDriver::WaitIdle();
}

extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader) {
    AgcDriver::RegisterShader(shader);
}

extern "C" void AgcDriverSuspendPoint_nid_postfix() {
    AgcDriver::SuspendPoint();
}

extern "C" void AgcDriverRegisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    AgcDriver::RegisterVideoOutput(handle, output);
}

extern "C" void AgcDriverUnregisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    AgcDriver::UnregisterVideoOutput(handle, output);
}

extern "C" void AgcDriverPresentClear_nid_postfix(const AgcDriver::PresentationWindow& window, bool opaque, void (*gpuReady)(void*), void* context) {
    AgcDriver::PresentClear(window, opaque, gpuReady, context);
}

extern "C" void AgcDriverPresentBuffer_nid_postfix(const AgcDriver::PresentationWindow& window, const AgcDriver::DisplayBuffer& buffer, void (*gpuReady)(void*), void* context) {
    AgcDriver::PresentBuffer(window, buffer, gpuReady, context);
}

extern "C" void AgcDriverReleaseWindow_nid_postfix(void* window) {
    AgcDriver::ReleaseWindow(window);
}

extern "C" void AgcDriverReportFailure_nid_postfix(std::exception_ptr error) {
    AgcDriver::ReportFailure(error);
}

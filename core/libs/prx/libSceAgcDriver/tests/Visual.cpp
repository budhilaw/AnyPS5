#define SDL_MAIN_HANDLED
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanLibrary.hpp"
#include "prx/libc/include/GuestMemoryBacking.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::uint32_t Width = 640;
constexpr std::uint32_t Height = 480;
std::span<std::byte> Pixels;

std::span<std::byte> MapPixels() {
    constexpr int GuestReadWrite = 1 | 2;
    const auto pageSize = GuestMemoryTracking::GuestMemoryTrackingPageSize_nid_postfix();
    const auto bytes = (static_cast<std::size_t>(Width) * Height * 4 + pageSize - 1) / pageSize * pageSize;
    const auto alignment = std::max<std::size_t>(pageSize, 65536);
    auto* memory = GuestMemoryBacking::GuestMemoryBackingMap_nid_postfix(nullptr, bytes, alignment, GuestReadWrite);
    Require(memory != nullptr, "cannot map the guest pixel buffer");
    return {static_cast<std::byte*>(memory), static_cast<std::size_t>(Width) * Height * 4};
}

ShaderRecompiler::RecompileResult LoadShader(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    Require(file.is_open(), "cannot open test SPIR-V: " + path.string());
    const auto size = file.tellg();
    Require(size >= 20 && size <= 1024 * 1024 && size % 4 == 0, "invalid test SPIR-V size");
    ShaderRecompiler::RecompileResult result;
    auto& words = result.spirv.Edit();
    words.resize(static_cast<std::size_t>(size) / 4);
    file.seekg(0);
    Require(static_cast<bool>(file.read(reinterpret_cast<char*>(words.data()), size)), "cannot read test SPIR-V");
    return result;
}

void DumpPixels(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    Require(file.is_open(), "cannot write readback image: " + path.string());
    file << "P6\n" << Width << ' ' << Height << "\n255\n";
    for (std::size_t i = 0; i < Pixels.size(); i += 4) file.write(reinterpret_cast<const char*>(&Pixels[i]), 3);
    Require(static_cast<bool>(file), "cannot write readback image: " + path.string());
}

void Run(SDL_Window* window, const std::filesystem::path& directory, bool verifyOnly, const std::filesystem::path& dumpPath) {
    unsigned count = 0;
    Require(SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr) == SDL_TRUE, SDL_GetError());
    std::vector<const char*> extensions(count);
    Require(SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data()) == SDL_TRUE, SDL_GetError());
    const AgcDriver::PresentationWindow presentation{window, extensions, [](void* context, VkInstance instance) {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        Require(SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(context), instance, &surface) == SDL_TRUE, SDL_GetError());
        return surface;
    }, [](void* context, std::uint32_t* width, std::uint32_t* height) {
        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_Vulkan_GetDrawableSize(static_cast<SDL_Window*>(context), &drawableWidth, &drawableHeight);
        *width = drawableWidth > 0 ? static_cast<std::uint32_t>(drawableWidth) : 0;
        *height = drawableHeight > 0 ? static_cast<std::uint32_t>(drawableHeight) : 0;
    }, Width, Height};
    AgcDriver::VulkanDevice device(&presentation);
    Pixels = MapPixels();
    auto vertex = LoadShader(directory / "Triangle.vert.spv");
    auto fragment = LoadShader(directory / "Triangle.frag.spv");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderRecompiler::ShaderStage::Vertex, &vertex, 0},
        {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}
    }};
    for (std::size_t i = 0; i < Pixels.size(); i += 4) {
        Pixels[i] = std::byte{16};
        Pixels[i + 1] = std::byte{24};
        Pixels[i + 2] = std::byte{40};
        Pixels[i + 3] = std::byte{255};
    }
    AgcDriver::Graphics::State state{};
    state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size()};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.scissor = {{0, 0}, {Width, Height}};
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    const std::array<std::uint16_t, 3> indices{0, 1, 2};
    const AgcDriver::Pm4::DrawParameters draw{reinterpret_cast<std::uintptr_t>(indices.data()), 3, 2, 1, 0};
    device.Draw(state, draw, shaders);
    const std::vector<std::byte> indexedPixels(Pixels.begin(), Pixels.end());
    for (std::size_t i = 0; i < Pixels.size(); i += 4) {
        Pixels[i] = std::byte{16};
        Pixels[i + 1] = std::byte{24};
        Pixels[i + 2] = std::byte{40};
        Pixels[i + 3] = std::byte{255};
    }
    const AgcDriver::Pm4::DrawParameters autoDraw{0, 3, 0, 1, 0, false};
    device.Draw(state, autoDraw, shaders);
    Require(std::equal(Pixels.begin(), Pixels.end(), indexedPixels.begin()), "GPU readback: auto draw differs from indexed triangle");
    const auto center = (Height / 2 * Width + Width / 2) * 4;
    Require(std::to_integer<unsigned>(Pixels[center]) > 30 && std::to_integer<unsigned>(Pixels[center + 1]) > 30 && std::to_integer<unsigned>(Pixels[center + 2]) > 30, "GPU readback: triangle center was not rendered");
    Require(Pixels[0] == std::byte{16} && Pixels[1] == std::byte{24} && Pixels[2] == std::byte{40}, "GPU readback: background changed");
    if (!dumpPath.empty()) DumpPixels(dumpPath);
    device.PresentPixels(Width, Height, Pixels);
    std::cout << "SPIR-V triangle rendered, GPU readback verified, frame queued for presentation. Close the window or press Escape.\n" << std::flush;
    if (!verifyOnly) {
        bool running = true;
        while (running) {
            SDL_Event event{};
            if (SDL_WaitEventTimeout(&event, 100)) {
                if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) running = false;
                if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED) device.PresentPixels(Width, Height, Pixels);
            }
        }
    }
    device.WaitIdle();
}

}

int main(int argc, char** argv) {
    try {
        const bool verifyOnly = argc >= 2 && std::string_view(argv[1]) == "--verify";
        const bool dump = argc == 4 && std::string_view(argv[2]) == "--dump";
        Require(argc == 1 || (verifyOnly && (argc == 2 || dump)), "usage: agc_driver_visual_test [--verify [--dump <readback.ppm>]]");
        SDL_SetMainReady();
        Require(SDL_Init(SDL_INIT_VIDEO) == 0, SDL_GetError());
        AgcDriver::ResolveVulkanLibrary();
        {
            const auto window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>(SDL_CreateWindow("AnyPS5 AGC - SPIR-V triangle test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Width, Height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN), SDL_DestroyWindow);
            Require(window != nullptr, SDL_GetError());
            const auto base = std::unique_ptr<char, decltype(&SDL_free)>(SDL_GetBasePath(), SDL_free);
            Require(base != nullptr, SDL_GetError());
            Run(window.get(), std::filesystem::path(base.get()), verifyOnly, dump ? std::filesystem::path(argv[3]) : std::filesystem::path{});
        }
        SDL_Quit();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
}

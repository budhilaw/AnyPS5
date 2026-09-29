# About

Tool for automatic executables porting to Linux and Windows.

Includes a [relinker](core/relinker) that converts executable to the target system's native format and implementations of [system prx libraries](core/libs/prx) suitable for dynamic linking. No emulation or separate runtime process.

Releases will be published after the first full successful launch of at least one game.

## Status

Execution reaches `_start`, [stack unwinding](core/libs/prx/libc/src/exception/Unwind.cpp) and exception handling tables are built, reaches main. Unsupported or unexpected states strictly throw `std::runtime_error`. `what()` is printed to stderr and the process terminates.

The [shader recompiler](core/shader/recompiler/Recompiler.cpp) successfully produces validated via [Spirv-Tools](3rdparty/SPIRV-Tools) SPIR-V.

The real game reaches the logo, main menu, and [gameplay](https://gist.github.com/user-attachments/assets/81d28e9b-c237-4545-b2ca-720071129816) with audio.

[Technical debt of the project](docs/TechnicalDebt.md), [code style conventions](docs/CONVENTIONS.md)

## Inputs

The relinker takes a plain PS5 ELF or a fake-signed SELF (the `eboot.bin` and `.prx`/`.sprx` files that console-side dumpers produce). A fake-signed SELF is unwrapped automatically; `relinker --unself <in> <out.elf>` only unwraps. Encrypted SELF files are refused: decrypting them requires the console.

## Build

The relinker uses only the C++20 standard library and should build with any conforming compiler.

[libc.prx](core/libs/prx/libc) implementations contain compiler-specific code. Linux builds work with GCC; on Windows, MinGW-w64 GCC 15.2.0 (`winlibs-gcc15`, `x86_64-ucrt-posix-seh`) is currently required.

The project targets maximum compiler portability. Support for additional compilers will be addressed after the first successful game launch.

### macOS (Apple Silicon under Rosetta 2, or Intel)

The whole stack builds on macOS with Apple Clang: the relinker emits Mach-O executables and dylibs (`relinker --macos`), the system libraries and the [AGC Vulkan driver](core/libs/prx/libSceAgcDriver) render on Metal through [MoltenVK](https://github.com/KhronosGroup/MoltenVK). On Apple Silicon the guest's x86-64 code runs under Rosetta 2, so the libraries are built for x86-64 and everything lives in one translated process. Hades (PPSA03355) runs to gameplay with audio on an Apple M4 MacBook Air.

Requirements:

- Xcode Command Line Tools (`xcode-select --install`), CMake and Ninja (`brew install cmake ninja`).
- Rosetta 2 on Apple Silicon: `softwareupdate --install-rosetta --agree-to-license`.
- The [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home#mac) for macOS. Its loader and MoltenVK are universal binaries; Homebrew's `molten-vk` is arm64 only and cannot load into the x86-64 process.

Build:

```sh
git submodule update --init --recursive
# native build: relinker and tests
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON -DAGC_BUILD_VISUAL_TEST=ON
ninja -C build relinker && ctest --test-dir build
# x86-64 libraries the title loads
cmake -S . -B build-x64 -G Ninja -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_BUILD_TYPE=Release
ninja -C build-x64 libs
```

Preparing a title from its dump (`/path/to/dump` holds `eboot.bin`, `sce_sys` and the title's own modules):

```sh
mkdir -p game/libs
./build/core/relinker/relinker --macos unused-filter=2 /path/to/dump/eboot.bin game/eboot
# every module the title ships next to eboot.bin (Hades: libfmod.prx, libfmodstudio.prx)
for module in /path/to/dump/*.prx; do ./build/core/relinker/relinker --macos "$module" "game/libs/$(basename "$module")"; done
ln -s "$PWD"/build-x64/core/libs/libs/*.prx game/libs/
ln -s /path/to/dump game/app0
```

The system modules in the dump's `sce_module` directory are not relinked: the libraries built here replace them.

Running:

```sh
cd game && VULKAN_SDK=$HOME/VulkanSDK/<version>/macOS ./eboot
```

- `VULKAN_SDK` points at the SDK's `macOS` directory; the driver loads its Vulkan loader and MoltenVK ICD from there (`SDL_VULKAN_LIBRARY` names another loader).
- `ANYPS5_NP_STUB_SUCCESS=1` makes the PSN (NP) stubs report success, for titles that stop when signing in fails (Hades).
- A connected game controller works through SDL. The keyboard fallback is defined in [InputMapping.hpp](core/libs/prx/libScePad/include/InputMapping.hpp) (Enter or Space is Cross, Escape is Options, F11 toggles fullscreen). Closing the window ends the title.
- The window title shows the frame rate. The first launch compiles every shader it meets; `pipeline.cache` and `shader-requests.cache` next to the executable keep that work, so later launches load in seconds.

Tests: `ctest --test-dir build` runs the guest runtime and relinker tests. The driver tests are built on demand, for example `ninja -C build agc_driver_graphics_tests agc_driver_pm4_tests agc_driver_bda_device_tests video_out_flip_tests` and then the executables under `build/core/libs/prx`; the Vulkan tests also need `VULKAN_SDK`, and `video_out_flip_tests present` needs an `app0/sce_sys/param.json` in its working directory.

Diagnostics and caches (any platform): the driver persists its Vulkan pipeline cache as `pipeline.cache` next to the executable (`ANYPS5_PIPELINE_CACHE=<file>` moves it, an empty value disables it), so the host's shader compiles survive a restart; `ANYPS5_BUFFER_MIRROR_MB` and `ANYPS5_TEXTURE_CACHE_MB` size the GPU mirrors of guest buffers and textures (3 GiB and 4 GiB by default). `ANYPS5_TRACE_LABELS=1` logs every GPU label release, wait, label write and submission, `ANYPS5_TRACE_TEXTURES=1` logs texture cache decisions, `ANYPS5_TRACE_UNWIND=1` traces the guest exception unwinder, `ANYPS5_TRACE_TIMING=1` prints the running frame's timing metrics every few seconds while a title renders without flipping, `ANYPS5_TRACE_PROTECT=1` logs guest memory protection changes, `ANYPS5_TRACE_AJM=1` logs audio decoder jobs, `ANYPS5_DUMP_AUDIO=<directory>` writes the decoded and mixed audio streams, and `ANYPS5_DUMP_SLOW_SHADERS=<directory>` writes the SPIR-V of pipelines whose host compile took longer than 100 ms (they are logged in any case). Log lines carry the seconds since the process started.

macOS specifics of the runtime: the executable is not position independent (dyld's 4 KiB slides would break the guest's 16 KiB page arithmetic); guest thread storage is rewritten from `%fs:0` to a `%gs` slot that libkernel fills per thread; the guest's `main` runs on a secondary thread while the process main thread serves Cocoa; SDL is one shared library so AppKit sees a single application. The driver enables `VK_KHR_portability_subset` automatically and degrades the optional features MoltenVK lacks (mesh shaders, fragment shader barycentrics).

## Disclaimer

This project is intended for interoperability, research, preservation, and compatibility purposes. It does not include, distribute, or require copyrighted software, firmware, cryptographic keys, or proprietary libraries. Users are responsible for ensuring that any binaries used with this project are obtained and used in accordance with applicable laws and their respective license terms.

## License

This project is licensed under the GNU General Public License version 2 only.

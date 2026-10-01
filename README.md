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
- A connected game controller works through SDL. The keyboard fallback is defined in [InputMapping.hpp](core/libs/prx/libScePad/include/InputMapping.hpp) (Enter or Space is Cross, Escape is Options, F11 toggles fullscreen, F9 toggles the frame-rate cap). Closing the window ends the title.
- The window title shows the frame rate, the emulated output (`60 Hz` or `120 Hz`, plus `VRR` with `ANYPS5_DISPLAY=vrr`) and the pacing (`PS5 pacing` or `uncapped`). The first launch compiles every shader it meets; `pipeline.cache` and `shader-requests.cache` next to the executable keep that work, so later launches load in seconds.
- Frame rate switches (any platform): the video output reads the first four variables below once, and a value it does not accept stops the title with a message that names the variable and the accepted values. The log's `frame pacing:` line shows the values read. The pacing in effect is in the window title and in the `frame rate:` lines, which appear when a run starts uncapped, after each F9 press and when `ANYPS5_UNCAPPED=1` is ignored; each `swapchain:` line shows the host present mode in use. [UnchartedStatus.md](docs/UnchartedStatus.md#frame-rate) shows what they do for one title, with recommended setups.
  - `ANYPS5_DISPLAY=60hz|120hz|vrr` (default `60hz`) is the TV the emulated PS5 is connected to. `60hz` offers only 59.94 Hz. `120hz` also offers 119.88 Hz, and the vblank switches to 119.88 Hz when the title selects it. `vrr` is `120hz` on a VRR display, reported as capable and active for the whole run: flips are shown as soon as they are ready, but no sooner than the flip rate allows, and flips the title pegs with `sceVideoOutVrrPegToFixedRate` keep the vblank grid.
  - `ANYPS5_UNCAPPED=0|1` (default `0`): `1` starts with the frame-rate cap off, and F9 toggles the cap during a run. Uncapped flips are shown as soon as they are ready instead of waiting for the vblank and the flip rate the title set. Pegged and immediate flips keep their own rules, a minimized window gets PS5 pacing, and the vblank rate, vblank events and guest clocks do not change.
  - `ANYPS5_FPS_LIMIT=<fps>` is the most flips per second while uncapped, `0` for no limit. The default is the monitor's refresh rate up to 120 (120 when SDL cannot read it, see the `display refresh rate` log line), and never below the emulated vblank rate. A limit above 120, or none, logs a warning, because 120 fps is the highest frame rate a PS5 build is tested at.
  - `ANYPS5_PRESENT_MODE=auto|fifo|mailbox|immediate|relaxed` (default `auto`) is the host swapchain present mode. `auto` picks FIFO only while the cap is on, no immediate flip has arrived and the monitor refreshes at least twice as fast as the emulated output's top rate (60 Hz, or 120 Hz with `120hz` and `vrr`; within 1 Hz, and an unknown refresh rate counts as 60 Hz). Otherwise it picks MAILBOX, so the host present, which holds the driver's guest memory lock, does not wait for the monitor. A mode the surface lacks falls back toward FIFO, never toward tearing, so only `immediate` and `relaxed` can tear. FIFO and FIFO_RELAXED swapchains get max(minImageCount + 1, 3) images, MAILBOX and IMMEDIATE max(minImageCount, 3). While uncapped flips are presented with FIFO or FIFO_RELAXED, the limit is lowered to 0.97 x the refresh rate if it is higher, and the log says uncapped is degraded.
  - Setting `ANYPS5_LEGACY_TSC`, to any value, makes `sceKernelReadTsc` and `sceKernelGetTscFrequency` report the old 1 GHz nanosecond clock instead of the host TSC rate libkernel measures when it loads, for A/B checks. Guest RDTSC timing then runs at the wrong speed, so `ANYPS5_UNCAPPED` and F9 are ignored.

Tests: `ctest --test-dir build` runs the guest runtime and relinker tests. The driver tests are built on demand, for example `ninja -C build agc_driver_graphics_tests agc_driver_pm4_tests agc_driver_bda_device_tests video_out_flip_tests` and then the executables under `build/core/libs/prx`; the Vulkan tests also need `VULKAN_SDK`, the `video_out_flip_tests` subcommands that present flips (`present*`, `vrr` and `uncapped*`) need an `app0/sce_sys/param.json` in their working directory, and `video_out_flip_tests uncappedlegacy` needs `ANYPS5_LEGACY_TSC` set.

Diagnostics and caches (any platform): the driver persists its Vulkan pipeline cache as `pipeline.cache` next to the executable (`ANYPS5_PIPELINE_CACHE=<file>` moves it, an empty value disables it), so the host's shader compiles survive a restart; `ANYPS5_BUFFER_MIRROR_MB` and `ANYPS5_TEXTURE_CACHE_MB` size the GPU mirrors of guest buffers and textures (3 GiB and 4 GiB by default). `ANYPS5_TRACE_LABELS=1` logs every GPU label release, wait, label write and submission, `ANYPS5_TRACE_TEXTURES=1` logs texture cache decisions, `ANYPS5_TRACE_UNWIND=1` traces the guest exception unwinder, `ANYPS5_TRACE_TIMING=1` prints the running frame's timing metrics every few seconds while a title renders without flipping, `ANYPS5_TRACE_PROTECT=1` logs guest memory protection changes, `ANYPS5_TRACE_AJM=1` logs audio decoder jobs, `ANYPS5_DUMP_AUDIO=<directory>` writes the decoded and mixed audio streams, and `ANYPS5_DUMP_SLOW_SHADERS=<directory>` writes the SPIR-V of pipelines whose host compile took longer than 100 ms (they are logged in any case). Log lines carry the seconds since the process started.

macOS specifics of the runtime: the executable is not position independent (dyld's 4 KiB slides would break the guest's 16 KiB page arithmetic); guest thread storage is rewritten from `%fs:0` to a `%gs` slot that libkernel fills per thread; the guest's `main` runs on a secondary thread while the process main thread serves Cocoa; SDL is one shared library so AppKit sees a single application. The driver enables `VK_KHR_portability_subset` automatically and degrades the optional features MoltenVK lacks (mesh shaders, fragment shader barycentrics).

## Disclaimer

This project is intended for interoperability, research, preservation, and compatibility purposes. It does not include, distribute, or require copyrighted software, firmware, cryptographic keys, or proprietary libraries. Users are responsible for ensuring that any binaries used with this project are obtained and used in accordance with applicable laws and their respective license terms.

## License

This project is licensed under the GNU General Public License version 2 only.

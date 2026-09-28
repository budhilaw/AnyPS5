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

The whole stack builds on macOS with Apple Clang: the relinker emits Mach-O executables and dylibs (`relinker --macos`), the system libraries and the [AGC Vulkan driver](core/libs/prx/libSceAgcDriver) render on Metal through [MoltenVK](https://github.com/KhronosGroup/MoltenVK). On Apple Silicon the guest's x86-64 code runs under Rosetta 2, so the libraries are built for x86-64 and everything lives in one translated process.

```sh
brew install cmake ninja
git submodule update --init --recursive
# native build: relinker, tests, headless GPU checks
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON -DAGC_BUILD_VISUAL_TEST=ON
ninja -C build relinker libs && ctest --test-dir build
# x86-64 libraries for a game (Apple Silicon: install Rosetta 2 and the Vulkan SDK's universal MoltenVK)
cmake -S . -B build-x64 -G Ninja -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_BUILD_TYPE=Release
ninja -C build-x64 libs
```

Running a title:

```sh
./build/core/relinker/relinker --macos unused-filter=2 eboot.bin game/eboot        # fake-signed SELF or ELF
./build/core/relinker/relinker --macos Il2CppUserAssemblies.prx game/libs/Il2CppUserAssemblies.prx   # each guest module
ln -s /path/to/build-x64/core/libs/libs/*.prx game/libs/                           # runtime libraries
ln -s "/path/to/dump" game/app0                                                    # the title's app0
cd game && VULKAN_SDK=$HOME/VulkanSDK/<version>/macOS ./eboot
```

Diagnostics and caches (any platform): the driver persists its Vulkan pipeline cache as `pipeline.cache` next to the executable (`ANYPS5_PIPELINE_CACHE=<file>` moves it, an empty value disables it), so the host's shader compiles survive a restart; `ANYPS5_BUFFER_MIRROR_MB` and `ANYPS5_TEXTURE_CACHE_MB` size the GPU mirrors of guest buffers and textures (3 GiB and 4 GiB by default). `ANYPS5_TRACE_LABELS=1` logs every GPU label release, wait, label write and submission, `ANYPS5_TRACE_TEXTURES=1` logs texture cache decisions, `ANYPS5_TRACE_UNWIND=1` traces the guest exception unwinder, `ANYPS5_TRACE_TIMING=1` prints the running frame's timing metrics every few seconds while a title renders without flipping, `ANYPS5_TRACE_PROTECT=1` logs guest memory protection changes, and `ANYPS5_DUMP_SLOW_SHADERS=<directory>` writes the SPIR-V of pipelines whose host compile took longer than 100 ms (they are logged in any case). Log lines carry the seconds since the process started.

macOS specifics of the runtime: the executable is not position independent (dyld's 4 KiB slides would break the guest's 16 KiB page arithmetic); guest thread storage is rewritten from `%fs:0` to a `%gs` slot that libkernel fills per thread; the guest's `main` runs on a secondary thread while the process main thread serves Cocoa; SDL is one shared library so AppKit sees a single application. The driver locates MoltenVK through `VULKAN_SDK` or `SDL_VULKAN_LIBRARY`, enables `VK_KHR_portability_subset` automatically and degrades the optional features MoltenVK lacks (mesh shaders, fragment shader barycentrics).

## Disclaimer

This project is intended for interoperability, research, preservation, and compatibility purposes. It does not include, distribute, or require copyrighted software, firmware, cryptographic keys, or proprietary libraries. Users are responsible for ensuring that any binaries used with this project are obtained and used in accordance with applicable laws and their respective license terms.

## License

This project is licensed under the GNU General Public License version 2 only.

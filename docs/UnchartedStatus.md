# Uncharted: Legacy of Thieves Collection (PPSA05684)

State of branch `feat/macos-uncharted` on 2026-09-30, written so the work can continue on another machine.

## Where it stands (macOS, Apple M4, MoltenVK)

- Boots, plays the intro movie with audio, renders the Language and HUD setup menus with text, and reaches the game selector with its 3D scene. The menus are navigable up to New Game, which still shows a black screen.
- The selector image is wrong: it is blue and blocky, and the lighting output is very dark. The blue comes from a half-resolution fog chain built from two history depth buffers that stay about 90% NaN: only two draws per frame (stencil reference 33) write them.
- Speed: about 3-4 fps in the menus. Some recompiled compute shaders are 150K-350K SPIR-V words and take 1-10 s each to compile on first use.
- The "Invalid Resource" GPU crash (`kIOGPUCommandBufferCallbackErrorInvalidResource`) that killed every run within 5-190 s is fixed; a 300 s run showed no GPU errors.

## Running it

Build and relink as described in the README. The run folder holds the relinked `eboot`, `libs/*.prx` (links to `build-x64/core/libs/libs`) and `app0` (a link to the dump).

- Rebuild the modules the game loads with `ninja -C <build> libs`. The default target does not rebuild the `.prx` files.
- Environment: `ANYPS5_SKIP_FAILED_PROGRAMS=1` skips GPU programs that fail to prepare instead of stopping, and `ANYPS5_NP_STUB_SUCCESS=1` makes the PSN stubs report success.
- A save profile in `_sd` (the English language and HUD choices) skips the first-run screens.
- If startup stalls for minutes at the first flip, MoltenVK is recompiling every library stored in `pipeline.cache` because the system Metal cache went cold. Move `pipeline.cache` aside.

## Debug switches added on this branch

- `ANYPS5_DEBUG_INPUT_FILE=<file>`: presses pad buttons from lines appended to the file, for example `cross:0.5,wait:2,down`.
- `ANYPS5_DUMP_DISPLAY_TRIGGER=<file>` / `ANYPS5_DUMP_TARGETS_TRIGGER=<file>`: touching the file dumps the displayed image, or every color and depth target plus the GPU journal.
- `ANYPS5_DUMP_DISPLAY_AT=t1,t2:prefix`: dumps the display at the given seconds.
- `ANYPS5_GPU_JOURNAL_DRAWS=1`, `ANYPS5_JOURNAL_TEXTURES=1`: the GPU journal printed on a driver failure also lists draws (with stencil state and depth addresses) and the textures each dispatch binds.
- `ANYPS5_DEBUG_GPU_LABELS=1`: inserts a debug label after every draw and dispatch (visible in GPU captures and command buffer error reports).
- `ANYPS5_DEBUG_SLOW_GPU=<ms>`: serializes GPU work and logs draws and dispatches slower than the threshold.
- `ANYPS5_DEBUG_STENCIL_PROBE=<depth address>`, `ANYPS5_DEBUG_REPORT_STENCILED=1`: stencil histograms and before/after pixel diffs.
- `ANYPS5_DUMP_COMPUTE_SPIRV=<dir>`, `ANYPS5_DUMP_SLOW_SHADERS=<dir>`: write compute SPIR-V.
- `ANYPS5_NO_HOST_IMPORT=1`, `ANYPS5_NO_HOST_VERTEX=1`: disable importing guest memory as GPU buffers.

## What the crash fix changed (all platforms unless noted)

- GPU objects (buffers, images, views, memory, pipelines, descriptor pools) are destroyed through a release queue once a fence submitted after the release has signalled.
- Guest memory is imported for vertex, index and buffer data in 32 MiB windows with a 2 GiB budget, instead of whole mappings. Metal pinned every imported byte, and whole-mapping imports pinned about 12 GB.
- Buffer device address (BDA) page tables only map windows that shaders have touched. A BDA access to an unmapped address inside registered GPU memory learns its window. Accesses outside guest memory are logged and the invocation stops instead of the driver failing.
- macOS only: Metal command buffers retain the resources they bind (`RetainMetalCommandReferences`), because MoltenVK 1.4.2 creates them unretained next to its residency set.

## Where it stands (Windows, i5-12400F, RTX 3070)

- Builds with MinGW-w64 GCC 15.2 and runs from the relinked `eboot.exe`. Guest physical memory is one pagefile-backed section mapped into placeholders in 1 MiB views, so the title's defragmentation (1 MiB `munmap` pieces remapped with `sceKernelBatchMap2`) costs about 15 µs per unmap.
- Speed: 18-20 fps in the first scenes (60-75 s after start), about 4 fps while compute pipelines of about 9700 dwords compile in 200 ms each.
- Crash (blocker): about two minutes in, a game leaf function reads a pointer it kept in the SysV red zone and gets zero. Windows has no red zone: when a guest thread takes a page fault on memory the driver tracks, the kernel writes the exception record just below `rsp` and overwrites the 128 bytes the function still uses. Linux and macOS skip the red zone when they deliver a signal. A run saw 14,000 such faults, mostly while loading. The fix is to patch every guest instruction that can fault while the red zone holds live data so it runs with `rsp` lowered by 128 (shadPS4 and the Kyty fork do this with Zydis). Fewer tracking faults only makes the crash rarer.
- Movies are black: `libSceVdecsw` only decodes with VideoToolbox (macOS).

## Windows findings

- MinGW winpthreads rounds timed waits up to the 15.6 ms system tick even with `timeBeginPeriod(1)`, and `sleep_for` below 1 ms returns at once. Guest sleeps, condition variable, event flag, semaphore and equeue timeouts go through high-resolution waitable timers (`PreciseSleep`, `PreciseWait`).
- `VirtualQuery` walks page tables and costs milliseconds on multi-GiB views; guest range checks consult the allocation registry first, and watches keep their original protections instead of querying them again.
- Remapping a view unmaps it for a moment. A guest thread that faults on it waits for the remap and retries; the retry budget restarts after every remap.
- Fatal exception reports print the state and protection of the faulting page.

## Debug switches for performance work

- `ANYPS5_SAMPLE_THREADS=1`: samples the title, worker and graphics threads every millisecond (Windows) and prints the hottest functions and call chains every 30 s.
- `ANYPS5_TRACE_SLOW_OPS=1`: prints 5 s summaries of lock waits and slow memory, tracking and device operations.
- `ANYPS5_TRACE_TIMING=1`: per-frame timing of the driver stages and GPU timestamps per command batch.

## Next steps

1. Windows: protect the guest red zone (see the crash above), then cut the tracking faults and the full pipeline barriers around every compute dispatch.
2. Selector image: find out why the history depth buffers (`0x8463400000` / `0x8464710000` in the logs) are never filled, then fix the dark lighting and the blocky tiles.
3. Continue past New Game into gameplay. Watch for GPU-culled indirect draw arguments exceeding the index buffer, a hint from the Kyty fork (github.com/budhilaw/Kyty, branch `feat/ps5-gameplay-fixes`).
4. Performance target: 30 fps in gameplay at a 720p internal resolution. That needs render scaling, smaller recompiled shaders and less per-draw CPU work.
5. The graphics unit tests stop at a stale "pitch" expectation, and the minimum-LOD case changed from an error to a log.

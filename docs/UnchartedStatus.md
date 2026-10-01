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
- Environment: `ANYPS5_SKIP_FAILED_PROGRAMS=1` skips GPU programs that fail to prepare and draws whose register state cannot be translated instead of stopping, and `ANYPS5_NP_STUB_SUCCESS=1` makes the PSN stubs report success.
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
- `ANYPS5_DEPTH_COPY=1`: samples depth and stencil buffers through copies instead of read-only views of the resident depth image. With `ANYPS5_TRACE_TIMING=1`, `Graphics.TextureCache` counts `depth_view` and `depth_view_hit` for the views, `depth_copy` and `depth_hit` for copies, and `depth_feedback_copy` for draws that sample the depth buffer they render to.

## What the crash fix changed (all platforms unless noted)

- GPU objects (buffers, images, views, memory, pipelines, descriptor pools) are destroyed through a release queue once a fence submitted after the release has signalled.
- Guest memory is imported for vertex, index and buffer data in 32 MiB windows with a 2 GiB budget, instead of whole mappings. Metal pinned every imported byte, and whole-mapping imports pinned about 12 GB.
- Buffer device address (BDA) page tables only map windows that shaders have touched. A BDA access to an unmapped address inside registered GPU memory learns its window. Accesses outside guest memory are logged and the invocation stops instead of the driver failing.
- macOS only: Metal command buffers retain the resources they bind (`RetainMetalCommandReferences`), because MoltenVK 1.4.2 creates them unretained next to its residency set.

## Where it stands (Windows, i5-12400F, RTX 3070)

- Builds with MinGW-w64 GCC 15.2 and runs from the relinked `eboot.exe`. Guest physical memory is one pagefile-backed section mapped into placeholders in 1 MiB views, so the title's defragmentation (1 MiB `munmap` pieces remapped with `sceKernelBatchMap2`) costs about 15 µs per unmap.
- Speed: 18-23 fps in the first scenes, 6-9 fps in the Language menu. In the menu the graphics thread is saturated: about 150 dispatches and 600 draws per frame, large GPU-written surfaces (2560x1440) revalidated byte by byte every frame because the texture descriptor does not match the resident render target, and depth textures copied through a buffer each time the depth changes.
- Crash: a game leaf function (`eboot+0x15bb5c0`) writing per-frame records keeps its object pointer in the SysV red zone. Windows has no red zone: when a guest thread takes a page fault on memory the driver tracks, the kernel writes the exception record just below `rsp` and overwrites the 128 bytes the function still uses. Linux and macOS skip the red zone when they deliver a signal. The fault that killed it hit a stale 64 KiB texture watch on memory the game had reused; watches no texture or buffer used for 250 ms are now released, and three 3-4 minute runs finished without a crash. Any remaining fault in such a function can still corrupt it; the full fix is to patch the guest instructions that can fault while the red zone holds live data so they run with `rsp` lowered by 128 (shadPS4 and the Kyty fork do this).
- Colors: the title presents 10:10:10:2 display buffers (pixel format `0x8100000000000000`). They used to be copied bit for bit into an 8-bit image, which tinted the menus green and red; they are now converted.
- Movies are black: `libSceVdecsw` only decodes with VideoToolbox (macOS).

## Windows findings

- MinGW winpthreads rounds timed waits up to the 15.6 ms system tick even with `timeBeginPeriod(1)`, and `sleep_for` below 1 ms returns at once. Guest sleeps longer than 50 µs, condition variable, event flag, semaphore and equeue timeouts go through high-resolution waitable timers (`PreciseSleep`, `PreciseWait`). Shorter guest sleeps yield with `SwitchToThread` until their deadline instead of spinning. Zero-length ones return at once, as FreeBSD's `nanosleep` does: `NCA::PumpThread` calls `sceKernelUsleep(0)` when less than 1 ms of its audio period is left, and a yield there hands its CPU to a ready thread of any priority (`Sleep(0)` as well), which took 1-13 ms in 7-10% of yields on a host loaded like the game's.
- libstdc++ has no futex on MinGW: `std::atomic::wait` parks every waiter on one of 16 process-wide condition variables chosen by address, and each notify wakes all waiters of that bucket, which spin through `sched_yield` before they sleep again. A guest mutex waiter therefore woke and spun on every contended unlock of any lock in its bucket: two waiting threads used about 80% of a core each in run 21's snapshots, and the neighbouring-lock case of `guest_scheduling_tests` measures 81% with the old lock and under 0.1% now. Guest mutexes now wait with `WaitOnAddress`.
- The title's 13 `NdJobWorkerThread`s idle in a `pause` loop that polls the job queues (`eboot+0x13884f6`) and make no system call while they spin. They are created at priority 766 and set 766 (or 701) again each time they go idle; both map to `THREAD_PRIORITY_LOWEST`. No HLE function can make them yield without patching guest code.
- Log output costs one `WriteFile` per line instead of one per character. MinGW's `fprintf` (`__USE_MINGW_ANSI_STDIO`, which libstdc++ forces) emits each character with `fputc`, and the UCRT writes an unbuffered stream one character per `WriteFile`; stderr, and stdout on a console, were written that way (45-60 µs per character with stderr redirected by PowerShell's `Start-Process`; 2,000 log lines of about 110 characters took 420-440 ms to a plain file, against 8 ms now). libc gives stdout and stderr a 64 KiB buffer when it loads, and a line leaves with the `fflush` after it, in one `WriteFile` per 5 KiB (the UCRT's text-mode translation chunk). The `APS5_LOG_*` macros flush after every line, and every direct `fprintf`, `fputs` or `fwrite` to stdout or stderr must flush at the end of its message or block. Guest writes to stderr are flushed at once, `abort` flushes both streams through a `SIGABRT` handler, and fatal exception reports flush them. Executables linked with MinGW's startup code (the test programs) make stderr unbuffered again after the DLLs have loaded; the relinked `eboot.exe` has no such code.
- `VirtualQuery` walks page tables and costs milliseconds on multi-GiB views; guest range checks consult the allocation registry first, and watches keep their original protections instead of querying them again.
- Remapping a view unmaps it for a moment. A guest thread that faults on it waits for the remap and retries; the retry budget restarts after every remap.
- Fatal exception reports print the state and protection of the faulting page.

## Frame rate

The switches (`ANYPS5_DISPLAY`, `ANYPS5_UNCAPPED`, `ANYPS5_FPS_LIMIT`, `ANYPS5_PRESENT_MODE`, `ANYPS5_LEGACY_TSC` and F9) are described in the README. Addresses below are `eboot.exe` virtual addresses.

### Game speed

- The title times each frame as an inline RDTSC delta divided by `sceKernelGetTscFrequency()`, which it reads once (`0x1405c6f73`, divide at `0x1405bb9d3`-`0x1405bb9e6`), and clamps the step to 0.1 s (1 / `MinimumFramerate` 10.0, `0x1413a5638`-`0x1413a5650`).
- `sceKernelGetTscFrequency` used to return 1 GHz while RDTSC on the i5-12400F counts at 2496 MHz, so game time ran at min(2.5x real time, 0.1 s per frame): 1.8-2.3x too fast in the first scenes at 18-23 fps, and 0.6-0.9x in the Language menu at 6-9 fps.
- libkernel now measures the TSC rate once when it loads, in about 100 ms (for example `TSC frequency 2495.996 MHz (calibrated over 100.6 ms)` on this PC), and `sceKernelReadTsc` and `sceKernelGetTscFrequency` both use the host TSC. Game time follows real time above 10 fps, so the first scenes should play at normal speed; below 10 fps the game runs in slow motion, as on a PS5.
- Without an invariant TSC, with a measured rate outside 100 MHz to 10 GHz, on a non-x86-64 build or with `ANYPS5_LEGACY_TSC` set, libkernel keeps the old 1 GHz nanosecond clock and logs why, and uncapped stays off.

### Display profiles and the title's modes

`SetRenderMode` (`0x141568a00`) turns the display option into a mode, depending on what the output offered at boot: the title asks for 119.88 Hz support once (`0x1405c7b8e`), so a profile holds for the whole run. Each mode is a {flip rate, pacing factor, flags} entry of the table at `0x141fa34e0`; applying it (`0x1415c3c50`) selects 59.94 or 119.88 Hz with `sceVideoOutConfigureOutput`, pegs or unpegs VRR and sets the flip rate.

| `ANYPS5_DISPLAY` | Option | Mode | Output | Flip rate | Frame rate on a PS5 | Pacing factor |
|---|---|---|---|---|---|---|
| `60hz` | Fidelity | 0 | 59.94 Hz | 1 | 30 | 1.0 |
| `60hz` | Performance | 1 | 59.94 Hz | 0 | 60 | 0.0 |
| `120hz`, `vrr` | Fidelity | 4 | 119.88 Hz | 2 | 40 | 0.5 |
| `120hz`, `vrr` | Performance | 3 | 119.88 Hz | 1 | 60 | 0.0 |
| `120hz`, `vrr` | Performance+ (1080p) | 2 | 119.88 Hz | 0 | 120 | -0.5 |
| `vrr` with Variable Framerate | Fidelity | 5 | 119.88 Hz VRR | 1 | 40-60 | 0.5 |
| `vrr` with Variable Framerate | Performance | 6 | 119.88 Hz VRR | 0 | 60-120 | 0.0 |

- When `SetRenderMode`'s second argument is set (probably during cinematics; not confirmed yet), modes 4 and 5 switch to mode 7 (flip rate 1, pegged with `sceVideoOutVrrPegToFixedRate`, each frame flipped twice: 30 fps) and mode 6 to mode 3. Pegged flips keep the vblank grid even when uncapped.
- With `vrr` the HLE shows unpegged flips as soon as they are ready, but no sooner than flip rate + 1 vblank periods (8.34 ms each at 119.88 Hz) after the previous flip, so mode 5 reaches at most 59.94 fps and mode 6 at most 119.88 fps.
- The title saves the display option. If it restores a mode the current profile cannot show, for example Performance+ under `60hz`, it falls back (`0x141568cf7`-`0x141568d30`): it flips immediately (HSYNC), skips its render-thread pacing and still asks for 119.88 Hz. The HLE then shows every flip at once, refuses 119.88 Hz with `VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE`, switches `ANYPS5_PRESENT_MODE=auto` to MAILBOX and logs `119.88 Hz output refused` and `first immediate (HSYNC) flip` once each. After changing `ANYPS5_DISPLAY`, re-select the display mode in Options > Display.

### Uncapped ceilings

Uncapped removes the HLE's vblank and flip-rate gate, but the title keeps pacing itself. Its render thread (`0x1415b6ca0`-`0x1415b6df6`) starts frame N no earlier than P - 4 ms after the flip event of frame N-2, with P = (pacing factor + 1) x 16.683 ms. The flip handler stamps that event with RDTSC (`0x1415c324a`, stored at `0x1415c36b3`), and this HLE sends it only after the host present has returned. With L the time from the start of a frame to its flip event, two frames take at least L + P - 4 ms, so the frame rate cannot exceed 2 / (L + P - 4 ms):

| Mode | P - 4 ms | L = 0 | L = 5 ms | L = 10 ms | L = 20 ms |
|---|---|---|---|---|---|
| Performance+ (2) | 4.34 ms | 461 fps | 214 fps | 139 fps | 82 fps |
| Performance (1, 3, 6) | 12.68 ms | 158 fps | 113 fps | 88 fps | 61 fps |
| Fidelity at 40 fps and VRR Fidelity (4, 5) | 21.03 ms | 95 fps | 77 fps | 64 fps | 49 fps |
| Fidelity (0) | 29.37 ms | 68 fps | 58 fps | 51 fps | 41 fps |

- L covers the title's render work, the driver's worker and graphics threads, the GPU, the uncapped limit and the host present. In the Language menu it is above 100 ms today (`flip_submit_to_complete_ms` 102-704 ms in run 26), which would keep even Performance+ below 19 fps, so shortening L is what raises the frame rate.
- For more than a PS5 delivers, choose Performance+: its P - 4 ms is 4.34 ms, so 120 fps needs L of at most 12.3 ms, against 4.0 ms in Performance.
- The default limit stops uncapped at the monitor's refresh rate or 120 fps, whichever is lower, but not below the emulated vblank rate. Above 120 fps, which needs an explicit `ANYPS5_FPS_LIMIT` (or `0`), the title runs untested: physics, animation, rope and cloth, and scripts may misbehave, and the log warns.

### Recommended setups

- PS5 pacing: no variables for a PS5 on a 60 Hz TV, or only `ANYPS5_DISPLAY=120hz` or `ANYPS5_DISPLAY=vrr` for a 120 Hz or VRR TV.
- Better than a PS5: `ANYPS5_DISPLAY=120hz ANYPS5_UNCAPPED=1`, then Performance+ in Options > Display. Frames are shown as soon as they are ready instead of on the next 119.88 Hz vblank, up to the default limit; a higher `ANYPS5_FPS_LIMIT` goes past the frame rates this build was tested at. `auto` presents uncapped flips with MAILBOX (3 images on this PC), so nothing tears and the host present does not wait for the monitor.
- G-Sync or FreeSync monitor: also set `ANYPS5_PRESENT_MODE=fifo` and `ANYPS5_FPS_LIMIT` to about 0.95 x the refresh rate (114 at 120 Hz, 137 at 144 Hz). FIFO lets the monitor show each frame as it arrives, without tearing, and the margin keeps the frame rate inside the VRR range, so a present does not wait for a refresh while it holds the driver's guest memory lock. The HLE never lets uncapped FIFO presents exceed 0.97 x the refresh rate and logs that uncapped is degraded when that hold applies; a limit above 120 also logs the untested frame rate warning.
- After changing `ANYPS5_DISPLAY`, re-select the display mode in Options > Display.

### To check in the next run

Unit tests cover the switches (`guest_time_tests`, `video_out_flip_tests`). The next announced run, with `ANYPS5_DISPLAY=120hz ANYPS5_UNCAPPED=1 ANYPS5_TRACE_TIMING=1` and F9 to compare, should check:

- the TSC line in the game's log, and movement, music and lip-sync at normal speed in the first scenes;
- that a scripted camera move or cutscene line takes the same wall-clock time with PS5 pacing and uncapped: the frame rate may change, the pace of the world must not;
- Performance+ in Options > Display (and Variable Framerate with `vrr`), and the frame rate each mode reaches with PS5 pacing and uncapped;
- with uncapped on, the `[FrameTiming]` values `flip_submit_to_complete_ms`, a lower bound for L in the ceilings above, and `VideoOut.Flip.present`, the part of it spent in the driver's present call (the wait for the frame's GPU work, the blit and the host present);
- the `swapchain:` line, and the `Vulkan.Present.acquire_fence_wait` and `Vulkan.Present.queue_present` timing marks staying near 0;
- whether the title ever flips immediately (`first immediate (HSYNC) flip`): besides the fallback above, `0x14062ae0d` sets the title's immediate-flip byte (`0x142153744`) on a path not yet identified;
- whether the internal resolution of a given scene changes, since every RDTSC interval the title measures is now 2.5x shorter than before.

Still open after that run: the title above 120 fps. Before the default limit is raised, traversal, climbing, rope and cloth need a check at more than 120 fps with `ANYPS5_FPS_LIMIT=0`, far above the 6-23 fps the title reaches today.

## Debug switches for performance work

- `ANYPS5_DUAL_LANE=1`: runs every wave64 compute program with two guest lanes per invocation, as before single-lane compute; `recompile_replay` prints which cached compute programs run single-lane.
- `ANYPS5_SAMPLE_THREADS=1`: samples the title, worker and graphics threads every millisecond (Windows) and prints the hottest functions and call chains every 30 s.
- `ANYPS5_TRACE_SLOW_OPS=1`: prints 5 s summaries of lock waits, the reasons the driver worker waits for the graphics thread, the stages of recording draws and dispatches, and slow memory, tracking and device operations. On Windows it also prints the first tracking fault of each guest instruction with the watch it hit.
- `ANYPS5_TRACE_TIMING=1`: per-frame timing of the driver stages and GPU timestamps per command batch. `Graphics.Draw.feedback` counts the draws that sample their own color or depth target. Draw-queue batches add up in `Graphics.GpuTime.execute`, one-off batches in `Graphics.GpuBatch.<name>` (`depth_copy`, `color_copy`, `resolve`, `cpu_access`, `flush_stores`, `null_texture`, `readback`, `present`). A batch is timed from the moment the GPU work submitted before it has finished, so batch times do not overlap.
- `ANYPS5_TRACE_GPU_PASSES=1`: times every render pass, keyed by its color target (or its depth target when it has no color target) and its extent, and every compute dispatch, keyed by program address and group count. A draw-queue batch holds 512 timestamps; once they run out, the batch stops timing passes and dispatches and counts the ones it dropped. Every 5 s, `[gpu-time]` lines on stderr give the batch, pass, dispatch, untimed and one-off totals and the 20 most expensive passes and programs. With `ANYPS5_TRACE_TIMING=1` the frame lines also carry `Graphics.GpuPass.<width>x<height>` and `Graphics.GpuProgram.<address>`.

## Next steps

1. Windows: protect the guest red zone (see the crash above). Performance: serve GPU-written surfaces sampled through a mismatched descriptor (for example `0x1492970000`, 256x256 R8 tile 4 over a tile 27 render target) from the GPU instead of revalidating guest memory, sample depth directly instead of copying it, and cut the per-draw resource setup and the full pipeline barriers around every compute dispatch.
2. Selector image: find out why the history depth buffers (`0x8463400000` / `0x8464710000` in the logs) are never filled, then fix the dark lighting and the blocky tiles.
3. Continue past New Game into gameplay. Watch for GPU-culled indirect draw arguments exceeding the index buffer, a hint from the Kyty fork (github.com/budhilaw/Kyty, branch `feat/ps5-gameplay-fixes`).
4. Performance target: 30 fps in gameplay at a 720p internal resolution. That needs render scaling, smaller recompiled shaders and less per-draw CPU work.

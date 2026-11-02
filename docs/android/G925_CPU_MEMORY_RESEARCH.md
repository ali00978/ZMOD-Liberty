# G925 CPU and GPU memory investigation (separate development branch)

Target: POCO X8 Pro Max, Dimensity 9500s, Immortalis-G925 MC11,
system Vulkan 49.1.0. Reference observations: 25–45 FPS when stationary,
approximately 46% aggregate CPU utilization, highest sampled CPU frequency
3.3 GHz, and roughly 2666 MiB process resident memory. Neither aggregate CPU
load nor process resident memory equals GPU utilization or dedicated VRAM.

## CPU

The native game already uses Android Performance Hint (ADPF) in
`glue/rexglue-sdk-main/gta4-recomp/src/android/android_performance_hint.cpp`.
It identifies up to six busy threads and sends their frame durations to Android.
Android then chooses frequency and CPU core placement; app-level code cannot
set a true CPU clock speed or bypass system thermal/power limits.

This investigation branch adds **Extreme (16 ms)** to the launcher's CPU Boost
menu, alongside existing Maximum (20 ms), Steady (25 ms), Balanced (30 ms),
Battery (33 ms). The underlying cvar explicitly accepts 10–50 milliseconds.
This is an *optional scheduling hint*, not a device overclock and not a
guarantee of 60 FPS. Shorter targets may draw more power, raise temperature,
and worsen long-session FPS by thermal throttling. 16 ms approximates a
60-FPS CPU work target; a precise 60 FPS frame budget is 16.667 ms.

Do not force CPU affinity by default. The source's
`gta4_thread_pinning` is off by default and the developer documentation
warns that changing thread timing can expose races. Use the system scheduler
first.

## GPU memory

Android integrated GPUs use **shared system RAM**, not separate VRAM. There
is generally no safe "assign X GB RAM to GPU" control at app level. Forcing
buffers to occupy unused memory does not make the renderer run faster, and can
increase memory pressure, evictions and crashes.

The existing in-game memory overlay reads process resident memory, **not**
per-GPU heap allocation or Vulkan memory usage. It currently cannot report
GPU utilization on devices where vendor sysfs telemetry is inaccessible.

### Experimental RAM headroom for reusable GPU images

The G925 memory-lab branch has increased the **Android-only** Vulkan image
allocation reuse pool in
`glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.h`:

- Original: **32 MiB**, up to **64** image allocations retained.
- G925 experiment: **96 MiB**, up to **128** image allocations retained.
- Desktop/platform defaults remain at 32 MiB / 64 entries.
- `NativeImageReusePool` does **not** allocate the full limit up front. It
  retains only eligible retired Vulkan image allocations after their GPU
  submissions have completed, and can hand them out for subsequent compatible
  image creations. Existing trim and allocation-recovery flows remain intact.
- This is **not** an increased texture-resolution setting, a 96 MiB mandatory
  RAM reservation, or an increase in the amount of addressable GPU memory.
  It is a **higher ceiling** for reusing compatible allocations.
- Possible upside: fewer Vulkan memory allocation/free operations and fewer
  allocation-related hitches. Possible downside: higher resident memory
  pressure or extra throttling. It does **not** guarantee an FPS increase.

The launcher additionally exposes **anisotropic filtering** for oblique road,
wall, and pavement textures. The three G925 graphics presets request 4x, 8x,
and 16x respectively. These are existing native renderer options; actual
quality/performance must be validated on the Immortalis G925 system driver.

Further development should focus on actual texture cache efficiency and
Vulkan memory allocation telemetry. Possible observables:
- `vkGetPhysicalDeviceMemoryProperties2` and, *if supported*,
  `VK_EXT_memory_budget` for per-heap allocation and available budget.
- CPU renderer, recorder and worker timings per frame (the port has profiling
  instrumentation for this).
- Shader pipeline cache hits/misses, texture upload frequency, driver warnings.
- Thermal state and frame-time percentiles in repeated matched scenes.

A memory budget number must be interpreted as *driver-reported* and does not
establish physical GPU memory dedication or guarantee more FPS.

Do **not** increase Android Memory Extension / swap in hopes of faster
GPU rendering; storage-backed swap is slower than physical RAM.

## How to test CPU Boost and the larger allocation reuse pool (after a successful APK build)

1. Back up existing Android/data/com.libertyrecomp files and game saves.
2. Using the same graphics, game scene and System Vulkan driver,
   compare Steady, Maximum and Extreme in 60–120 second sessions.
3. Warm the device to comparable temperatures. Note steady FPS and worst
   frame-time dips, not just momentary peak FPS.
4. Prefer Maximum/Steady if Extreme heats the phone or produces worse lows.
5. Compare this experimental branch against the standard G925 graphics build using
   the **same** graphics and boost settings. Record 1% lows, stutters after
   turning corners, memory pressure, device temperature, and crashes.
6. Switch back to the standard build if performance regresses or the device
   begins closing other applications. Back up saves before updating APKs.

This branch is **not** the active G925 APK build branch; it does not cancel
or modify `feature/g925-graphics-lab` or its current GitHub Actions run.

## References

- Android ADPF: https://source.android.com/docs/core/perf/performance-hint-api
- Android memory allocation: https://developer.android.com/topic/performance/memory-management
- Vulkan embedded UMA: https://docs.vulkan.org/guide/latest/embedded_programming.html
- Vulkan memory allocation: https://docs.vulkan.org/guide/latest/memory_allocation.html

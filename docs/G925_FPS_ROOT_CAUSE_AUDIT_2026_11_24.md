# G925 Native FPS root-cause audit — 2026-11-24

## Goal and constraints

Increase *actual GTA IV title/game-update frames*, **without** reducing the
user's resolution, traffic/pedestrians, draw distance, shadows, reflections,
texture quality, anti-aliasing, effects, or display sharpness. Keep the stock
Immortalis/Mali Vulkan driver and preserve game saves. Do not confuse present
attempts with unique gameframes. No root requirement.

**Important:** GitHub CI/Android APK success does **not** demonstrate FPS
improvement. Changes not installed on the phone cannot improve device FPS.

## Evidence already obtained (captures are instrumented)

| Build | GPU envelope mean | Native CPU command recording | Native callback | Title frame interval mean |
| --- | ---: | ---: | ---: | ---: |
| 2.4, Nov 3 | 21.64 ms | 24.40 ms | 31.51 ms | 43.32 ms |
| 2.7, Nov 3 | 20.94 ms | 22.58 ms | 29.62 ms | 39.26 ms |
| 2.9, Nov 3 | 17.99 ms | 20.73 ms | 26.60 ms | 35.00 ms |
| 2.11, Nov 3 | 12.31 ms | 18.48 ms | 24.49 ms | 36.95 ms |
| 2.39, Nov 21 | 21.78 ms | 20.25 ms | 27.16 ms | 37.72 ms (p95 47.99) |

Captures are not apples-to-apples: scenes/settings and profiler overhead may
differ. CPU spans overlap and must not be added. Vulkan timestamps are not
proof that the GPU was entirely busy. Nevertheless, the large changes in GPU
time with nearly unchanged game interval **suggest** a second bottleneck in
CPU capture/assembly/recorder, frame admission/backpressure, or title pacing.

60 true FPS requires ~16.67 ms per unique gameframe. A 37.72 ms interval is
~26.5 title FPS equivalent. Simply raising a 60 FPS cap cannot remove CPU
serialization, GPU passes, guest logic cost or image-presentation backpressure.
Reducing 37.72 ms to 16.67 ms requires ~56% less *end-to-end interval*,
not necessarily 56% less in any individual overlapping CPU phase.

Earlier build 2.11's detailed self-time included:
- \`publish / refresh-output\`: **10.436 ms** per detailed frame;
- \`record-frame\`: **3.691 ms**;
- \`record-command\`: **2.678 ms**.

\`refresh-output\` includes CPU work and the presentation gate. It is not all
necessarily idle. The subsequent v2.12 publication overlap attempted to
remove admission serialisation. 2.39 not improving proves this cannot
be assumed fixed; its CURRENT detailed measurements are required.

## Source audit

- \`gta4-recomp/src/android/android_frame_wait_hook.cpp\`:
  \`sub_82A46D70\` is polled while two frames are pending, with a 250µs
  sleep instead of core-spinning. \`gta4_extra_frames_in_flight\` can relax
  the *guest-visible* count without changing actual Vulkan completion. The
  user already tried up to 4 run-ahead frames, which did not deliver
  playable FPS; don't blindly increase it again.
- \`src/graphics/gta4_native\`: title render thread captures draw/state,
  worker assembles, recorder emits Vulkan; thousands of native draw commands
  per frame. Existing state caches, descriptor working sets, constant-ring
  reuse, source draw merges and vertex fallback already optimize part of
  this path. Do not reimplement them.
- \`gta4-recomp/src/android/android_performance_hint.cpp\`: default Android
  ADPF CPU target already 16ms with stable hint threads. This is a *hint*,
  not an FPS unlock; prior A/B did not solve gameframe rate.
- \`os/android/scripts/build_native.sh\`: targets A720-class Armv9 CPU,
  compiles optimized C++ via CMake; direct-calls rewrite intentionally
  disabled due measured no gain and previous crash. Do not switch it on
  as an unmeasured fix.
- \`native_profile_detail.h\`: diagnostic CSVs already include:
  - \`native-performance-frames.csv\` transport (queue backpressure,
    queue dwell, worker assembly and idle, paint CPU, command count);
  - \`native-performance-cpu.csv\` self-time by CPU operation;
  - \`native-performance-cpu-shaders.csv\` CPU cost by shader pair/phase;
  - \`native-performance-gpu-passes.csv\` approximate pass-level
    timestamps, region labels, draw counts, unavailable spans.
- Previous report summarized top CPU operations but did not rank the frame
  transport, GPU pass regions or CPU shader-phase costs. This branch adds
  **read-only** additional summaries with exact frame+sequence correlation,
  bounded reads, stale-run rejection and raw-text privacy validation.
  Nothing added to rendering threads or visual cvars.

## Root-cause investigation order

1. **Title pacing / queue backpressure (highest uncertainty, strong lead).**
   Compare matching \`backpressure_sum_ms\`, \`queue_dwell_sum_ms\`,
   \`worker_assembly_ms\`, \`paint_total_ms\`, and publish/refresh-output
   CPU self-time. Confirm whether title submits are blocked when CPU/GPU
   still have headroom, or waits are genuine work. A *long* queue dwell
   alone is not proof of CPU overhead; only blocking spans matter.
2. **CPU recorder and draw submission.** Locate top self-time per render
   command and shader pair; look at thousands of state changes/metadata
   calls. Prefer safe batching of strictly identical draws or hoist
   immutable Vulkan bindings. Multi-threaded secondary command buffers
   are a separate, high-risk experiment requiring correct ordering,
   per-thread command/descriptor pools, and on-device A/B.
3. **Actual GPU bottleneck.** If the measured pass regions and GPU envelope
   are limiting, rank \`resolve\`, \`depth-resolve\`, \`deferred-light-volumes\`,
   translucent, shadow, post-processing and reflection passes. On Mali's
   tile renderer, only remove demonstrably redundant copies/load-store
   ops while preserving pixel output. Never silently skip effects or
   lower resolution.
4. **Guest CPU simulation / thread contention.** If neither recorder nor
   GPU is saturated, profile the recompiled game thread with native
   sampling and identify scheduling, barrier/busy-wait or AOT overhead.
   Thread pinning can provoke guest races; neither forced big-core
   affinity nor overriding the system Mali driver is a safe default.
5. **Correct FPS reporting.** \`title-fps\` counts actual gameframe
   submissions, not Vulkan present attempts, fake 60FPS from display
   repeats, or interpolation.

## Evidence-driven release gate

Compare **identical** save, scene, quality cvars, device temperature and
gameplay mode against a known-good baseline:
- unique title FPS and p95 frame interval over 60s warm gameplay;
- native CPU recorder self-time and overall callback;
- GPU envelope and credible pass timings;
- queue blocking/assembly and draw counts;
- visual correctness and input/audio latency, no regressions or device loss.

Only claim an FPS gain when the same quality settings yield a repeatable
improvement on the actual G925 phone. If not, revert or isolate the change.
A stable 40 FPS interval (<=25 ms) is a meaningful intermediate threshold;
60 FPS <=16.67 ms is the longer target, not a release promise.

## References

- Project source: \`native_profile_detail.h\`, \`android_frame_wait_hook.cpp\`,
  \`native_cpu_profile.h\`, \`build_native.sh\`, profiling reports above.
- Khronos Vulkan Samples, multi-threaded command buffers:
  https://docs.vulkan.org/samples/latest/samples/performance/command_buffer_usage/README.html
- Arm Vulkan best practices for Mali:
  https://developer.arm.com/mobile-graphics-and-gaming/vulkan-api-best-practices-on-arm-gpus
- Android GPU Inspector/APA Mali CPU/GPU attribution:
  https://developer.android.com/agi/sys-trace/long
- Android port architecture:
  https://github.com/vaduur/LibertyRecompAndroid/blob/main/docs/android/PORTING.md

**Current status:** validated hypothesis and better diagnostics only.
No FPS gain is established from this research branch.

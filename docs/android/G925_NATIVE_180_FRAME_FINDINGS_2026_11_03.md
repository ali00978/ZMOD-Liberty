# G925: confirmed 180-frame CPU/GPU performance bottleneck (2026-11-03)

The user supplied actual native 2.4 measurements on POCO / Mali G925:

| Stage | Avg ms | P95 ms | Max ms |
| --- | ---: | ---: | ---: |
| GPU timestamp envelope | 21.64 | 28.78 | 30.74 |
| Title game frame interval | 43.32 | 57.96 | 77.88 |
| Native render callback CPU | 31.51 | 40.08 | 56.83 |
| Native command recording CPU | 24.40 | 31.93 | 44.66 |
| Native texture preparation CPU | 3.43 | 4.70 | 9.32 |
| Native housekeeping CPU | 2.16 | 3.88 | 9.01 |
| Native presenter CPU | 1.64 | 3.60 | 12.54 |
| GPU fence wait CPU | 0.00 | 0.00 | 0.01 |
| Pipeline wait and compile CPU | 0.00 | 0.00 | 0.00 |

60 FPS requires a 16.67 ms throughput budget. Both the measured GPU envelope and native command recording exceed it, and native render callback is longer again. Some CPU ranges overlap, so NEVER sum them. Vulkan timestamp intervals can contain idle gaps and profiling itself adds overhead; do not overclaim actual hardware GPU occupancy.

Prior uninstrumented 2.3 report had about 60 host Vulkan present attempts/s while GTA IV new gameframes fell from 60 to 30/s. It did not measure unique displayed frames. Swapchain acquire averaged about 0.2ms during degradation. ADPF stable enabled vs disabled both showed this decline per user. Thermal headroom did not continuously fall.

## Important engineering finding

Source graphics_system.cpp already uses NativeDrawStateCache to avoid identical vkCmdBindPipeline, vkCmdBindDescriptorSets, vkCmdSetViewport and vkCmdSetScissor calls. native_descriptor_working_set.inc already compares desired descriptor slots against published snapshots, batches the changed writes and skips unchanged pages. Many other normal defaults optimize constant ring, prefetch, pipeline memo, draw merging, and persistent buffers. Reimplementing those mechanisms will not remove the measured 24ms CPU recording time.

## Capture was already saved: NO MORE PHONE GAMEPLAY TESTS NEEDED

The same 2.4 capture already wrote files in Android/data/com.libertyrecomp/files/LibertyRecomp/Diagnostics:

- native-performance-cpu.csv: phase, operation, calls, inclusive_ms, self_ms, worst-command
- native-performance-cpu-shaders.csv: shader and render-phase CPU timings
- native-performance-gpu-passes.csv: exact measured GPU pass ranges
- native-performance-frames.csv: native frame transport and timing
- native-performance-latest.csv: aggregate already summarized by the user

The only next evidence needed is the existing first three detailed CSV files, not another play session. Rank operation self_ms and shader/phase attributions before making expensive changes to native rendering. A new native APK should bundle all proven changes into one tested build. Preserve graphics/resolution and System Vulkan; do not turn on stale/lazy resolves or change shaders without validation. Do not promise locked 60 FPS without a new real-device verification.

This documentation-only commit is excluded from APK triggers.
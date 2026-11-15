# G925 2.23 — eliminate proven zero-fragment Vulkan draws

**Android versionCode:** 51
**versionName:** `0.5.9.3-mali-alpha-2.23-g925-no-fragment-draws`

**Starting point:** 2.22 head
`5f03fa60799f5b208b812abc72e884d2eab9d50b`.
The 2.22 Android build is kept available for rollback and comparison.

## Background: measured phone work

The last supplied G925 2.20 180-frame capture reports ~3,565 Vulkan
drawcalls per frame, 2,727 indexed record operations, 171 renderpass
boundaries, **20.27ms** of CPU command recording and **21.39ms** GPU time
on average; title intervals **36.11ms** (p95 44.31ms). GPU and CPU work
overlap, detailed profiling adds overhead, and 60 FPS requires a 16.67ms
frame period. Existing v2.7–2.22 cache, pipeline, batching and
Vulkan-synchronization improvements remain untouched.

## New optimization: omit a GPU draw with no covered pixels

The original native renderer still submits vertex/index/pixel draws when
the guest's *effective scissor* has an empty intersection with the
logical target. Such a draw has no fragments to write any target, but
the GPU can still spend work on fetching vertices, invoking vertex
shaders and primitive assembly.

A new `native_zero_coverage_draw.h` policy matches the precise guest
scissor clamping used by `ComputeNativeRasterArea`: left/top and
right/bottom are clamped to the logical target bounds. A draw whose
result has zero width or height may be elided. Invalid target dimensions
and dimensions above signed 32-bit range **do not trigger** the shortcut.

Integration in `graphics_system.cpp` checks the cheap empty-scissor
condition first and then requires all of the following before omitting
the draw: **no active fire, translucency or water occlusion query; no
legacy frame diagnostics, trace/breadcrumb probe or explicit GPU
attribution; and no temporal or special composite work.** The game
still records its command in title order, performs any necessary
surface/materialization work and **opens the rendering scope** (so
attachment LOAD/CLEAR effects still happen). Only the no-fragment
`vkCmdDraw` or `vkCmdDrawIndexed` is omitted. No global GPU resource,
barrier, fence, shader, raster quality, frame cap or scheduling change.

This is not an unsafe blanket no-op filter: normal valid coverage,
queries and uncertainty take the original draw path. The new native
profiling counter `zero-coverage-draw-skips` makes the number of
elided draws visible to performance traces.

## Automated correctness

`tools/test_g925_zero_coverage_v223.cpp`: 300,000 randomized scissor
rectangles with an independently written reference intersection,
compile-time empty/full/boundary cases, query/diagnostic/temporal
exclusions, and exact counter name. Integration regression
`tools/test_g925_zero_coverage_v223.py`: verifies the actual recorder
calls the policy **after** the query setup and **after** rendering
scope creation, with a fast normal-draw reject, and tests the real
compiled helper and profiler counter implementation.

CI must run this and all earlier native C++ and Java regressions,
then build the full Android Mali ARMv9 libraries and APK.

## What this can and cannot improve

Whenever a frame contains empty-scissor draws, the number of Vulkan draw
commands and GPU vertex/index work **is genuinely reduced** without
changing covered pixels. The impact is **zero if the workload has zero
eligible draws**; this is why the new counter matters. FPS or p95
frame-time improvement on the POCO X8 Pro Max is **not yet measured**.
Do not infer real device speedup from a successful host test.

For a separate, substantial GPU fill-rate reduction that is *not*
pixel-identical, the existing launcher already offers FSR 1 at Ultra
Quality. This intentionally lowers internal rendering resolution and
then sharpens to the output resolution; opt in only if accepting its
visual tradeoff. This 2.23 release does **not** enable it automatically.

Rollback: [2.21 verified APK](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42431895529)
and the last successful later version when available.

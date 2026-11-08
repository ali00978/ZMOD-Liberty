# G925 2.21 — exact color/depth surface hot-cache on the CPU recording path

**Android:** versionCode 49, `0.5.9.3-mali-alpha-2.21-g925-surface-hot`.

**Base:** successfully built G925 2.20 (commit
`d7f17663fbf5a0b32f5dfcd2d5f6c130d607d361`).
https://github.com/ali00978/ZMOD-Liberty/actions/runs/42413291261

## Device data used to prioritize this change

Actual user-supplied G925 2.20 profile: 180 frames, one-shot detailed CPU
capture 65s after rendering began. This diagnostic adds overhead, so it is
**not a normal FPS benchmark**. The recorded average title interval was
36.11 ms (p95 44.31 ms); GPU execution 21.39 ms (p95 29.82 ms);
native CPU command recording 20.27 ms (p95 23.65 ms, max 35.10 ms).
Render callback 26.92 ms, outside-renderer 9.26 ms, native housekeeping
2.25 ms, queue-submit 0.14 ms. Stages can overlap and must not be summed.

Each captured frame reused **6,828.1 surface lookups on average**, with
3,564.5 vk-draw operations, 2,727.4 indexed records and 170.7 Vulkan
render-pass boundaries. The prior state cache returned a surface by hashing
the complete surface identity and checking one or two buckets each time.

## Code change

`native_draw_lookup_cache.h` now has a **two-entry exact most-recent surface
lookup**, checked before computing its existing set hash. Repeated color/depth
target alternations can therefore bypass the expensive hash/table probing.
A cache hit still requires the *exact nine-field descriptor*, depth role,
sample count, **published registry revision**, a live non-null pointer and
the current lookup invalidation **epoch**. The pair is recorder-local
mutable metadata, not a new GPU image owner or memory cache.

All three ways of filling the underlying two-way surface cache remember the
new valid exact entry. A victim eviction forgets its matching fast key first.
Every release/reflection/destruction invalidation clears both borrowed
pointers before incrementing the existing epoch. Full epoch-wrap clearing
is unchanged. No GPU-in-flight resource is reclaimed sooner, no new
cross-frame persistent pointer ownership is introduced, and no
render/synchronization semantics change.

## Automated regression

`tools/test_g925_surface_hot_v221.cpp` runs 200,000 alternating
color/depth lookups and tests exact descriptor fields, metadata revision,
resurfacing under the same handle, collisions, one-way/two-way eviction,
1,000 churned surface identities and 2,000 explicit lifetime resets.
`tools/test_g925_surface_hot_v221.py` checks the production registration
and invalidation call sites and compiles/runs the native regression with
C++20 before the **full Android Mali ARMv9 APK** is built. All prior
C++/Java/NDK tests continue.

No resolution, traffic density, shadows, reflection quality, shaders,
draw counts, Vulkan submission fences, GPU drivers, clocks or frame queue
policy are changed. This optimization reduces CPU work *when the workload
repeats the same surface pair*. It **cannot alone fix the measured GPU
frame time**, which remains over 16.67 ms for a 60 FPS target. No FPS
improvement is claimed until a like-for-like phone comparison is measured.

## Next evidence to seek

Because the sampled GPU frame (21.39 ms) and recording (20.27 ms) both
exceed 16.67 ms, a second major FPS increase may need renderer-architecture
work that reduces legitimate draws or renderpass switching while preserving
the same image. Those changes require per-pass draw/state traces and tests;
blindly merging Vulkan passes or removing draw commands would risk missing
visuals and is not included in this safety-focused release.

Known-good fallback: G925 2.20
https://github.com/ali00978/ZMOD-Liberty/actions/runs/42413291261

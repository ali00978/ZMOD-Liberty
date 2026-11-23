# G925 Native v2.40 — stable mesh-worker throughput

This iteration follows the **2.39 / 2026-11-21** G925 Native profile and focuses
on reducing CPU work and eliminating a reproducible overflow risk. It does **not**
pretend to have solved 60 FPS: the previous device capture measured ~37.72 ms
average title intervals, 47.99 ms p95, ~21.78 ms GPU time and ~20.25 ms CPU
command recording, with profiler overhead and overlapping CPU stages.

## Changes

1. For opaque triangle lists shorter than **96 triangles**, keep the already
   supported original-order draw instead of copying guest indices, allocating
   a reorder job, and competing with the renderer for CPU. An unaligned
   index count (not divisible by three) is likewise rejected for safe fallback.
   The draw geometry, materials, texture fidelity, resolution and saves are
   unchanged. On complex meshes the existing cache optimizer still runs.
2. Fix a **32-bit index-span wraparound** where indices spanning
   `0..UINT32_MAX` computed zero vertices then indexed an empty array.
   The range and triangle count now use checked 64-bit arithmetic and reject
   sparse or overflowing ranges before allocating adjacency buffers.
3. If a queued mesh was evicted and the background worker holds its last
   strong reference, mark it unusable and discard the obsolete optimization
   rather than wasting CPU on vertex reordering. A concurrent reader can
   still safely use the existing original-order fallback.
4. Advance Android version code to **68**, label release `2.40-g925-vertexguard`
   and update the standalone APK name to `ZMOD-Liberty-G925-Native-v3.1-armv9.apk`.
   The package remains `com.zmodliberty.g925native` (in-place update,
   independent of `com.libertyrecomp`).

## Verification gates

- Host C++20 test with random 16-bit and 32-bit triangle lists asserts
  lossless triangle/winding preservation; explicit sparse overflow and invalid
  counts must fail without changing output.
- Python integration test checks producer short-mesh filtering, safe
  span arithmetic, stale worker cancellation and APK workflow integration.
- The pull request should run the Windows full native Android build and
  upload both Classic and G925 Native artifacts. A green build establishes
  compile/test/package status only, **not measured FPS**.

## Device comparison

Use the same save, scene, graphics, charger/thermal state and *normal Play*
(no active profiler) as 2.39. Compare minimum, average and p95 title frame
times, command recording, GPU frame time and audio/input stutter. The intended
improvement is fewer CPU spikes from low-value geometry jobs, not a guaranteed
40/60 FPS. Restore 2.39 if this build introduces rendering regressions.

No custom system driver, root, unsafe clock overrides or game files are added.

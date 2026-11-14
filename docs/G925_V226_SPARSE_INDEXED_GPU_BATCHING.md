# G925 2.26 — reduce *actual* Vulkan indexed draw commands with sparse triangle batches

Android: `versionCode 54`, `0.5.9.3-mali-alpha-2.26-g925-sparse-gpu-draws`.
Starting point: successful [2.25 full Mali ARMv9 build](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42531451265).

## Why this is different from earlier fast-path optimizations

The previous 2.20 phone capture showed about 3,565 actual GPU draws per frame,
about 2,727 indexed record operations, 21.39 ms average GPU frame time and
20.27 ms native command recording. A 60 FPS period needs 16.67 ms. Cache and
index decoding improvements save CPU cycles without removing much GPU work.

Version 2.24 merged rectangle strips, and merged *adjacent* index/vertex runs.
That left a big opportunity for draws that use the **same resources/state**
but reference **nonadjacent regions of one index buffer**. Such a group
previously still needed N separate Vulkan draws.

## Changes

- The recorder searches up to 64 consecutive indexed triangle-list commands.
  It admits a *sparse* batch only when at least four are identical under
  strict shader/pipeline, vertex/index buffer, textures, descriptors, constants,
  light, render-target phase, fixed/raster state and base-vertex checks.
- Each original 16-bit/32-bit host-endian index slice is copied in exactly
  guest order to a bounded combined upload, at most 65,536 indices (128 KiB
  for 16-bit or 256 KiB for 32-bit). No index is reinterpreted, triangle
  reordered, skipped or synthesized.
- **One** Vulkan `vkCmdDrawIndexed` draws the combined list. That removes
  `N-1` actual draw commands, without changing texture/shadow resolution,
  shader precision or graphic quality. Original zero-copy merging wins for
  contiguous runs; sparse batching must save at least two *additional* draws
  before the upload is worth considering.
- Opaque/non-stencil, non-temporal, non-query/non-probe paths only. Vertex-index
  shaders and non-triangle primitives are excluded. Existing vertex-cache
  triangle reordering is not combined with a sparse batch, avoiding
  accidentally reordering its independent draw ranges.
- On upload, pipeline, bound buffer or state failure, the recording path
  clears the temporary override and retries the untouched first draw;
  subsequent original commands remain in the normal queue.
- The new native profile counter `sparse-indexed-draws-merged` counts actual
  Vulkan draw commands eliminated. If it is zero in the player's scene,
  **this change will not materially increase FPS there**.

## Validation and performance honesty

`tools/test_g925_sparse_index_batch_v226.py` builds a C++20 executable
with thousands of randomized 16-bit/32-bit index runs, duplicate and
nonadjacent spans, exact source-to-destination verification, unaligned
buffers, canary checks and invalid bounds that must never write partially.
Production integration checks the state/query exclusions, no-fallback-loss
path and native performance counter. Existing tests stay enabled.

This improves the work performed *when eligible runs occur*. It is not an
on-device FPS benchmark. Repacking indices adds a bounded upload, which can
outweigh drawcall savings for short runs; the conservative admission policy
and the runtime counter make its actual usefulness observable. Phone FPS
and p95 frame times must be measured before claiming a numerical gain.

Known-good rollback: [2.25](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42531451265).

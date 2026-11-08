# G925 v2.14: batched exact CPU cache optimization

Android package versionCode 42, `0.5.9.3-mali-alpha-2.14-g925-cache-batch`.
Follows **2.12 verified** native presenter overlap and the **2.13** exact
quad/restart index conversion without changing game graphics, save files,
resolution or the user's Fast 16ms / Stable ADPF settings.

## Multiple improvements in one APK

1. **Index conversion** (from v2.13): Native 16-/32-bit all-ones restart
   markers can bulk-copy, and quad ABCD -> ABC ACD can use three exact
   byte-copy groups. Masked 32-bit restart indices still convert. No pixel,
   triangle, draw order or shader changes.
2. **FrameGenerationMap insertion**: The old `Insert` called `Find` and
   then `InsertWithoutGrowth`, recomputing a hash and walking the probe
   chain twice on every new entry. New `Insert` detects duplicates and
   finds free generation-stamped slots with one hash + probe sequence.
   At the established <=50% occupancy bound, resizing still uses the
   existing rehash/insert path. Exact-key comparisons still guard
   collisions. The same map serves frame constant reservations and
   converted-index allocation metadata.
3. **Surface lookup associativity**: The 64-entry recorder cache is now
   2-way set-associative. Two exact surface variants sharing a handle/set
   can coexist instead of evicting one another every alternating draw.
   Each hit requires the full descriptor, registry revision and matching
   invalidation epoch. `Capacity=1` remains a one-way fallback.
4. **O(1) surface invalidation**: Releasing/replacing a resource bumps a
   cache epoch instead of clearing all entries; stale image pointers are
   never returned. Wraparound clears every entry before restarting
   the epoch counter. Existing registry revisions and GPU fence/resource
   lifetimes remain authoritative. No new GPU buffer lifetime is added.

## Validation

- New `tools/test_g925_cache_batch_v214.cpp`: exact duplicate identity,
  one hash computation for ordinary insertion, 120 generations of forced
  hash collisions, random lookup streams and constant-buffer slot fence
  protections.
- Existing `tools/test_g925_draw_lookup_v28.cpp` extended: 2-way alternating
  surfaces, three-way eviction, changed registry revisions, 10,000 epoch
  invalidations and stale pointer non-observability.
- Existing index converter tests cover 20,000 reference-equivalent random
  16-/32-bit streams, bounds, masks and unaligned memory.
- New C++ test is a required step in the full native APK workflow along
  with previous Android/Java/runtime regression tests.
- Source changes do not bypass guest validation, Vulkan synchronization,
  slot fences, render thread admission, texture cache budgets or the
  native renderer's error/recovery paths.

## What is deliberately NOT activated

- Unbounded render ahead, increasing frames in flight, undocumented driver
  overclocks or modifications to OS GPU drivers.
- Persistent GPU index uploads across independently submitted frames; that
  needs reliable GPU fence completion and memory-pressure eviction rules.
- Auto-lowering resolution, shadows, draw distance or other visual quality.

The 2.12 verified APK is retained as rollback:
https://github.com/ali00978/ZMOD-Liberty/actions/runs/41734118290

The 2.14 branch must pass a complete GitHub Actions Android native build.
Phone-side FPS improvements are not yet measured; no 60 FPS guarantee.

# ZMOD Liberty G925 2.22 — reduce repeated native draw-state comparisons

**Android versionCode:** 50. **Mali ARMv9 versionName:**
`0.5.9.3-mali-alpha-2.22-g925-raster-hot`.

Based on **verified 2.21 Android APK**, commit
`254bcf5736cfaaa512a517921c1dfcbd0d8aa6e4`.
[Verified 2.21 build and rollback](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42431895529)

## Why these paths

The user's detailed 180-frame 2.20 capture observed:
- Approx. 3,565 Vulkan draw operations per frame
- Approx. 6,828 native surface lookups reused per frame
- CPU command recording 20.27ms average, 23.65ms p95, 35.10ms maximum
- GPU 21.39ms average, 29.82ms p95; mean title interval 36.11ms

Recording and GPU work can overlap, and detailed profiling adds overhead.
The GPU is also over the 16.67ms budget for 60 FPS, so these CPU-only
changes **cannot guarantee 60 FPS**.

## New native optimizations

1. **Actual last-consumed raster area wins.** The two-entry
   `NativeExactMemo::Get` path previously updated recency on Store, not
   when a previously cached raster state was consumed. On an A/B/A/C draw
   sequence, it could evict A (the freshly consumed state), forcing an
   expensive viewport/scissor selection the next time A appeared.
   `Get` now promotes a hit in the older slot. On misses it tests the two
   exact keys only once, computes the new raster area without touching
   existing entries if computation throws, and directly replaces the older
   slot. The const `Find` remains a non-promoting peek and `Store` retains
   its existing explicit replacement behavior for other call sites.

2. **Cheap color/depth hot-key rejection.** The recorder's existing
   two-entry Vulkan surface MRU can inspect each candidate's guest handle,
   depth role and sample count first. Different color and depth targets
   often fail this cheap check without comparing all nine descriptor words.
   The full exact descriptor, current registry revision, non-null pointer
   and invalidation epoch remain *mandatory* for a hit. Release,
   reflection, backing-cache eviction, and GPU-in-flight tracking remain
   unchanged.

No shader or pipeline edits, no fewer legitimate draws, no graphics quality
changes, no renderpass merging, no resolution/distance downgrade, no ADPF
or CPU clock changes and no Vulkan fence removal. Prior v2.7–2.21
optimizations are retained.

## Automated verification

`tools/test_g925_raster_surface_hot_v222.cpp` plus a required Python
integration runner cover: true MRU with A/B/A/C, countably one comparison
per resident key on Get miss, throwing computation leaving old entries valid,
legacy Store and Find semantics, generation/snapshot independence,
150,000 alternating surface hits, same-handle/different-format collisions,
all descriptor fields, sample/depth roles, registry revisions, and
invalidation epochs. The **full Mali ARMv9 native APK** must also pass all
prior C++/Java regressions in GitHub Actions.

This is a targeted CPU hot-path reduction, not an on-device measured FPS
gain. Evaluate frame intervals/p95 without capture overhead on the same
scene before claiming performance changes.

## About the previous CPU hint report

The phone's launch-time diagnostic printed the saved/raw
`android_performance_hint_target_ms=20`; production
`G925EffectiveHintTargetMs(20)` migrates that older Maximum setting to
a **16ms requested CPU work target**. This is Android ADPF guidance,
not a verified clock speed or FPS cap.

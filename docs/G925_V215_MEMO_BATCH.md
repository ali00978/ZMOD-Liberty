# G925 v2.15 – one bundled CPU/draw-cache update

Android versionCode **43**, versionName
`0.5.9.3-mali-alpha-2.15-g925-memo-batch`.

Base: v2.14 commit `7b2d1979212967b17e479f67f43864241cd6c5aa`,
verified native/Android APK workflow
https://github.com/ali00978/ZMOD-Liberty/actions/runs/41749872509

## Improvements combined into one APK

1. **2-way exact CPU memo for derived raster and constant/allocation data**:
   a memo can retain two exact identities instead of the latest one. Alternating
   viewport/scissor combinations (and alternating frame-local constant identities)
   no longer repeat CPU computation every other draw. `Reset()` invalidates
   both entries and still owns no GPU resource.
2. **2-way exact vertex-layout memo**: an immutable pipeline snapshot can retain
   two verified vertex stream masks. Full pointer identities, input signatures
   and declaration hashes must match on each hit. Existing entries refresh in
   place or promote to the recent slot. Snapshot copies remain independent.
3. **Bounded incremental descriptor-cache eviction**: the per-pool resolve
   descriptor cache used to clear all 256 valid descriptors when a 257th key
   arrived, and even when updating an identical full-table key. It now checks
   duplicates first, reserves the target bucket count and evicts only one
   arbitrary descriptor when the cap is reached. The map stays capped at 256.
   `BeginScope`/pool switch and `Reset` still invalidate every borrowed
   descriptor. Vulkan descriptor handles remain owned by their original
   descriptor pool and must not outlive it.
4. All verified v2.14 changes are included: one-pass generation-map inserts,
   2-way surface lookups, constant-time invalidation, exact quad/restart index
   conversion and the bounded presenter-admission overlap.

Graphics and texture settings, frame cap, run-ahead limit, shader paths,
GPU resource/fence ownership, Stable thread mode and Fast 16ms ADPF response
are **not** changed. No persistent GPU buffer caching and no driver overrides.

## Tests

- `tools/test_g925_memo_resolve_v215.cpp` verifies 20,000 A/B exact memo
  lookups perform only two value computations; exact three-way eviction,
  reset/independent copies, 20,000 A/B vertex layouts and altered signature
  rejection. It verifies filling 256 resolve descriptors, no eviction on
  duplicate insert at capacity, one-only eviction at overflow, 10,000
  bounded insertions and both command-buffer and descriptor-pool invalidation.
- Existing raster regression now alternates 10,000 times between two exact
  viewport/scissor keys, then tests eviction and reset. Existing vertex-layout
  regressions test A/B reuse, signature changes and copy independence.
- The Android GitHub Actions full-native build must run the new test via
  `tools/test_g925_memo_resolve_v215.py` plus all existing C++ / Java
  regression gates before producing an APK.

No on-phone FPS improvement is claimed; no device test required until this
bundle passes CI. Because changes target hit rates only, their real benefit
depends on how often the game alternates these cache keys.

## Rollback

Known-good compiled APK:
https://github.com/ali00978/ZMOD-Liberty/actions/runs/41749872509

For the next iteration, use CPU detail and cache hit counts before making
higher-risk renderer scheduling or GPU memory changes.

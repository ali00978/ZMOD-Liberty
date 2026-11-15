# G925 2.16 – batched native renderer hot paths

**Version:** `0.5.9.3-mali-alpha-2.16-g925-render-hotpaths`, Android versionCode 44.
Built on the successfully compiled **2.15** base commit
`435bbedd4b334fddd3669037ec47c9dac9ff5596`:
https://github.com/ali00978/ZMOD-Liberty/actions/runs/41763984704

## Three new native CPU optimizations in one APK

1. **Fast repeated pipeline lookup:** the four-entry native Vulkan pipeline
   memo now checks its most recently matched pipeline first. This skips
   repeated fixed-function and attachment-context comparisons when a series
   of draws uses one pipeline. Both fixed-state and prehashed-signature paths
   still validate exact snapshot owner, Vulkan pipeline lifetime, render
   attachment context and state identity before accepting the entry.
   The entry count remains four: no increased snapshot memory budget.

2. **Better native render-surface cache indexing:** the two-way/64-entry
   recorder cache now distributes surface variants using format, packed
   dimensions and guest address, in addition to handle, sample count and
   depth role. The previous handle-only set index made every format variant
   of a surface compete for two entries. The exact descriptor comparison,
   registry-revision check and lifetime epoch still guard every hit. Hash
   collisions still cause only cache misses, never an incorrect surface.

3. **One-entry hot resolve descriptor:** the command-buffer/pool-scoped
   Vulkan resolve descriptor map retains the last exact resolved key/handle.
   Repeated resolves of the same target can bypass an unordered_map hash and
   lookup. Every pool/command-buffer switch and Reset invalidates the hot
   entry before the original pool-owned descriptor can expire. Duplicate
   inserts continue returning the first valid descriptor, and the map remains
   capped at 256 entries.

All v2.7–2.15 native improvements are retained, including bounded presenter
overlap, exact index conversion, per-frame generation cache optimizations,
two-entry viewport/vertex memos and incremental descriptor eviction. Fast
16ms feedback and Stable CPU hint policy remain untouched. No dynamic graphics
quality reduction, overclocking, driver hacks or unbounded frame lead is added.

## Automatic validation

`tools/test_g925_render_hotpaths_v216.cpp` validates 30,000 exact-pipeline
lookups with only one fixed-state comparison on the repeated-entry fast path,
signature and owner/lifetime rejection, four-entry eviction, 24 distinct
format variants of one surface handle repeated 1,000 times, invalidation
epochs, 20,000 exact repeated descriptor lookups and 2,000 bounded inserts
across pool and command-buffer changes. The production-header integration
is tested through `tools/test_g925_render_hotpaths_v216.py` and is a required
step before the full Mali ARMv9 native APK compilation in Actions.

The changes target CPU recording overhead. They do not guarantee higher FPS,
and the device has not yet confirmed their impact on frame intervals or GPU
work. The single phone benchmark should wait for the whole batch to pass CI.

## Rollback

Known-good v2.15:
https://github.com/ali00978/ZMOD-Liberty/actions/runs/41763984704

## FPS-drop / frame-spike mitigation (same 2.16 release)

The maintenance path previously destroyed up to 12 completed native
Vulkan surfaces in a single ordinary frame, and reclaimed up to 16
under-budget host buffers (4 MiB). To reduce occasional CPU frame spikes,
normal-frame work is now limited to **6 surfaces** and **8 host buffers
(2 MiB)**. GPU-in-flight resources are still protected by the existing
completed-submission fence; paused/forced drains retain their unlimited
cleanup behavior, and **over-budget emergency reclamation remains 64
buffers/16 MiB**. This is bounded spreading of cleanup work, not
disabling resource reclamation or retaining unlimited memory.

A second compiled C++ test validates normal/emergency/forced cleanup
budgets and in-flight resource safety. It runs as part of the same
mandatory 2.16 CI test step. Device impact on FPS drops is not yet
measured.

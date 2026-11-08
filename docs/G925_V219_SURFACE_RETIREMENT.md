# G925 v2.19 — bounded native Vulkan surface retirement + indexed release

**Version:** `0.5.9.3-mali-alpha-2.19-g925-surface-release-scan`, versionCode 47.
Base: **2.18** commit `3bf09d1cf035e42f1a3ff2c82779f806c1675602`.
Previous full native Android CI APK completed successfully:
https://github.com/ali00978/ZMOD-Liberty/actions/runs/42317651733

## Why this is different from a speculative FPS slider

Native command recording remains a measured bottleneck. The existing 2.17
resource cleanup quota limited GPU surface destruction to **six resources
per title frame**, but it could still walk **all live surface images**
for *each* housekeeping frame. In scenes with many surfaces and a few
deferred releases, this is an unbounded CPU pass even if it destroys
nothing. A queued release also walked the entire surface vector despite
a maintained handle -> live-image index being available.

Both loops are now changed in production native C++ code:

1. **Bounded, resumable retirement scan**: a normal frame visits at most
   **512** surface image entries before deferring any remaining search to
   the next title frame. The position persists across frames and wraps
   safely, including after vector erase/compaction and newly created
   surfaces. The existing **6 GPU-completed resource retirements per
   frame** remains a second independent limit. GPU-in-flight images
   never retire before their completed submission, and pending lifetime
   IDs remain excluded from reuse.
2. **Indexed release request**: `QueueSurfaceImageRelease(handle)` now
   uses the existing `surface_images_by_handle_` map instead of searching
   the whole `native_surface_images_` vector. Creation adds every new
   image to the index and destruction removes it. All live generations
   for the requested handle are still queued, without risking use of a
   recycled handle or stale raw pointer.
3. **Forced/no-frame cleanup unchanged**: when the game explicitly
   drains while idle, scanning remains unbounded across the initially
   live vector and still retires *only* resources whose GPU submissions
   completed. Shutdown retains the original full release path.

We deliberately preserve vector insertion/erase order: other recording
paths iterate the vector and can rely on its order. We also preserve
real Vulkan fences, aliases, descriptor-set lifetimes, placement-owner
unlinking and memory-pressure emergency policy. No shaders, graphics
settings, draw distance, thermal guard, 16ms ADPF/Stable settings or
frame caps change. Existing v2.7–v2.18 enhancements remain.

## New tests

- `tools/test_g925_surface_retirement_v219.cpp` replays **30,000**
  surfaces with releases at the end of the scene to confirm the 512
  scan bound and eventual (non-starved) cleanup; validates 300
  completed/unfinished GPU lifetimes across normal frames, 1,000
  completed idle lifetime drains, additions after an empty vector,
  cursor wrap, six-per-frame destruction and fence protection.
- Python integration test verifies the production renderer uses the
  exact scan helper, persists the cursor, avoids full-vector release
  scans and maintains per-handle index creation/destruction. The
  mandatory full Android workflow runs this test and **all preceding**
  regressions before compiling native Vulkan C++ and packaging an APK.

**This aims at CPU frametime spikes and may improve FPS lows, but cannot
guarantee an average FPS increase**. The title's GPU rendering time is
unchanged. Avoid frequent device tests: once CI passes, one comparable
gameplay pass can establish whether the drops improve. Retain the 2.18
APK as the known-good rollback.

[Verified 2.18 APK](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42317651733/artifacts/13552556111)

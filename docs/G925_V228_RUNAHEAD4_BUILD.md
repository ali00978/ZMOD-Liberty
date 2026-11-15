# G925 2.28 — four guest run-ahead frames, four native queue slots, faster native builds

Android `versionCode 56`, `0.5.9.3-mali-alpha-2.28-g925-runahead4-fast-build`.
Base: [verified 2.27 Mali ARMv9 APK](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42563504041).

## Why

The existing phone captures show BOTH the native CPU command recorder and GPU
frame time above the 16.67-ms frame budget for 60 FPS. Earlier default native
settings allowed 3 queued guest frames, and the title's own frame-wait hook
allowed zero *additional* frames. That can force the guest game thread to wait
while the renderer and presenter have independent work in flight.

## Changes

1. `gta4_extra_frames_in_flight` now accepts 0-4, defaulting to **4 additional
   guest frames** beyond the GTA IV title's original 2-frame pending limit.
   This changes only the reported value observed by the guest wait loop.
   It DOES NOT falsify GPU-completion fences, release GPU resources early,
   remove a GPU wait, or change game simulation rate.
2. `g925_run_ahead_policy.h` saturates subtraction and clamps the extra
   frames to four even with a bad persisted setting.
3. The native renderer's existing **bounded** queue remains capped at four
   (`gta4_native_max_queued_frames` already permits 1-4). The default
   bundled Android args and migration of the old bundled 3-slot value
   select 4. No unbounded GPU memory queue is introduced.
4. Android Graphics > Performance now exposes 0/1/2/3/4 extra frames.
   On upgrade, a one-time migration changes a saved old-default 0 to 4
   (or inserts 4 if the setting was never written); explicit 1/2 selections
   remain untouched. It never touches saved games or graphics assets.
   Once migrated, users may select any setting, including 0, without the
   migration overriding it on subsequent launches.
5. Full Android builds on four-vCPU GitHub runners now select **three**
   compiler jobs instead of two, reserving one CPU for Windows and sccache.
   Cached native builds continue to use content-addressed sccache.
   The release changes were committed on a PR-free feature branch; opening
   a single PR at the end avoids a cancelled full APK build per commit.

## What does not change

No display resolution, draw distance, shading, GPU driver, Vulkan
synchronization, frame cap, texture size or shadow quality is reduced.
The verified 2.27 Android notification/surface/audio fixes remain included.
The "CLEAN ORIGINAL SETTINGS + 60 FPS" benchmark reset still selects zero
extra frames explicitly, for a usable comparison baseline.

## Risk and measurement honesty

Extra game-frame lead can improve throughput only if game/renderer overlap
was the bottleneck. It **cannot** shorten an individual 21-ms GPU frame and
may worsen input latency, audio/video alignment, memory pressure or stalls
in some scenes. If responsiveness worsens, set Run-ahead frames back to
0, 1 or 2. There is no claim of a specific FPS gain without a same-scene
phone comparison and unique displayed-frame observation.

## Validation

`tools/test_g925_runahead_v228.py` compiles and runs C++20 verification
with 200,000 randomized original/extra pending pairs and additional
compile-time edge cases; checks production integration, 0-4 launcher
selection, existing-install migration, bounded native queue, preserved
clean benchmark and GitHub build job configuration.
The new test is mandatory in the full Mali ARMv9 APK workflow. All earlier
native C++ and Java regressions remain mandatory.

Rollback: [2.27 verified build](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42563504041).

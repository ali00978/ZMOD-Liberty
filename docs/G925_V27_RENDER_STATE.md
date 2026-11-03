# G925 2.7: native state recording batch

Base: `feature/g925-stable-16ms-v26`, commit `6d67ace239ab7b6babe88b530fed1ad11b27106e`.
Development branch: `feature/g925-render-state-v27`.
Android version: `0.5.9.3-mali-alpha-2.7-g925-render-state`, version code 35.

## Implemented

- Retain viewport, scissor, depth bias, stencil and blend constants between native pipelines. The native pipeline builder declares all seven corresponding Vulkan states dynamic. External/static passes retain conservative invalidation, as do command-buffer boundaries. Push constants retain their previous layout rule.
- Emit only changed stencil reference/compare/write fields. Equal front and back values use one combined command; differing face values remain distinct.
- Deduplicate index-buffer binds by command buffer, buffer, full 64-bit offset and index type. All six native indexed paths, including fans, quads and merged immediate draws, pass through the tracker. A command-buffer reset invalidates it even when the handle is reused.
- Reuse recorder-owned texture-command pointer scratch capacity. Scope-exit clearing covers failed preparations as well as successful frames, so no pointers survive frame retirement. Resource ownership and descriptor lifetimes are unchanged.
- Give newly observed live main/recorder threads priority in a full stable ADPF session. Background workers still have the existing two-miss hysteresis. This fixes up to two sampling intervals in which an old noncritical member could block a new critical worker.
- Capture whitelisted settings after final launch argument overrides, with saved native configuration as the lower-priority source. Previously the report could claim the graphics-file heat setting while the performance-test Intent overrode it. The game publishes a bounded snapshot through atomic file replacement; the launcher reads it fresh and matches the exact capture token passed in the Intent. This avoids cross-process SharedPreferences caches and rounded file timestamps. Missing, incomplete, oversized or different-session snapshots retain the explicitly labeled pre-launch fallback.
- Expose automatic draw-distance and reflection FPS guards in the launcher, and warn in the report when automatic quality reductions are enabled. No saved choices are reset or silently changed.
- Build the child branch with a pull request targeting the verified 2.6 branch. This grants access to the base branch cache without changing 2.6 or building the older default branch. APK checkout explicitly selects the PR head commit. Desktop workflows remain for main-targeting PRs only. Persist sccache human/JSON statistics as a separate artifact and in the run summary.

## Preserved

Application ID and signing key, saved games, existing graphics/performance settings, Fast 16 ms feedback, Stable selection, current target/cap, native Vulkan, render pass ordering, synchronization/barriers, shader code, texture formats, resolution, shadow and reflection settings, frame queues and memory budgets. No system driver, overclock or OS thermal-safety modification.

The 2.6 bundled configuration already has `gta4_dynamic_draw_distance=true` (floor 0.80) and `gta4_fps_guard=2`. `android_thermal_guard=true` is bundled too, but the launcher performance-test Intent can override it. These are existing quality compromises, not new performance improvements. Use the new report's final applied settings to interpret the comparison. Changing them between tests would invalidate an equal-quality comparison.

## Validation

- Nine existing APK workflow suites pass locally: fast APK compatibility/staging, clean recovery, frame timing, pipeline admission, sustained hints, presenter telemetry, bottleneck capture, batched CPU and stable 16 ms.
- Standalone C++20 executable performs 20,000 mixed dynamic/static/reset state replays and 20,000 stencil field replays against the state actually emitted to a driver model. Also covers index-type/offset/command-buffer changes, exact floating-point bits, critical-thread admission, dead TIDs, capacity, duplicate candidates and hysteresis.
- The executable passes optimized compilation with warnings as errors and AddressSanitizer/UndefinedBehaviorSanitizer. Leak detection is disabled locally because the executor prevents LeakSanitizer's `/proc` thread inspection; no leak-check result is claimed.
- A synthetic 1,000 pipeline-switch workload demonstrates removal of redundant state commands with identical modeled draw state. This is not a phone performance benchmark.
- Java 17 behavioral tests compile the actual report class with only a BuildConfig stub. Cover actual launch precedence, saved values, quality warnings, privacy whitelist, missing timings, statistics and rejection of stale captures. A separate JVM writes the final snapshot while the reporting JVM retains stale state; checks cover exact session matching, replacement, rounded/future file timestamps, corrupt/oversized/missing files and cleanup after write failure. These also run in the Java-only packaging job.
- Java-only packaging rejects native changes on either side of a renamed file. The follow-up report fix uses the completed 2.7 native build only after the source/artifact compatibility guard passes; it does not repeat native compilation.
- PR updates check at most three successful APK runs on the same branch. Only a successful compatibility check selects Java-only packaging; native edits, unrelated branches, expired/missing artifacts and unavailable verification select a full build. Both modes check out the exact PR head, and the fast job independently revalidates its chosen baseline before downloading it. Manual full builds remain available.
- Repository metadata and `git diff --check` pass. The old `test_g925_graphics_lab.py` describes a removed v1 launcher and is not part of the 2.6/2.7 APK test suite; its historical assertions are not treated as current release gates.
- Full NDK C++/Android packaging and APK/signature inspection are checked in the linked GitHub build. On-device graphics and FPS still require the single test below.

## Build/cache baseline

2.6 successful run: https://github.com/ali00978/ZMOD-Liberty/actions/runs/41641435960

The 2.6 native step took about 44 minutes. Sccache reported 42 hits / 971 misses (4.15%), zero read/write errors, zero noncacheable compilations. The build-directory cache was not found. GitHub cache isolation prevents ordinary push runs from restoring a sibling/parent development branch cache; PR runs can access their base branch's cache. Actual 2.7 cache results must be read from this run, not assumed.

2.7 full build succeeded: https://github.com/ali00978/ZMOD-Liberty/actions/runs/41653938119

The full workflow took 23 minutes 36 seconds, versus 55 minutes 16 seconds for 2.6. The 2.6 build-directory cache was restored. Sccache recorded 927 hits / 86 misses (91.51%), zero read/write errors and zero noncacheable compilations. The resulting APK is the native baseline for the final Java-only report correction. These are build-time measurements, not gameplay FPS measurements.

## Remaining limits and one device test

At the supplied 2.6 sample, recording is 22.52 ms average / 28.42 ms P95; GPU time is 20.89 / 28.79 ms, total title interval 38.59 / 51.49 ms. CPU ranges overlap. Even if CPU submission improved substantially, the measured GPU workload is above the 16.67 ms budget for 60 FPS. This batch reduces redundant CPU/driver command work; it does not claim less shading, guaranteed 60 FPS, or a measured percentage gain.

Install as an update without deleting the app or its data. Keep Fast 16 ms, Stable, target 20 ms, 60 FPS cap and the same in-game graphics. Start one CPU + GPU bottleneck capture, load the same save/location, follow the same short route for about two minutes with a similarly cool phone, then share the native report. Check recording average/P95, title interval/P95, GPU average/P95 and visual correctness (shadows, reflection, transparency, HUD). No repeated manual testing is requested.

Rollback remains the unchanged 2.6 branch and APK artifact at the baseline run. Android may refuse a normal install of version code 34 over 35; do not uninstall to work around this because it can remove app data. A data-preserving debug downgrade uses `adb install -r -d <2.6.apk>` where the device allows it.

## Technical references

- Vulkan dynamic state lifetime: https://docs.vulkan.org/spec/latest/chapters/pipelines.html#pipelines-dynamic-state
- Index buffer state: https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindIndexBuffer.html
- Android Performance Hint API: https://developer.android.com/ndk/reference/group/a-performance-hint
- GitHub cache branch scope: https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching#restrictions-for-accessing-a-cache
- Android SharedPreferences process boundary: https://developer.android.com/reference/android/content/SharedPreferences

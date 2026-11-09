# G925 2.18 – high-performance CPU hints + repaired anti-stutter build

Android versionCode 46, `0.5.9.3-mali-alpha-2.18-g925-16ms-anti-stutter`.
Based on 2.17 commit `d1a19ddb8192ca2949c9fc939dd879d2a7657660`.
The 2.17 Android workflow run 41771863600 **failed in host regressions**,
before native Android compilation: it expected the old per-frame cleanup
quotas (12 surfaces and 16 host buffers), while the actual source enforced
the newer stricter quotas (6 surfaces and 8 buffers). It had **no usable APK**.

## New combined fixes in 2.18

1. **Repair the build gate.** The v2.17 host Python and C++ tests now
   explicitly verify actual normal-frame limits of six GPU-completed surface
   retirements and eight host-buffer retirements / 2 MiB. Work under cache
   pressure remains 64 resources / 16 MiB, and forced no-frame drains remain
   unbounded for *completed* resources. GPU-in-flight lifetimes remain pending.
   The 300-surface C++ scenario requires 45 normal frames to retire 270
   completed objects, then 10 to retire the remaining 30 after GPU completion.
   This matches the actual 2.17 implementation and the v2.16 budget test.

2. **More aggressive Android CPU performance request for 60-FPS-oriented
   gameplay.** The native ADPF target defaults to 16ms of CPU work, instead
   of 25ms. The earlier GUI **Maximum** setting serialized a target of 20ms,
   which is now interpreted as 16ms. New UI offers Maximum (16ms), Steady
   (25ms), Balanced (30ms), Battery (33ms). Lower-power choices and their
   serialized values remain unchanged. This is an OS performance request,
   **not a frame cap**, forced core clock, or CPU overclock. Android governs
   frequency/power/thermal behavior, so actual FPS improvement is unknown
   until a phone test. The existing 16ms CPU *feedback interval* and Stable
   thread-hint setting remain selectable and untouched.

3. **All previous v2.7–2.17 native optimization and stutter controls
   preserved**, including 2.12 bounded presenter overlap, exact index
   conversion, state caches, native pipeline/surface/descriptor hot paths,
   and phased surface/host cache cleanup. No resolution/shadow/texture
   downgrade, driver hack, GPU-fence bypass, run-ahead expansion, or
   persistent cross-frame GPU memory change.

## Verification

- New pure native target-policy header `g925_hint_target_policy.h` with
  constexpr mapping, compiled C++ test and required Python integration
  runner. Tests 16ms default, upgrade of old 20ms Maximum, unchanged 25/30/33
  modes and 10–50ms range checks.
- Corrected Python+C++ real cleanup regression. Both are mandatory in the
  full Android CI before native C++ compilation and APK packaging.
- One clean, full Mali ARMv9 GitHub Actions APK build must complete before
  delivery. Static/host tests cannot establish actual phone FPS.

## What to compare after a verified build

Use a single representative two-minute gameplay sequence with a repeatable
scene, without intrusive profiler first. Compare FPS *and p95 frame intervals*,
not only the maximum FPS. If the scene remains choppy, take one CPU/GPU
detailed capture to determine whether presentation, CPU command recording
or GPU frame time dominates. If the 16ms CPU mode runs hotter and performs
worse over long play, use Steady 25ms in the launcher; no reinstall is needed.

Known-good rollback: successfully built v2.15
https://github.com/ali00978/ZMOD-Liberty/actions/runs/41763984704

No unmeasured +FPS or 60 FPS claim is made.

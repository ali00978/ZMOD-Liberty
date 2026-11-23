> **RETIRED in v2.42 at the user's request.** The 540p, reduced-population
> and reduced-draw-distance Max FPS preset was removed before release.
> This document records a superseded experiment, not current app behavior.
> The active profile preserves every graphical quality setting; see
> [G925_V242_FULL_QUALITY.md](G925_V242_FULL_QUALITY.md).

# G925 Native v2.41 — actual frame-work reduction (Max FPS experiment)

This release keeps the v2.40 background-vertex safety fixes and adds an
explicit **Max FPS (low graphics)** action to the independently installable
G925 Native launcher. It is designed for the POCO X8 Pro Max / Immortalis-G925
and requires no root or custom system graphics driver.

## Prior measurements (2026-11-21, 180-frame profile)

- Title interval: 37.72ms mean (26.5 FPS equivalent); p95 47.99ms.
- GPU time: 21.78ms mean; CPU native command recording: 20.25ms mean.
- Profiler overhead applies, CPU stages overlap and frame intervals do not
  necessarily equal actual display timing. A 60 FPS target is a 16.67ms budget.
- Prior 2.39 40 FPS profile and v2.40 worker changes still require on-phone
  testing. No FPS number from this release is a guarantee.

## New Max FPS toggle

- Internal render height **540p** (vs. 720p in the 40 FPS profile). For the
  same aspect ratio, 540p costs **43.75% fewer output pixels** than 720p.
  Pixel shaders and bandwidth may cost less; vertex/draw-command overhead
  is not guaranteed to fall from resolution reduction alone.
- Draw distance **60%** and traffic/pedestrian density **20%** to reduce
  scene simulation and render work in dense city scenes.
- No anti-aliasing, console-sized shadows, reflection AA disabled; existing
  dynamic distance and FPS guard stay enabled. Visual quality is lower.
- FPS cap **60**, not forced 60 FPS. It does not synthesize frames or use a
  GPU driver replacement.
- Run-ahead **1** for lower queued-frame latency, Android hint feedback
  **16ms**; preserve the user's custom system graphics driver.
- Uses only already supported graphics dropdown/cvar values.
- Validates **all settings before making any changes** and backs up the
  original `graphics.txt` as `graphics-before-g925-maxfps.txt` once.
  No changes to saves or the original Classic app.
- The earlier 40 FPS profile remains available.

## Validation

- `tools/test_g925_max_fps_v241.py` checks every cvar against the actual
  graphics dropdown, that the selected profile never silently saves a
  partially valid configuration, the backup order and G925-only UI.
- Existing C++20 vertex-cache random replay/overflow tests remain in APK CI.
- The first v2.40 CI run did not reach native compilation because the v2.29
  test's version regex rejected alpha 2.40. The assertion is updated to
  accept future alpha 2.3x–2.9x versions without weakening rendering tests.
- APK versionCode **69**; separate package `com.zmodliberty.g925native`;
  artifact `ZMOD-Liberty-G925-Native-v3.2-armv9.apk`.

## What would count as a result

On the phone, apply Max FPS, restart and compare **normal gameplay** at
the same save and location as 2.39. Compare 60s stationary and 60s driving
for average FPS, p95 title interval, GPU ms, command recording ms, input/audio
stutter and battery/temperature. Both cap=60 and 540p must be visible in
`graphics.txt`. Target improvement is actual FPS/frametime, not a higher cap.
If low graphics do not improve FPS, the dominant bottleneck is still
CPU render/command work, and further native trace-guided optimization is
required.

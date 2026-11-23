# G925 Native v2.42 — improve FPS without changing image quality

**User requirement:** no lower resolution, no decrease in population,
draw distance, shadows, reflections, textures, upscaling, anti-aliasing,
or effects to gain FPS.

## Current state

Earlier v2.41 Max FPS (540p, 20% population, 60% draw distance) and the
earlier v2.39 40 FPS quality-lowering one-tap buttons are **removed** from
the launcher. They were experiments, and are NOT valid for this goal.

The **G925 Native / FPS BOOST (SAME GRAPHICS)** button now changes ONLY:

- `gta4_frame_limit=60` (cap, not forced achievable FPS)
- `gta4_extra_frames_in_flight=1` (guest scheduling / latency choice)
- `android_performance_hint_target_ms=16`
- `android_performance_hint_report_interval_ms=16`
- `android_performance_hint_stable_threads=true`
- `android_performance_hint=true`

Every visual cvar remains precisely as selected by the user. The system Mali
Vulkan driver, game data, and saves are unchanged. A first-use backup at
`graphics-before-g925-full-quality.txt` allows manual recovery.

A second explicit **Restore Previous Graphics Backup** button restores the
original `graphics.txt` from the older 40 FPS backup, or from the old
Max FPS backup if no earlier backup is present. The current configuration is
saved in `graphics-before-quality-restore.txt` before replacement.
Nothing is restored automatically: the user controls which visuals to keep.

The independent app package remains `com.zmodliberty.g925native`; release
versionCode **70**, suffix **2.42-g925-fullquality**, resulting APK
`ZMOD-Liberty-G925-Native-v3.3-armv9.apk`.

## Native performance work retained

The earlier v2.40 native vertex-optimizer changes reduce CPU overhead without
changing source indices, draw commands, textures or graphics options: skip
tiny/incomplete background optimization jobs, skip stale evicted geometry,
and protect sparse 32-bit index ranges against overflow. Original-order draws
remain the fallback. C++ replay tests exercise exact original triangle winding
and index preservation.

The latest previous on-device capture from 2026-11-21 shows the actual
bottleneck remains title/frame intervals around 37.72 ms average
(~26.5 FPS equivalent), 47.99 ms p95, Vulkan GPU envelope around 21.78 ms,
and native CPU command recording around 20.25 ms. The capture had profiling
overhead and CPU phases overlap. No 40/60 FPS guarantee follows from build
success.

## Validation

- Python tests verify only the six scheduling/CPU cvars are affected and all
  proposed settings exist in the UI, the old 540p and low-density one-tap
  presets are absent, and recovery uses an explicit backup.
- Native regression tests verify lossless 16/32-bit index handling and safe
  fallback.
- GitHub Actions must complete a full Armv9 Mali build; host tests alone
  do **not** prove faster FPS or correct rendering on the POCO.
- Evaluate the exact same game save/scene, **same graphical quality**, on
  the real G925 Native APK before claiming a measured FPS gain.

The next native optimization should rank exact CPU command recorder hotspots
and GPU pass attribution using the stored 180-frame CSV capture instead of
changing visual quality or repeating unmeasured fast-path guesses.

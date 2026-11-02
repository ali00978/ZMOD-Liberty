# G925 V4: preserve maximum native resolution and diagnose slow frames

This is an **A/B launcher experiment**, not a guaranteed FPS fix.
The user reported V3 as performing very poorly. Before modifying native renderer
code or GPU drivers blindly, compare costs at the *same native panel resolution*.

The APK reuses **exactly the V3 native code** (GitHub run 41370482543) if
the guarded Java-only fast-build succeeds. No native GPU/CPU engine code
or memory policy is changed by V4.

## Profiles

- **G925 NATIVE MAX / ALL HIGH** remains the one-time default:
  full panel native output, SMAA, anisotropic 16x, shadow 512,
  far/full reflections, draw distance 120%, population 100%, 60 FPS ceiling.
  This was observed to perform poorly; 60 is a limit, not a promised FPS.
- **G925 MAX NATIVE / STEADY 30 TEST** copies every MAX setting,
  changes only the frame limit to 30 and leaves resolution, reflections,
  textures, shadows and draw distance identical. Compare **frametime
  consistency** (not simply average FPS). This can be smoother if the device
  is actually capable of stable 30 FPS at MAX.
- **G925 NATIVE HIGH / STUTTER TEST** keeps the same full native
  output, no FSR upscaling, SMAA, 16x textures and high shadows; changes:
  draw distance 100%, population 70%, original/near environment
  reflections without extra reflection AA and guarded reflection updates.
  Targets expensive reflection/draw calls during fast driving.
  **These are real visual trade-offs**, not hidden optimization.
- Restore the first-launch profile with **G925 NATIVE MAX / ALL HIGH**.
  Previous pre-V3 backup still available through **RESTORE PREVIOUS GRAPHICS**
  if present.

After selecting a profile, press Save changes and Play (restart).

## How to isolate a bottleneck

Use System Vulkan. Turn on FPS, frametime and optional memory overlay.
Drive the same 60-90 second route, at similar thermal conditions, in 3 modes:
native MAX 60, native MAX 30, native HIGH 40. Note pauses greater than a
quarter-second, FPS lows, battery temperature and whether artifacts appear.

Interpretation:
- If MAX 30 is smooth, the game may be **frame-pacing-limited** and the
  expensive 60 FPS ceiling is unnecessary on this device.
- If MAX 30 still stalls but native HIGH 40 improves, very expensive
  reflections or world/population complexity probably matter.
- If all modes freeze for several seconds, investigate shader compilation,
  allocator, texture streaming or blocking queues with an Android
  **Play with Debug Log** capture.

The changes in V2/V3 (texture retention, geometry cache, memory image
pool) are also experiments; no on-device benefit has been demonstrated.

## Build

GitHub fast APK workflow compares the v3 successful source commit with
this branch and refuses to build if any native source changed. The prior
APK supplies native libraries, while Java launcher and package version
are rebuilt. This is only safe for an **exact compatible source and
package signing key**. If validation fails, do not bypass it; use full build.

Static test: `python tools/test_g925_v4_diagnostics.py`.

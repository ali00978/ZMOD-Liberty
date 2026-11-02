# G925 Performance v2 — smooth graphics / memory experiment

**Status: experimental source; test on the actual POCO X8 Pro Max before claiming
any FPS improvement.** Targets system Mali Vulkan and ARMv9. Does not install
a root GPU driver or modify the device's firmware.

## Why this exists

The successful G925 Graphics Lab v1 APK mostly introduced launcher controls.
It did **not** automatically replace existing in-game High graphics settings.
The user reported persistent stalls/freezes on the Mali G925 device, with the
old High settings unchanged. The exact symptom could be either frame stutter
or a genuine non-responsive freeze, which need different diagnosis.

## Changes from G925 Graphics Lab v1

- **G925 High Smooth (recommended):** 1080p output with FSR1 Quality (lower
  internal render resolution), SMAA, 8x anisotropic filtering, 512-size
  shadow maps, original reflections near the camera, reflection AA off, 40 FPS
  cap, dynamic draw distance, low background population, 20ms ADPF CPU hint,
  on thermal guard and no extra title run-ahead. Intended to retain high visual
  quality while shedding expensive work at the fastest driving views.
- **G925 Native Detail:** native 900p, SMAA, 8x anisotropic filtering, 512-size
  shadows and the same balanced scene/CPU settings. Can be sharper than
  FSR1 output in fine detail but is not guaranteed to be faster.
- Earlier Sharp/Balanced, Visual+ and Ultra benchmark are still available.
- **GPU texture cache:** recent image grace 240→300 frames; cached texture
  retention 600→720 frames. GPU-produced resources are never treated as
  reconstructible CPU textures. Existing Vulkan budget pressure detection,
  LRU handling and allocation retry are unchanged.
- **Reusable geometry buffer cache:** retention 600→720 frames, cache cap
  256→320 MiB. Cached buffers remain reclaimable when over budget.
- **CPU Boost Extreme:** optional 16ms ADPF hint from the CPU research branch
  is present, but High Smooth chooses Maximum/20ms to avoid unnecessary heat.

These changes can increase resident memory in scenes that reuse assets; they
do not allocate a fixed 1GB block or guarantee more FPS. If Vulkan does not
expose a usable heap budget, the driver can still fail allocations, so this
experiment deliberately increases cache capacity only **moderately**.

## Installing safely

New build is `0.5.9.3-mali-alpha-1.5-g925-perf2`, versionCode **21**,
same Android package `com.libertyrecomp` and repository debug signing
configuration. As always, **back up** the app's data, GTA IV saves and game
assets before installing. Update over the existing package **only if**
Android accepts the signing key and the APK is a completed GitHub Actions
artifact. Never uninstall a working build as a first fix for signature errors.
You can revert launcher changes with **RESET TO DEFAULT** and **Save changes**
without erasing the game data.

After installing, on the Graphics tab press **G925 HIGH SMOOTH - RECOMMENDED**,
**Save changes**, then **Play**. The app intentionally does not overwrite
the user's existing High settings silently. Use **System Vulkan** driver;
the tested PanVK drivers produced black/grey scenes.

## Reproducible performance check

Test a repeated drive through the same part of Liberty City:

1. Disable game recording and unnecessary overlays.
2. On the launcher enable **FPS counter**, **Frame time (ms)** and optionally
   **Memory** in the Performance overlay; Save changes.
3. Compare the older original High profile, High Smooth, Native Detail, and
   optionally a 30 FPS cap as a stable control.
4. Use matched starting location, time of day, speed and temperature.
   Record repeated worst stalls, 1% lows and stable FPS rather than peaks.
5. If the game becomes permanently unresponsive, use the launcher's
   **Play with Debug Log** and share crash reports/last log lines if available.
   A freeze with no crash report needs a different diagnosis from low FPS.

## Validation

```bash
python tools/test_g925_graphics_lab.py
python tools/test_g925_cpu_hint.py
python tools/test_g925_cache_policy.py
```

Full Android native + Java compilation must also succeed. GitHub Actions
build artifacts alone are **not** validation of device runtime stability.

## Rollback

The previous successful APK is stored under GitHub Actions run
`41363883009` (Graphics Lab v1). Do not assume a downgrade over version
code 21 will install: Android normally blocks version downgrades. Retain a
verified backup and the v1 artifact; restore through a compatible and
non-destructive update path when possible.

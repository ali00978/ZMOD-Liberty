# G925 Graphics Lab — POCO X8 Pro Max (experimental)

This branch adds opt-in, user-selectable graphics presets to the **existing Mali Edition Android launcher**.
It is **not** an Android GPU driver replacement, game asset pack, or a proven 60-FPS fix.
The source project is GPL-3.0; this branch includes no GTA IV game files.

## Device and current baseline

- POCO X8 Pro Max / Dimensity 9500s / Immortalis-G925 MC11
- System Mali Vulkan 49.1.0, Vulkan 1.3.278
- Previously observed 25–45 FPS, including fluctuations while stationary
- Mesa PanVK experimental drivers caused black/grey scenes: use **System**
- GPU load is reported as N/A; total CPU load does not isolate per-core bottlenecks

## Presets

| Button | Render mode | Visual settings | Expected tradeoff |
|---|---|---|---|
| G925 SHARP / BALANCED | Native 900p; SMAA | Console shadows, original reflections, 40 FPS cap | More detail than 720p, potentially lower FPS |
| G925 VISUAL+ | 1080p output using FSR1 Ultra Quality; SMAA | 512 shadow map, original reflections, 40 FPS cap | Sharper shadows with somewhat higher GPU cost |
| G925 ULTRA — BENCHMARK ONLY | Native 1080p, SMAA | 512 shadows, 1080p reflections, 120% draw distance, 70% population, 60 FPS cap | Significantly heavier; **not intended as guaranteed 60 FPS** |

All presets are *opt-in*. They update only graphics selectors. Click **Save changes** to apply,
then **Play**. The original settings remain available via **RESET TO DEFAULT**.

Additional menu controls expose: reflection resolution, reflection capture distance, dynamic
draw-distance setting and the below-30-FPS reflection guard.

Note: If the game has been configured with custom graphics overrides, choosing a preset
only changes the options in that preset. Other unrelated options retain their selected values.

## Source and validation

Launcher implementation: `os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java`

Run the static test from repository root:

```sh
python tools/test_g925_graphics_lab.py
```

This validates the three buttons, mapped menu choices, and isolation from GPU driver configuration.
**This does not test actual runtime performance or Android compatibility.**

## APK compilation

The existing repository workflow `.github/workflows/build-android-apk.yml` compiles native
code with Windows + Android NDK r29 and packages a debug-signed APK artifact.

For this development branch, the APK workflow is configured to trigger on pushes to
`feature/g925-graphics-lab`, and to use Mali system Vulkan plus ARMv9 CPU target by default.
Check **Actions → Android APK → latest run → Artifacts**.

A build **must show Success** before treating the artifact as an installable APK.
GitHub Actions on a new fork might first need to be enabled under the Actions tab.
The first native build compiles a large generated codebase and may fail or need adjustments.

## Installation and rollback — important

Before any APK installation, **back up GTA IV files and saved games**, including files under
`Android/data/com.libertyrecomp/files/`, using ADB or another accessible method.
Do not uninstall the current working app unless the backup is verified.

This branch retains the existing application ID `com.libertyrecomp`. The source uses the
Mali debug keystore from the repository, but Android will reject an update if signatures do
not match. Updates and downgrades may also require compatible version codes.
If the installation conflicts, do **not** blindly uninstall and erase user data.
The APK contains no Rockstar game data, and users supply their own legally obtained files.

## Test protocol

1. Warm up on the current working APK and record 60 seconds of FPS/frametime in one scene.
2. Back up saves and settings; install the new APK **only if build passes**.
3. Test System Vulkan only, first with the original settings and then Sharp, Visual+.
4. Compare worst frame time, FPS lows, temperature and loading times at the same spot.
5. Test Ultra only briefly. Stop if temperature or throttling becomes excessive.
6. Report crashes, grey scenes, incorrect reflections, or significantly worse performance.

**No changes are made to the Xiaomi system driver or to the GTA IV game assets.**

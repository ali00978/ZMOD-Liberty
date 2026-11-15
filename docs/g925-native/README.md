# ZMOD Liberty G925 Native — standalone test build v2

This is a **new Android app package**, not an update of the installed
`com.libertyrecomp` application. Build branch:
`feature/g925-native-standalone-app-v1`.

| | Classic edition | G925 Native v2 |
|---|---|---|
| Package ID | `com.libertyrecomp` | `com.zmodliberty.g925native` |
| Launcher | Mali Edition | G925 Native |
| Vulkan driver | System Vulkan on Mali | System Vulkan on Mali |
| CPU/ABI | Mali ARMv9 | Mali ARMv9 |
| Performance hint | default feedback cadence 25 ms | default feedback cadence 16 ms |
| Game/rendering engine | existing verified recompilation + renderer | **same verified core initially** |
| Game data / saves | classic sandbox | **separate sandbox** |

## First install

Download `ZMOD-Liberty-G925-Native-v1-armv9.apk` from the artifacts of a
successful [Android APK workflow](../../.github/workflows/build-android-apk.yml).
Install it **beside** the previous app. The separate Android package needs
the legitimate GTA IV game files installed again through its own installer.
The existing app, files, settings and savegames are left alone.

G925 Native v2 is the next stage of a dedicated G925 renderer. The build retains the tested native Vulkan engine and its exact rendering
semantics, adds the isolated app identity and CPU scheduling profile, and now
uses a fixed-capacity queue for up to 128 texture uploads. This removes
per-frame vector growth while preserving both image layout barriers and every
Vulkan copy in order. **It is not yet a rewritten Vulkan
backend and does not guarantee an FPS improvement.** We must first validate
that a clean isolated installation can boot the game correctly.

For a valid A/B comparison, use the **same scene**, graphics, resolution,
FPS cap, temperature and saved game in both apps. Record:
- title FPS and frame p95;
- native CPU command recording time;
- GPU frame time;
- merged indexed draw count;
- crashes / stuttering / setup errors.

On the target POCO / Dimensity 9500s, the standalone G925 profile asks Android
ADPF for 16 ms target and 16 ms feedback, without forcing GPU clocks or
reducing visual quality. Explicit user settings override these defaults.

## Build commands

After the repo's native build has staged the Mali Armv9 libraries:

```sh
LIBERTY_GPU=mali bash os/android/scripts/build_apk.sh debug
LIBERTY_GPU=mali LIBERTY_EDITION=g925native bash os/android/scripts/build_apk.sh debug
```

Both commands produce `os/android/app/build/outputs/apk/debug/app-debug.apk`,
so copy the first APK before running the second. GitHub Actions automates
this and publishes **two different APKs**.

### Safety and compatibility

- No root, overclocking or unsupported GPU driver modification.
- Native engine reuse in fast CI is permitted only after a successful pinned
  source SHA, artifact, branch ancestry and exact changed-file allowlist
  verification. Changes to any GPU/backend source require a full build.
- Do not copy private game data between Android app sandboxes automatically.
- The game needs legally obtained GTA IV resources as before.

## Next development step

Keep this app identity and sandbox, then prototype a new backend behind a
compile/runtime switch and compare measured CPU/GPU work against the same
recompiled game workload. Keep exact pixel output and stable game behavior
before optimizing shaders and Vulkan render passes.

### G925 Native v2 test checklist

1. Under the successful workflow's **Artifacts**, obtain
   `ZMOD-Liberty-G925-Native-v2-armv9.apk` (not the Classic APK).
2. Install alongside classic; load your own original GTA IV data in this
   app's **separate** data folder. Do not delete your old app.
3. Keep graphics/resolution the same as in the other app. Confirm the menu,
   game rendering, camera, and controls all work before judging FPS.
4. If it does not render or crashes, return `last_launch.txt` and the
   automatically logged error; do not repeatedly change drivers or graphics.

The 2.37 native upgrade removes per-frame heap growth from the bounded
texture upload batch. Compilation and host byte/order/barrier tests cannot
prove game playability or measure device-side FPS; those remain real phone
acceptance gates.

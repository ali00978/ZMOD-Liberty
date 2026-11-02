# G925 Official Clean Rebuild — original renderer, measured optimization

This branch starts from the original published Mali-compatible Android
fork (ali00978/ZMOD-Liberty at mali-dimensity-8400), sharing the native
renderer, texture buffer policies, frame scheduling and CVar defaults
with the pre-experiment G925 Graphics Lab v1 build.

For a fast and safe APK, native code and assets are exactly those from
the verified workflow artifact 41363883009 (commit
99d29c7ac0eb0d7d46964396153b5d8e10460046). The fail-closed
tool validate_g925_fast_apk.py checks that no incompatible native
sources changed. This is the Mali-compatible build of the original
LibertyRecompAndroid port. The standard vaduur Adreno package must not
be substituted blindly for the phone's System Vulkan driver.

## Real reason a "stock" upgrade can still stutter

Older versions may have left arguments in args.txt, graphics.txt,
overlay.txt, native.toml, env.txt, and the "performance_test" app
preferences. A Java-only engine rollback does NOT clear those.
This version detects legacy G925 experiment marker files and
performs a one-time backed-up reset. The user can also explicitly
choose "CLEAN ORIGINAL SETTINGS + 60 FPS" at any time.

The reset preserves games, saves, imported content, touch choices,
the chosen driver, and shader caches; it backs up modified config
files under Android/data/com.libertyrecomp/files/g925-official-backups/.
The clean reset reimports the original APK default_args.txt rather
than blindly adding another set of overrides. The 60 FPS cap and
zero run-ahead do not force higher graphics. The thermal guard and
reflection guard remain the original Mali-port defaults.

To compare native output under equal graphics, select:
- Original graphics / 60 FPS cap (performance goal)
- Original graphics / steady 40 FPS (frame-pacing baseline)

Neither setting guarantees FPS or remedies Vulkan-driver stalls.
A high average FPS can hide severe frame-time spikes. Capture FPS
and frame time with overlays, keep the phone temperature stable,
and compare the same street scene and direction in each mode.
For native modifications, create a separate tested branch and
require a full compilation instead of repackaging old .so files.

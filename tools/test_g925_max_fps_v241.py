#!/usr/bin/env python3
"""G925 v2.42: protected full-quality performance profile, no visual downgrades."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
launcher = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text(encoding="utf-8")
gradle = (root / "os/android/app/build.gradle.kts").read_text(encoding="utf-8")
workflow = (root / ".github/workflows/build-android-apk.yml").read_text(encoding="utf-8")

start = launcher.index("private void applyG925FullQualityPerformanceProfile()")
end = launcher.index("// Graphics settings: one dropdown", start)
profile = launcher[start:end]
selected = dict(re.findall(r'\{"([^"]+)", "([^"]+)"\}', profile))
required = {
    "gta4_frame_limit": "60",
    "gta4_extra_frames_in_flight": "1",
    "android_performance_hint_target_ms": "16",
    "android_performance_hint_report_interval_ms": "16",
    "android_performance_hint_stable_threads": "true",
    "android_performance_hint": "true",
}
assert selected == required, (selected, required)

# Refuse all render-quality knobs, including dynamic guards that lower
# reflection update frequency or draw distance behind the user's back.
for prohibited in (
    "gta4_output_height_cap", "gta4_native_upscaler",
    "gta4_native_anti_aliasing", "gta4_draw_distance_scale",
    "gta4_traffic_density_scale", "gta4_ped_density_scale",
    "gta4_parked_car_density_scale", "gta4_scenario_ped_density_scale",
    "gta4_shadow_map_base_size", "gta4_reflection_aa",
    "gta4_fps_guard", "gta4_dynamic_draw_distance",
    "android_thermal_guard", "gta4_fsr1_quality",
):
    assert prohibited not in profile, prohibited

assert 'button("RESTORE PREVIOUS GRAPHICS BACKUP", false)' in launcher
assert "restoreG925GraphicsBackup()" in launcher
restore = launcher[launcher.index("private void restoreG925GraphicsBackup()"):
                   launcher.index("// Graphics settings: one dropdown")]
assert '"graphics-before-g925-40fps.txt"' in restore
assert '"graphics-before-g925-maxfps.txt"' in restore
assert '"graphics-before-quality-restore.txt"' in restore
assert "REPLACE_EXISTING" in restore
assert "mGraphicsPositions[index] = selection;" in restore
assert "applyG925MaxFpsProfile()" not in launcher
assert "applyG925Steady40Profile()" not in launcher
assert "MAX FPS (LOW GRAPHICS)" not in launcher
assert "G925 NATIVE / 40 FPS PERFORMANCE" not in launcher
assert 'button("G925 NATIVE / FPS BOOST (SAME GRAPHICS)", true)' in launcher
assert "fullQuality.setOnClickListener(v -> applyG925FullQualityPerformanceProfile())" in launcher
assert 'new File(mDataRoot, "graphics-before-g925-full-quality.txt")' in profile
assert profile.index("Files.copy(graphics.toPath()") < profile.index("mGraphicsSpinners[index].setSelection(choice")
assert profile.index("int[] settingIndices") < profile.index("mGraphicsSpinners[index].setSelection(choice")
assert "G925FullQualityCvarMerge.merge(original, selected)" in profile
assert "writeCleanConfig(graphics, merged.getBytes(StandardCharsets.UTF_8))" in profile
assert "if (mSettingsDirty)" in profile
assert "saveSettings(false);" not in profile
assert "writeGraphicsSettings();" not in profile

# Every setting is guaranteed to exist in the launcher and have the expected
# exact backing cvar value, so no half-application or accidental downgrade.
graphics = launcher[launcher.index("private static final GraphicsSetting[] GRAPHICS_SETTINGS"):
                    launcher.index("private final Selector[] mGraphicsSpinners")]
for key, value in required.items():
    matches = []
    for entry in graphics.split("new GraphicsSetting(")[1:]:
        arrays = re.findall(r'new String\[\]\s*\{([^}]*)\}', entry.split("),", 1)[0])
        if len(arrays) == 3 and f'"{key}"' in arrays[0]:
            matches.append(arrays[-1])
    assert len(matches) == 1 and f'"{value}"' in matches[0], (key, value, matches)

assert "versionCode   = 70" in gradle
assert "-alpha-2.42-g925-fullquality" in gradle
assert 'ZMOD-Liberty-G925-Native-v3.3-armv9.apk' in workflow
assert "python tools/test_g925_max_fps_v241.py" in workflow

# Compile a real Java regression for duplicate CPU options and preservation of
# every known, unknown and user-custom visual cvar across repeated upgrades.
javac = os.environ.get("G925_TEST_JAVAC") or shutil.which("javac")
java = shutil.which("java")
if not javac or not java:
    raise SystemExit("Java 17 JDK required for lossless G925 quality preservation")
with tempfile.TemporaryDirectory(prefix="g925-full-quality-") as folder:
    subprocess.run([javac, "-encoding", "UTF-8", "-d", folder,
        str(root / "os/android/app/src/main/java/com/libertyrecomp/G925FullQualityCvarMerge.java"),
        str(root / "tools/fixtures/g925/G925FullQualityCvarMergeTest.java")], check=True)
    subprocess.run([java, "-cp", folder, "com.libertyrecomp.G925FullQualityCvarMergeTest"], check=True)

print("PASS: G925 fps-scheduling only; no 540p/density/draw-distance/shadow/reflection changes")

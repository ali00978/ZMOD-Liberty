#!/usr/bin/env python3
"""G925 v2.41: Max FPS lowers both CPU scene and GPU pixel workloads."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
launcher = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text(encoding="utf-8")
gradle = (root / "os/android/app/build.gradle.kts").read_text(encoding="utf-8")
workflow = (root / ".github/workflows/build-android-apk.yml").read_text(encoding="utf-8")

start = launcher.index("private void applyG925MaxFpsProfile()")
end = launcher.index("/** Opt-in 40 FPS baseline", start)
profile = launcher[start:end]
choices = dict(re.findall(r'\{"([^"]+)", "([^"]+)"\}', profile))

required = {
    "gta4_output_height_cap": "540",
    "gta4_native_upscaler": "native",
    "gta4_native_anti_aliasing": "off",
    "gta4_frame_limit": "60",
    "gta4_draw_distance_scale": "0.6",
    "gta4_traffic_density_scale": "0.2",
    "gta4_shadow_map_base_size": "256",
    "gta4_reflection_aa": "off",
    "gta4_extra_frames_in_flight": "1",
    "android_performance_hint_target_ms": "16",
    "android_performance_hint_report_interval_ms": "16",
    "android_performance_hint_stable_threads": "true",
    "android_performance_hint": "true",
    "gta4_dynamic_draw_distance": "true",
    "gta4_fps_guard": "2",
    "android_thermal_guard": "true",
}
assert choices == required, (choices, required)
graphics = launcher[launcher.index("private static final GraphicsSetting[] GRAPHICS_SETTINGS"):
                    launcher.index("private final Selector[] mGraphicsSpinners")]
for key, value in required.items():
    matches = []
    for entry in graphics.split("new GraphicsSetting(")[1:]:
        arrays = re.findall(r'new String\[\]\s*\{([^}]*)\}', entry.split("),", 1)[0])
        if len(arrays) == 3 and f'"{key}"' in arrays[0]:
            matches.append(arrays[-1])
    assert len(matches) == 1 and f'"{value}"' in matches[0], (key, value, matches)

assert "if (!BuildConfig.G925_NATIVE_EDITION) return;" in profile
assert 'new File(mDataRoot, "graphics-before-g925-maxfps.txt")' in profile
assert profile.index("int[] settingIndices") < profile.index("mGraphicsSpinners[index].setSelection(choice")
assert profile.index("Files.copy(existing.toPath()") < profile.index("mGraphicsSpinners[index].setSelection(choice")
assert "saveSettings(false);" in profile
assert 'button("G925 NATIVE / MAX FPS (LOW GRAPHICS)", true)' in launcher
assert "maxFps.setOnClickListener(v -> applyG925MaxFpsProfile());" in launcher
assert "applyG925Steady40Profile()" in launcher
assert "versionCode   = 69" in gradle
assert "-alpha-2.41-g925-maxfps" in gradle
assert "python tools/test_g925_max_fps_v241.py" in workflow
print("PASS: G925 2.41 Max FPS valid cvars, 540p/20% scene, 60 cap, backup, independent G925 package")

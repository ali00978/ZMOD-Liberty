#!/usr/bin/env python3
"""Verify standard-graphics/60-FPS cap and safe migration. Not a device benchmark."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
java = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
activity = (root / "os/android/app/src/main/java/com/libertyrecomp/LibertyActivity.java").read_text()
gradle = (root / "os/android/app/build.gradle.kts").read_text()
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()

a = java.index("private static final String[][] G925_STANDARD_60_PROFILE")
b = java.index("    };", a)
profile = dict(re.findall(r'\{"([^"]+)", "([^"]+)"\}', java[a:b]))
expected = {
    "gta4_output_height_cap": "720",
    "gta4_native_upscaler": "native",
    "gta4_native_anti_aliasing": "fxaa",
    "gta4_frame_limit": "60",
    "gta4_draw_distance_scale": "1.0",
    "gta4_traffic_density_scale": "0.4",
    "gta4_shadow_map_base_size": "256",
    "gta4_reflection_resolution": "original",
    "gta4_reflection_capture_distance": "near",
    "gta4_dynamic_draw_distance": "true",
    "gta4_fps_guard": "2",
    "gta4_extra_frames_in_flight": "0",
    "android_performance_hint_target_ms": "20",
    "android_thermal_guard": "true",
}
assert all(profile.get(k) == v for k, v in expected.items()), profile
settings = java[java.index("private static final GraphicsSetting[] GRAPHICS_SETTINGS"):a]
pattern = r'new GraphicsSetting\("([^"]+)",[\s\S]*?new String\[\] \{([^}]+)\},\s*new String\[\] \{([^}]+)\},\s*new String\[\] \{([^}]+)\}\)'
mapped = {}
for _, keys, labels, values in re.findall(pattern, settings):
    for key in re.findall(r'"([^"]+)"', keys):
        mapped[key] = set(re.findall(r'"([^"]+)"', values))
for key, val in profile.items():
    assert key in mapped and val in mapped[key], (key, val)

oncreate = java[java.index("protected void onCreate("):java.index("protected void onResume()")]
assert "initializeG925Standard60Defaults();" in oncreate
assert "initializeG925NativeMaxDefaults();" not in oncreate
init = java[java.index("private void initializeG925Standard60Defaults()"):
            java.index("private void restoreG925PreStandard60Graphics()")]
assert "if (!DriverBridge.isMaliBuild()) return;" in init
assert "if (marker.isFile()) return;" in init
assert init.index("Files.copy(graphics.toPath(), backup.toPath())") < init.index("Files.move(staged.toPath(), graphics.toPath()")
for key in ("gta4_parked_car_density_scale","gta4_ped_density_scale","gta4_scenario_ped_density_scale"):
    assert key in init
assert 'Button g925Standard60 = button("G925 STANDARD / 60 FPS"' in java
reset = java[java.index('Button reset = button("RESET TO DEFAULT"'):java.index('return card;',java.index('Button reset = button("RESET TO DEFAULT"'))]
assert 'applyG925Preset(G925_STANDARD_60_PROFILE, "G925 Standard / 60 FPS")' in reset
assert "RESTORE PRE-STANDARD GRAPHICS" in java
assert "G925 NATIVE MAX / ALL HIGH" in java
assert "GRAPHICS_FILE" in activity and "withoutDuplicateOptions(arguments)" in activity
assert "feature/g925-standard-60-fps" in workflow
assert "versionCode   = 24" in gradle
print("PASS: normal 720p visuals, 60-FPS cap, no run-ahead, backup/restore and mapped settings")

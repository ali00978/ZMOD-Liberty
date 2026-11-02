#!/usr/bin/env python3
"""Validate one-time G925 native maximum-resolution defaults and rollback safety.

These are static source invariants. Actual Android runtime behavior and sustained
frame times must be checked on the device after a successful APK compilation.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
java = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text(encoding="utf-8")
activity = (root / "os/android/app/src/main/java/com/libertyrecomp/LibertyActivity.java").read_text(encoding="utf-8")
gradle = (root / "os/android/app/build.gradle.kts").read_text(encoding="utf-8")
workflow = (root / ".github/workflows/build-android-apk.yml").read_text(encoding="utf-8")

start = java.index("private static final String[][] G925_NATIVE_MAX_PROFILE")
end = java.index("    };", start) + 6
values = dict(re.findall(r'\{"([^"]+)", "([^"]+)"\}', java[start:end]))
required = {
    "gta4_output_height_cap": "0",  # full native screen resolution
    "gta4_native_upscaler": "native",
    "gta4_native_anti_aliasing": "smaa",
    "gta4_anisotropic_filtering": "16x",
    "gta4_frame_limit": "60",
    "gta4_draw_distance_scale": "1.2",
    "gta4_traffic_density_scale": "1.0",
    "gta4_shadow_map_base_size": "512",
    "gta4_reflection_resolution": "full",
    "gta4_reflection_capture_distance": "far",
    "gta4_reflection_aa": "original",
    "gta4_dynamic_draw_distance": "false",
    "gta4_fps_guard": "0",
    "android_thermal_guard": "false",
}
assert all(values.get(k) == v for k, v in required.items()), values

# All settings must map to documented selector values.
settings = java[java.index("private static final GraphicsSetting[] GRAPHICS_SETTINGS"):java.index("private static final String G925_MAX_DEFAULT_MARKER")]
pattern = r'new GraphicsSetting\("([^"]+)",[\s\S]*?new String\[\] \{([^}]+)\},\s*new String\[\] \{([^}]+)\},\s*new String\[\] \{([^}]+)\}\)'
menu = {}
for _, keys, labels, item_values in re.findall(pattern, settings):
    for key in re.findall(r'"([^"]+)"', keys):
        menu[key] = re.findall(r'"([^"]+)"', item_values)
for key, value in values.items():
    assert key in menu, f"Missing graphics selector: {key}"
    assert value in menu[key], f"Invalid selection: {key}={value}"

# One-time rollout, no changes on subsequent launches, backup before overwrite.
setup = java[java.index("private void initializeG925NativeMaxDefaults()"):java.index("private void restoreG925PreviousGraphics()")]
assert "if (!DriverBridge.isMaliBuild()) return;" in setup
assert "if (marker.isFile()) return;" in setup
assert setup.index("java.nio.file.Files.copy(graphics.toPath(), previous.toPath())") < setup.index("java.nio.file.Files.move(staged.toPath(), graphics.toPath()")
assert "G925_MAX_PREVIOUS_GRAPHICS" in setup
assert "gta4_parked_car_density_scale" in setup
assert "gta4_ped_density_scale" in setup
assert "gta4_scenario_ped_density_scale" in setup
assert "StandardCopyOption.REPLACE_EXISTING" in setup
assert "initializeG925Standard60Defaults();" in java
assert "initializeG925NativeMaxDefaults();" not in java
assert java.index("initializeG925Standard60Defaults();") < java.index("setContentView(buildLayout());")

# Previous settings can be restored by the user without an uninstall.
assert "RESTORE PREVIOUS GRAPHICS" in java
assert "private void restoreG925PreviousGraphics()" in java
assert "G925 NATIVE MAX / ALL HIGH" in java
assert "G925_NATIVE_MAX_PROFILE, \"G925 Native Max\"" in java
assert "GRAPHICS_FILE" in activity
assert "withoutDuplicateOptions(arguments)" in activity

# Experimental native-max APK is versioned above previous lab and perf v2.
assert "versionCode   = 24" in gradle
assert '-alpha-1.5-g925-standard60' in gradle
assert "feature/g925-native-max-v3" in workflow

print(f"PASS: G925 native screen-resolution defaults, {len(values)} mapped controls, backup and rollback")

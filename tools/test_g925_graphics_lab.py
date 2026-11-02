#!/usr/bin/env python3
"""Static validation of G925 v2 graphics profiles and launcher mappings.

This does not establish FPS, freedom from stutter, or runtime stability.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
launcher = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text(encoding="utf-8")
runtime = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/LibertyActivity.java").read_text(encoding="utf-8")

assert launcher.count("G925 GRAPHICS LAB v1") == 1  # v1 option definitions retained
assert launcher.count("G925 GRAPHICS LAB v2") == 1
assert launcher.count("applyG925Preset(new String[][]") == 6
assert launcher.count("private void applyG925Preset(") == 1
assert launcher.count("Button g925Preset") == 3
assert "G925 HIGH SMOOTH - RECOMMENDED" in launcher
assert "G925 NATIVE DETAIL - SHARPER" in launcher
assert 'mGraphicsSpinners[i].setSelection(choice + 1, false)' in launcher
assert "markSettingsChanged();" in launcher

start = launcher.index("private static final GraphicsSetting[] GRAPHICS_SETTINGS")
end = launcher.index("private final Selector[] mGraphicsSpinners", start)
settings = launcher[start:end]
start = launcher.index('card.addView(heading("IMMORTALIS G925 PERFORMANCE V2"')
end = launcher.index('Button reset = button("RESET TO DEFAULT"', start)
ui = launcher[start:end]

# Map every persisted CVar to the exact choices that its selector may save.
choices_by_cvar = {}
pattern = (
    r'new GraphicsSetting\("([^"]+)",[\s\S]*?'
    r'new String\[\] \{([^}]+)\},\s*'
    r'new String\[\] \{([^}]+)\},\s*'
    r'new String\[\] \{([^}]+)\}\)'
)
for _, keys, display, values in re.findall(pattern, settings):
    keys = re.findall(r'"([^"]+)"', keys)
    display = re.findall(r'"([^"]+)"', display)
    values = re.findall(r'"([^"]+)"', values)
    assert len(display) == len(values) + 1, (keys, display, values)
    for key in keys:
        assert key not in choices_by_cvar, key
        choices_by_cvar[key] = set(values)

pairs = re.findall(r'\{"((?:gta4|android)_[a-z0-9_]+)", "([^"]+)"\}', ui)
assert len(pairs) >= 70, f"Only {len(pairs)} preset assignments"
for key, value in pairs:
    assert key in choices_by_cvar, f"Unknown setting {key}"
    assert value in choices_by_cvar[key], f"Invalid setting value {key}={value}"

assert '"gta4_frame_limit", "40"' in ui
assert '"gta4_shadow_map_base_size", "512"' in ui
assert '"gta4_native_upscaler", "fsr1"' in ui
assert '"gta4_fsr1_quality", "quality"' in ui
assert '"gta4_native_upscaler", "native"' in ui
assert '"gta4_anisotropic_filtering", "8x"' in ui
assert '"android_performance_hint_target_ms", "20"' in ui
assert '"android_thermal_guard", "true"' in ui

# User must explicitly opt in; they can save or reset settings.
assert "Save changes" in launcher
assert "RESET TO DEFAULT" in launcher
assert "GRAPHICS_FILE" in runtime and "withoutDuplicateOptions(arguments)" in runtime
assert all(x not in ui for x in ("DriverBridge", "driver.txt", "AndroidManifest"))

print(f"PASS: G925 v2 five presets, {len(pairs)} selector mappings, driver isolation")

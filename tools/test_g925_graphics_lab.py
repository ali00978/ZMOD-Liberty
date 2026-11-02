#!/usr/bin/env python3
"""Static checks for the optional G925 Graphics Lab launcher presets.

Run with python tools/test_g925_graphics_lab.py. Native rendering and device
performance must still be tested on a physical Immortalis-G925 phone.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java"
code = SOURCE.read_text(encoding="utf-8")

assert code.count('G925 GRAPHICS LAB v1') == 2, "Expected one controls block and one UI block"
assert code.count('Button g925Preset') == 3, "Expected exactly 3 G925 profile buttons"
# Three Graphics Lab presets plus one shared Stock Stable 30/60 FPS helper.
# Only count the original lab section for the lab's own three profiles.
lab_start = code.index('card.addView(heading("IMMORTALIS G925 GRAPHICS LAB"')
lab_end = code.index('Button reset = button("RESET TO DEFAULT"', lab_start)
assert code[lab_start:lab_end].count('applyG925Preset(new String[][]') == 3
assert code.count('applyG925Preset(new String[][]') == 4
assert 'applyG925StockPreset(String cap)' in code
assert code.count('private void applyG925Preset(') == 1
assert 'markSettingsChanged();' in code
assert 'mGraphicsSpinners[i].setSelection(choice + 1, false)' in code
assert 'mGraphicsPositions[i] = choice + 1' in code

settings_start = code.index("private static final GraphicsSetting[] GRAPHICS_SETTINGS")
settings_end = code.index("private final Selector[] mGraphicsSpinners")
settings = code[settings_start:settings_end]
ui_start = code.index('card.addView(heading("IMMORTALIS G925 GRAPHICS LAB"')
ui_end = code.index('Button reset = button("RESET TO DEFAULT"', ui_start)
ui = code[ui_start:ui_end]

for label in ("Reflection resolution", "Reflection distance",
              "Dynamic draw distance", "Reflection FPS guard"):
    assert 'new GraphicsSetting("' + label + '"' in settings, label

# Every preset must reference only existing control keys and valid menu values.
pairs = re.findall(r'\{"(gta4_[a-z0-9_]+)", "([^"]+)"\}', ui)
assert len(pairs) >= 34, f"Only {len(pairs)} preset assignments found"
for key, value in pairs:
    assert '"' + key + '"' in settings, f"Unknown setting: {key}"
    assert '"' + value + '"' in settings, f"Unknown value: {key}={value}"

assert '"gta4_output_height_cap", "900"' in ui
assert '"gta4_native_upscaler", "fsr1"' in ui
assert '"gta4_fsr1_quality", "ultra_quality"' in ui
assert '"gta4_output_height_cap", "1080"' in ui
assert 'g925Preset2 = button("G925 ULTRA - BENCHMARK ONLY"' in ui
assert "g925Preset0" in ui and "g925Preset1" in ui

# Never select or mutate a GPU driver from graphics preset code.
assert "DriverBridge" not in ui
assert "driver.txt" not in ui
assert "AndroidManifest" not in ui

print("PASS: G925 graphics controls, 3 presets and settings mappings")

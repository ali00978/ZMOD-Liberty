#!/usr/bin/env python3
"""G925 v4: two opt-in experiments, no quality change to the initial default.

This is source validation; device framerate and Vulkan stability require
the actual POCO hardware.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
src = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
gradle = (root / "os/android/app/build.gradle.kts").read_text()

assert "G925 MAX NATIVE / STEADY 30 TEST" in src
assert "G925 NATIVE HIGH / STUTTER TEST" in src
assert "G925 NATIVE MAX / ALL HIGH" in src
assert "initializeG925Standard60Defaults();" in src
assert "RESTORE PREVIOUS GRAPHICS" in src
assert 'if (paced[i][0].equals("gta4_frame_limit")) paced[i][1] = "30";' in src
assert 'G925_NATIVE_MAX_PROFILE[i].clone();' in src
assert 'G925_NATIVE_MAX_PROFILE, "G925 Native Max"' in src
assert "versionCode   = 24" in gradle
assert "-alpha-1.5-g925-standard60" in gradle

block = src[src.index('Button g925MaxNativeStream = button('):
            src.index('card.addView(g925MaxNativeStream,', src.index('Button g925MaxNativeStream = button('))]
assignments = dict(re.findall(r'\{"([^"]+)", "([^"]+)"\}', block))
expected = {
    "gta4_output_height_cap": "0",
    "gta4_native_upscaler": "native",
    "gta4_native_anti_aliasing": "smaa",
    "gta4_anisotropic_filtering": "16x",
    "gta4_frame_limit": "40",
    "gta4_shadow_map_base_size": "512",
    "gta4_draw_distance_scale": "1.0",
    "gta4_traffic_density_scale": "0.7",
    "gta4_reflection_resolution": "original",
    "gta4_reflection_capture_distance": "near",
    "gta4_reflection_aa": "off",
}
assert all(assignments.get(k) == v for k,v in expected.items()), assignments
assert assignments["gta4_output_height_cap"] == "0"
assert assignments["gta4_native_upscaler"] == "native"
assert "mGraphicsSpinners[i].setSelection" in src
print("PASS: G925 v4 preserves native-max default; 30FPS-only and native High diagnostics validated")

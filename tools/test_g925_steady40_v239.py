#!/usr/bin/env python3
"""Check the real G925 Native 40 FPS launcher preset and bounded CPU queue."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
launcher = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
cache = (ROOT / "glue/rexglue-sdk-main/src/graphics/gta4_native/native_vertex_cache.h").read_text()
gradle = (ROOT / "os/android/app/build.gradle.kts").read_text()
native_defaults = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/G925NativeDefaults.java").read_text()

profile_start = launcher.index("private void applyG925Steady40Profile()")
profile_end = launcher.index("// Graphics settings: one dropdown", profile_start)
profile = launcher[profile_start:profile_end]
graphics_start = launcher.index("private static final GraphicsSetting[] GRAPHICS_SETTINGS")
graphics_end = launcher.index("private final Selector[] mGraphicsSpinners", graphics_start)
graphics = launcher[graphics_start:graphics_end]

assert "if (BuildConfig.G925_NATIVE_EDITION)" in launcher
assert "G925 NATIVE / 40 FPS PERFORMANCE" in launcher
assert 'new File(mDataRoot, "graphics-before-g925-40fps.txt")' in profile
assert "saveSettings(false)" in profile
assert "if (!BuildConfig.G925_NATIVE_EDITION) return;" in profile
assert "if (applied != selected.length)" in profile
assert "markSettingsChanged()" in profile
assert "versionCode   = 67" in gradle
assert "-alpha-2.39-g925-steady40" in gradle
assert "addUnlessConfigured" in native_defaults  # user's explicit CPU choices still take priority

required = {
    "gta4_output_height_cap": "720",
    "gta4_native_upscaler": "native",
    "gta4_native_anti_aliasing": "off",
    "gta4_frame_limit": "40",
    "gta4_draw_distance_scale": "0.8",
    "gta4_traffic_density_scale": "0.3",
    "gta4_shadow_map_base_size": "256",
    "gta4_reflection_aa": "off",
    "gta4_extra_frames_in_flight": "2",
    "android_performance_hint_target_ms": "16",
    "android_performance_hint_report_interval_ms": "16",
    "android_performance_hint_stable_threads": "true",
    "android_performance_hint": "true",
    "gta4_dynamic_draw_distance": "true",
    "gta4_fps_guard": "2",
    "android_thermal_guard": "true",
}
pairs = dict(re.findall(r'\{"([^"]+)", "([^"]+)"\}', profile))
assert pairs == required, (pairs, required)
for key, value in required.items():
    # Only use cvars and option values actually exposed by the launcher.
    matches = re.findall(
        r'new GraphicsSetting\(.*?new String\[\] \{[^}]*"' + re.escape(key) +
        r'"[^}]*\}.*?new String\[\] \{([^}]*)\}\s*\)',
        graphics, flags=re.S)
    assert matches, key
    assert f'"{value}"' in matches[0], (key, value, matches[0])

assert "G925CanQueueVertexOptimization" in cache
assert "queued_bytes_.fetch_add(bytes" in cache
assert "queued_bytes_.fetch_sub(job.source.size()" in cache
assert "if (!G925CanQueueVertexOptimization" in cache
assert "if (!ok) job.range->indices = {};" in cache  # preserve fallback behaviour

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("C++20 compiler required for G925 queue-budget test")
with tempfile.TemporaryDirectory(prefix="g925-steady40-") as temp:
    executable = Path(temp) / ("steady40.exe" if os.name == "nt" else "steady40")
    subprocess.run(
        [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
         str(ROOT / "tools/test_g925_steady40_v239.cpp"), "-o", str(executable)],
        check=True,
    )
    subprocess.run([str(executable)], check=True)

print("PASS: native-only 40 FPS launcher preset, 16 supported cvars, backup, bounded optimization queue")

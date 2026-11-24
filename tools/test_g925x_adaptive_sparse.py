#!/usr/bin/env python3
"""G925 X opt-in C++ recorder policy + Android launch defaults, no real-phone FPS claims."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main/src/graphics/gta4_native"
cpp = (SDK / "graphics_system.cpp").read_text()
h = (SDK / "graphics_system.h").read_text()
activity = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/LibertyActivity.java").read_text()
report = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/G925NativeProfileReport.java").read_text()
defaults = ROOT / "os/android/app/src/main/java/com/libertyrecomp/G925XNativeDefaults.java"

for needle in (
    "REXCVAR_DEFINE_BOOL(gta4_native_g925x_adaptive_sparse, false",
    "g925x_sparse_policy_.BeginPass()",
    "g925x_sparse_policy_.EndPass(g925x_probe_sparse, g925x_sparse_draws_saved != 0)",
    "sparse_allowed = g925x_probe_sparse && REXCVAR_GET(gta4_native_merge_sparse_indexed_draws)",
    "g925x_sparse_draws_saved += uint32_t(sparse_count - 1)",
):
    assert needle in cpp, needle
assert "NativeG925XAdaptiveSparse g925x_sparse_policy_" in h
assert "G925XNativeDefaults.apply(arguments, BuildConfig.G925_X_EDITION)" in activity
assert '"gta4_native_g925x_adaptive_sparse"' in report
assert not any(forbidden in defaults.read_text() for forbidden in (
    "gta4_draw_distance_scale", "gta4_fps_guard", "gta4_output_height_cap",
    "gta4_native_shadow_interval", "gta4_native_water_reflection_interval"
)), "Engine lab must never lower graphical quality"

cxx = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
java = shutil.which("java")
javac = os.environ.get("G925_TEST_JAVAC") or shutil.which("javac")
assert cxx and java and javac, "C++20 and Java 17 needed for full off-device tests"
with tempfile.TemporaryDirectory(prefix="g925x-schedule-") as directory:
    exe = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run([cxx, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(ROOT / "tools/test_g925x_adaptive_sparse.cpp"),
                    "-o", str(exe)], check=True, timeout=60)
    subprocess.run([str(exe)], check=True, timeout=60)
    subprocess.run([javac, "-encoding", "UTF-8", "-d", directory,
                    str(defaults),
                    str(ROOT / "tools/fixtures/g925/G925XNativeDefaultsTest.java")],
                   check=True, timeout=60)
    subprocess.run([java, "-cp", directory, "com.libertyrecomp.G925XNativeDefaultsTest"],
                   check=True, timeout=60)
print("PASS: G925 X scheduler logic, disabled-old-editions and quality-preserving launch")

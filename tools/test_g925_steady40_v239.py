#!/usr/bin/env python3
"""Regression: keep G925 worker safety while retiring quality-reducing 2.39 preset."""
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

assert "G925 NATIVE / 40 FPS PERFORMANCE" not in launcher
assert "applyG925Steady40Profile()" not in launcher
assert 'button("G925 NATIVE / FPS BOOST (SAME GRAPHICS)", true)' in launcher
assert "applyG925FullQualityPerformanceProfile()" in launcher
assert "addUnlessConfigured" in native_defaults  # overrides never silently replace explicit user choices
version = re.search(r"versionCode\s*=\s*(\d+)", gradle)
assert version and int(version.group(1)) >= 70
assert re.search(r'-alpha-2\.(?:4[2-9]|[5-9]\d)-g925-', gradle)

for required in (
    "G925CanQueueVertexOptimization(",
    "queued_bytes_.fetch_add(bytes",
    "queued_bytes_.fetch_sub(job.source.size()",
    "if (!G925CanQueueVertexOptimization",
    "if (!ok) job.range->indices = {};",
):
    assert required in cache, required

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("C++20 compiler required for G925 bounded queue regression")
with tempfile.TemporaryDirectory(prefix="g925-budget-") as folder:
    exe = Path(folder) / ("budget.exe" if os.name == "nt" else "budget")
    subprocess.run(
        [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
         str(ROOT / "tools/test_g925_steady40_v239.cpp"), "-o", str(exe)],
        check=True,
    )
    subprocess.run([str(exe)], check=True)

print("PASS: v2.39 low-quality preset retired, lossless G925 native vertex queue preserved")

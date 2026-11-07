#!/usr/bin/env python3
"""Verify real native lifetime integration and bounded housekeeping replay."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
native = SDK / "src/graphics/gta4_native"
source = (native / "graphics_system.cpp").read_text()
header = (native / "graphics_system.h").read_text()
policy = (native / "native_surface_retirement_scan.h").read_text()
schedule = (native / "native_frame_scheduling.h").read_text()
assert '#include "native_surface_retirement_scan.h"' in source
assert "size_t surface_retirement_scan_cursor_ = 0;" in header
assert "NativeSurfaceRetirementScan scan(native_surface_images_.size()," in source
assert "surface_retirement_scan_cursor_ = scan.index();" in source
assert "scan.HasNext(native_surface_images_.size())" in source
assert "scan.Next(native_surface_images_.size(), false);" in source
assert "scan.Next(native_surface_images_.size(), true);" in source
assert "ShouldKeepNativeSurfacePending((*image)->last_used_submission" in source
assert "completed_command_buffer_submission_" in source
assert "pending_surface_release_ids_.erase(release_id);" in source
assert source.count("native_surface_images_.erase(") == 1
assert "kNativeSurfaceRetirementScanItemsPerFrame = 512" in policy
assert "kNativeSurfaceRetirementsPerFrame = 6" in schedule
assert "NativeSurfaceRetirementLimit(force_drain)" in source
assert "ReleasePendingSurfaceImages(true);" in source

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("C++20 host compiler required")
with tempfile.TemporaryDirectory(prefix="g925-surface-scan-") as folder:
    exe = Path(folder) / ("surface_scan.exe" if os.name == "nt" else "surface_scan")
    args = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(SDK / "src"),
            str(ROOT / "tools/test_g925_surface_retirement_v219.cpp"),
            "-o", str(exe)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        args[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(args, check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
print("PASS: native 2.19 bounded surface scan production integration, GPU safety and fairness")

#!/usr/bin/env python3
"""Validate compiled frame-spike policies and native renderer integration."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
source = (SDK / "src/graphics/gta4_native/graphics_system.cpp").read_text()
header = (SDK / "src/graphics/gta4_native/graphics_system.h").read_text()
schedule = (SDK / "src/graphics/gta4_native/native_frame_scheduling.h").read_text()
assert "void ReleasePendingSurfaceImages(bool force_drain = false);" in header
assert "void Gta4NativeGraphicsSystem::ReleasePendingSurfaceImages(bool force_drain)" in source
assert "ShouldKeepNativeSurfacePending((*image)->last_used_submission" in source
assert "retired_surface_count, force_drain)" in source
assert "pending_surface_release_ids_.erase(release_id);" in source
assert "std::unordered_set<uint64_t> still_pending_ids;" not in source
assert "if (retired_surface_count >= NativeSurfaceRetirementLimit(force_drain)) break;" in source
# No other path erases individual native surface images. Final shutdown
# deliberately clears both containers, so the in-place pending set is sound.
assert source.count("native_surface_images_.erase(") == 1
assert source.count("native_surface_images_.clear();") == 1
assert source.count("pending_surface_release_ids_.clear();") == 1
assert source.count("ReleasePendingSurfaceImages(true);") == 2
assert "ReleasePendingSurfaceImages();\n" in source
assert "GetNativeBufferReclamationWorkLimit(cache_bytes, cache_limit)" in source
assert "static thread_local std::vector<Candidate> candidates;" in source
assert "candidates.clear();" in source
assert "candidates.reserve(buffer_resources_.size());" in source
assert "NativeWorkBudget buffer_reclamation(reclamation_limit.max_resources," in source
assert "kNativeSurfaceRetirementsPerFrame = 6" in schedule
assert "NativeBufferReclamationWorkLimit{8, 2ull * 1024 * 1024}" in schedule

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("C++20 host compiler required")
with tempfile.TemporaryDirectory(prefix="g925-stutter-retire-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    args = [
        compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I" + str(SDK / "src"),
        str(ROOT / "tools/test_g925_stutter_retirement_v217.cpp"),
        "-o", str(executable),
    ]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        args[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(args, check=True)
    subprocess.run([str(executable)], check=True, timeout=20)
print("PASS: GPU fence-safe frame-spike amortization and native renderer call sites")

#!/usr/bin/env python3
"""Guard the G925 16ms/60fps-oriented ADPF profile and run C++ unit tests."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
src = (SDK / "gta4-recomp/src/android/android_performance_hint.cpp").read_text()
launcher = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
schedule = (SDK / "src/graphics/gta4_native/native_frame_scheduling.h").read_text()
assert "REXCVAR_DEFINE_UINT32(android_performance_hint_target_ms, 16" in src
assert "G925EffectiveHintTargetMs(requested)" in src
assert "REXCVAR_DEFINE_BOOL(android_performance_hint_stable_threads, true" in src
assert '"Maximum (16ms)"' in launcher
assert 'new String[] {"16", "25", "30", "33"}' in launcher
assert "kNativeSurfaceRetirementsPerFrame = 6;" in schedule
assert "NativeBufferReclamationWorkLimit{8, 2ull * 1024 * 1024}" in schedule
compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("C++20 compiler is required")
with tempfile.TemporaryDirectory(prefix="g925-target-") as folder:
    exe = Path(folder) / ("g925_target.exe" if os.name == "nt" else "g925_target")
    cmd = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
           "-I" + str(SDK / "gta4-recomp/src"),
           str(ROOT / "tools/test_g925_hint_target_v218.cpp"), "-o", str(exe)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        cmd[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(cmd, check=True)
    subprocess.run([str(exe)], check=True, timeout=20)
print("PASS: 60fps-oriented ADPF target, legacy maximum migration and working stutter budgets")

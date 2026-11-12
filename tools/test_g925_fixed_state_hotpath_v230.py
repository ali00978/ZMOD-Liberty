#!/usr/bin/env python3
"""Compile exact state-integrity policy and verify production hotpath integration."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
tool = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not tool: raise SystemExit("C++20 compiler required")
with tempfile.TemporaryDirectory(prefix="g925-hash-v230-") as tmp:
    exe = Path(tmp) / ("hash.exe" if os.name == "nt" else "hash")
    subprocess.run([tool, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(root / "tools/test_g925_fixed_state_hotpath_v230.cpp"),
                    "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
code = (root / "glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.cpp").read_text()
assert code.count("ShouldCaptureRecordedFixedState(") == 2
assert re.search(r"native_command\.captured_fixed_function_state_hash\s*=\s*"
                 r"ShouldCaptureRecordedFixedState\([\s\S]*?"
                 r"\? HashFixedFunctionState\(native_command\.fixed_function_state\)\s*:\s*0;", code)
assert re.search(r"command\.recorded_fixed_function_state_hash\s*=\s*"
                 r"ShouldCaptureRecordedFixedState\([\s\S]*?"
                 r"\? HashFixedFunctionState\(command\.fixed_function_state\)\s*:\s*0;", code)
assert re.search(r"queued_command\.recorded_fixed_function_state_hash\s*&&\s*ShouldVerifyRecordedFixedState\(", code)
assert "BindCommonDrawState(" in code and "vkCmdDrawIndexed(" in code
assert 'gta4_trace_native_renderer, false, "GTA IV/Diagnostics"' in code
gradle = (root / "os/android/app/build.gradle.kts").read_text()
assert re.search(r"versionCode\s*=\s*58", gradle)
assert "-alpha-2.30-g925-hash-hotpath" in gradle
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()
assert "python tools/test_g925_fixed_state_hotpath_v230.py" in workflow
print("PASS: no diagnostic hashes in gameplay; integrity and draw-state checks retained")

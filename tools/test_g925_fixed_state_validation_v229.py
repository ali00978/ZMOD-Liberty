#!/usr/bin/env python3
"""Compile exact native recording integrity policy and verify its live integration."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
tool = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not tool:
    raise SystemExit("C++20 compiler required for native recording correctness test")
with tempfile.TemporaryDirectory(prefix="g925-fixed-recording-") as tmp:
    executable = Path(tmp) / ("fixed.exe" if os.name == "nt" else "fixed")
    subprocess.run([tool, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(root / "tools/test_g925_fixed_state_validation_v229.cpp"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)

renderer = (root / "glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.cpp").read_text()
assert '#include "native_fixed_state_validation_policy.h"' in renderer
assert "ShouldVerifyRecordedFixedState(" in renderer
assert "REXCVAR_GET(gta4_validate_native_hot_caches)" in renderer
assert "REXCVAR_GET(gta4_native_gpu_breadcrumbs)" in renderer
assert re.search(r"ShouldVerifyRecordedFixedState\([\s\S]*?command_index\) &&\s*queued_command.recorded_fixed_function_state_hash !=", renderer)
gradle = (root / "os/android/app/build.gradle.kts").read_text()
assert re.search(r"versionCode\s*=\s*57", gradle)
assert "-alpha-2.29-g925-native-recording" in gradle
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()
assert "python tools/test_g925_fixed_state_validation_v229.py" in workflow
print("PASS: sampled hash check preserves actual render state, full debug validation and v2.29 integration")

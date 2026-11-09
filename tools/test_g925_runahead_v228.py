#!/usr/bin/env python3
"""Build the actual 4-frame wait policy and guard the native+launcher integration."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
tool = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not tool:
    raise SystemExit("C++20 compiler required for runahead correctness test")

with tempfile.TemporaryDirectory(prefix="g925-runahead-") as tmp:
    output = Path(tmp) / ("runahead.exe" if os.name == "nt" else "runahead")
    subprocess.run([tool, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(root / "tools/test_g925_runahead_v228.cpp"),
                    "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)

hook = (root / "glue/rexglue-sdk-main/gta4-recomp/src/android/android_frame_wait_hook.cpp").read_text()
policy = (root / "glue/rexglue-sdk-main/gta4-recomp/src/android/g925_run_ahead_policy.h").read_text()
launcher = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
activity = (root / "os/android/app/src/main/java/com/libertyrecomp/LibertyActivity.java").read_text()
defaults = (root / "os/android/app/src/main/assets/default_args.txt").read_text()
native = (root / "os/android/scripts/build_native.sh").read_text()
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()
gradle = (root / "os/android/app/build.gradle.kts").read_text()

assert re.search(r"REXCVAR_DEFINE_UINT32\(gta4_extra_frames_in_flight, 4,", hook)
assert ".range(0, 4);" in hook
assert "pending = gta4::android::G925AdjustedPendingFrames(pending, extra);" in hook
assert "if (pending >= 0x80000000u) return;" in hook
assert "pending >= 2" in hook and "nanosleep(&delay, nullptr);" in hook
assert "std::min(requested_extra, 4u)" in policy
assert '"Run-ahead frames"' in launcher
assert 'new String[] {"Default (4)", "0", "1", "2", "3", "4"}' in launcher
assert 'new String[] {"0", "1", "2", "3", "4"}' in launcher
assert '--gta4_native_max_queued_frames=4' in defaults
assert '{"--gta4_native_max_queued_frames=3", "--gta4_native_max_queued_frames=4"}' in activity
assert '--gta4_extra_frames_in_flight=0' in launcher  # Clean benchmark remains available.
assert "JOBS=\"\u0024{1:-\u0024(( \u0024(nproc) > 2 ? \u0024(nproc) - 1 : 1 ))}\"" in native
assert "python tools/test_g925_runahead_v228.py" in workflow
assert re.search(r"versionCode\s*=\s*56", gradle)
print("PASS: 4 extra guest frames, native queue bounded, faster build, C++ replay and clean baseline")

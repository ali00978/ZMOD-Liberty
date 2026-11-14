#!/usr/bin/env python3
"""Regression guards for stable ADPF 16 ms feedback and unambiguous profiles."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
src = (root / "glue/rexglue-sdk-main/gta4-recomp/src/android/android_performance_hint.cpp").read_text()
native_build = (root / "os/android/scripts/build_native.sh").read_text()
app = root / "os/android/app/src/main/java/com/libertyrecomp"
launcher = (app / "LauncherActivity.java").read_text()
report = (app / "G925NativeProfileReport.java").read_text()
gradle = (root / "os/android/app/build.gradle.kts").read_text()
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()

# The tested 16ms feedback and stable hint policy must remain selectable.
assert 'new String[] {"Default (25ms)", "Fast (16ms)", "Balanced (33ms)",' in launcher
assert 'new String[] {"16", "33", "100"}' in launcher
assert 'new String[] {"Default (stable)", "Stable", "Original (switching)"}' in launcher
assert 'REXCVAR_DEFINE_BOOL(android_performance_hint_stable_threads, true' in src
assert 'std::clamp<uint32_t>(' in src
assert 'REXCVAR_GET(android_performance_hint_report_interval_ms), 16, 100' in src
assert 'if (hot.empty() && session) {' in src
assert 'Log("no live frame workers; session closed", 0);' in src
assert 'session_target = 0;' in src

# 30FPS is not forced by the capture command; the performance settings persist.
assert 'CAPTURE CPU + GPU BOTTLENECK' in launcher
assert 'does NOT enable a 30 FPS cap' in launcher
assert 'play(true, true)' in launcher
assert 'G925NativeProfileReport.captureSettings(mDataRoot)' in launcher
assert 'G925NativeProfileReport.summarize(mDataRoot, started, settings)' in launcher
assert '.putString("capture_settings",' in launcher
assert 'captureSettings(File root)' in report
assert 'BuildConfig.VERSION_NAME' in report
assert 'cpu_frame_interval_ms' in report
assert 'csv.lastModified() < captureStartedAt' in report

# Android native builds should actually use sccache, not merely install it.
assert 'CMAKE_C_COMPILER_LAUNCHER:STRING' in native_build
assert 'CMAKE_CXX_COMPILER_LAUNCHER:STRING' in native_build
assert 'SCCACHE_CMAKE_ARGS' in native_build
assert int(re.search(r'versionCode\s*=\s*(\d+)', gradle)[1]) >= 34
assert re.search(r'versionSuffix\s*=\s*"-alpha-2\.\d+-g925-', gradle)
assert 'branches: [feature/g925-stable-16ms-v26]' in workflow
assert workflow.count('python tools/test_g925_stable_16ms_v26.py') == 2
print("PASS: 16ms / stable preserved; native hint lifecycle, reports and CMake cache wired")

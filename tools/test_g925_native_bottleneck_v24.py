#!/usr/bin/env python3
"""Check one-shot delayed native CPU/GPU capture and safe share report."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
app = root / "os/android/app/src/main/java/com/libertyrecomp"
activity = (app/"LibertyActivity.java").read_text()
launcher = (app/"LauncherActivity.java").read_text()
report = (app/"G925NativeProfileReport.java").read_text()
flow = (root/".github/workflows/build-android-apk.yml").read_text()
native = (root/"glue/rexglue-sdk-main/src/graphics/gta4_native/native_performance_samples.cpp").read_text()
assert 'EXTRA_G925_NATIVE_PROFILE = "g925_native_profile"' in activity
assert '--diagnostics_categories=logging,native-profiler' in activity
assert '--gta4_profile_native_autostart=false' in activity
assert '--gta4_profile_native_capture_delay_seconds=65' in activity
assert '--gta4_profile_native_samples=180' in activity
assert '--gta4_profile_native_gpu_query_budget=64' in activity
assert 'CAPTURE 30 FPS BOTTLENECK' in launcher
assert 'SHARE NATIVE CPU + GPU REPORT' in launcher
assert 'play(true, true)' in launcher
assert 'G925NativeProfileReport.summarize(mDataRoot, started)' in launcher
assert 'csv.lastModified() < captureStartedAt' in report
assert 'cpu_fence_wait_ms' in report
assert 'cpu_command_recording_ms' in report
assert 'gpu_frame_ms' in report
assert 'Values are not' not in report
assert '"frame-interval"' in native
assert 'branches: [feature/g925-batched-render-cpu-v25]' in flow
print("PASS: delayed native profiling; actual frame timings; source CSV filtered to no-private-data summary")

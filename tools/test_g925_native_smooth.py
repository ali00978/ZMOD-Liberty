#!/usr/bin/env python3
"""Static checks for native frame-stability experiment; real G925 FPS must be measured."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
hint = (root / "glue/rexglue-sdk-main/gta4-recomp/src/android/android_performance_hint.cpp").read_text()
hook = (root / "glue/rexglue-sdk-main/gta4-recomp/src/android/android_frame_wait_hook.cpp").read_text()
java = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()
assert '#include <array>' in hint
assert 'std::array<int64_t, 256> g_frame_durations{};' in hint
assert 'std::copy_n(g_frame_durations.begin(), duration_count, durations.begin());' in hint
assert 'durations.swap(g_frame_durations)' not in hint
assert 'g_frame_durations.push_back' not in hint
assert 'if (!REXCVAR_GET(android_performance_hint)) return;' in hint
assert 'utime + stime >= last->second' in hint
assert 'REXCVAR_DEFINE_BOOL(gta4_frame_pacing_log, false' in hook
assert 'is_new_frame || ((++polls & 63u) == 0u)' in hook
assert 'gaps-over-50ms' in hook
assert 'if (collect_spikes)' in hook
assert 'REXCVAR_DEFINE_UINT32(gta4_extra_frames_in_flight, 0' in hook
assert '.range(0, 2)' in hook, "Run-ahead remains originally bounded"
assert "ORIGINAL CPU HINT / 20 MS" in java
assert "MALI CPU HINT / 20 MS" in java
assert "chooseG925CpuPolicy(true)" in java
assert "chooseG925CpuPolicy(false)" in java
assert "--android_perf_hint=true" in java
assert "--android_performance_hint=false" in java
assert "gta4_frame_pacing_log" in java
assert 'feature/g925-native-smooth-lab' in workflow
assert "github.ref == 'refs/heads/feature/g925-native-smooth-lab'" in workflow
assert "github.event_name == 'workflow_dispatch' && inputs.build_mode == 'java-only'" in workflow
print("PASS: allocation-free title sampling, guarded clocks/logging, mutually exclusive CPU hints, full native build required")

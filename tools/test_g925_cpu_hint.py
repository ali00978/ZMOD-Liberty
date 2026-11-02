#!/usr/bin/env python3
"""Static coverage for optional G925 Extreme CPU Boost (not a live perf test)."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
launcher = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text(encoding="utf-8")
hint = (root / "glue/rexglue-sdk-main/gta4-recomp/src/android/android_performance_hint.cpp").read_text(encoding="utf-8")

assert '"Extreme (16 ms)"' in launcher
assert 'new String[] {"16", "20", "25", "30", "33"}' in launcher
assert 'new String[] {"android_performance_hint_target_ms"}' in launcher
assert 'REXCVAR_DEFINE_UINT32(android_performance_hint_target_ms, 25' in hint
assert '.range(10, 50)' in hint
assert 'APerformanceHint_reportActualWorkDuration' in hint
assert 'APerformanceHint_updateTargetWorkDuration' in hint
print("PASS: G925 Extreme Boost is a valid optional 16ms ADPF hint")

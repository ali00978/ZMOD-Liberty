#!/usr/bin/env python3
"""Static regression: real Vulkan host paint phase timings, no GPU sync added."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
header = (root/"glue/rexglue-sdk-main/include/rex/ui/vulkan/presenter.h").read_text()
probe = (root/"glue/rexglue-sdk-main/include/rex/ui/vulkan/g925_paint_window.h").read_text()
hint = (root/"glue/rexglue-sdk-main/gta4-recomp/src/android/android_performance_hint.cpp").read_text()
power = (root/"glue/rexglue-sdk-main/gta4-recomp/src/android/android_power.cpp").read_text()
java = (root/"os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
workflow = (root/".github/workflows/build-android-apk.yml").read_text()
assert "#include <rex/ui/vulkan/g925_paint_window.h>" in header
assert "g925_paint_window_.Observe(acquire_ticks, submit_ticks, present_ticks," in header
assert "G925PaintWindow g925_paint_window_{};" in header
assert "g925-paint: paint-hz=%.1f new-title-hz=%.1f" in probe
assert "acquire-max-ms=%.2f" in probe
assert "submit-max-ms=%.2f" in probe
assert "present-max-ms=%.2f" in probe
assert "total-max-ms=%.2f" in probe
assert "HOST-CPU-only" in probe
assert "vkQueueWaitIdle" not in probe and "vkDeviceWaitIdle" not in probe
assert "performance-hint: config enabled=%d stable=%d" in hint
assert "legacy-hint=%s" in power
assert 'line.contains("g925-paint:")' in java
assert 'branches: [feature/g925-presenter-stutter-v23]' in workflow
print("PASS: existing GPU-presenter host timestamps, ADPF effective config and filtered reports")

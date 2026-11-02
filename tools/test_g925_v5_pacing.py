#!/usr/bin/env python3
"""Static regression tests for G925 v5 pacing experiment (not an on-device FPS test)."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
java = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
hook = (root / "glue/rexglue-sdk-main/gta4-recomp/src/android/android_frame_wait_hook.cpp").read_text()
policy = (root / "glue/rexglue-sdk-main/gta4-recomp/src/android/g925_frame_queue_policy.h").read_text()
hint = (root / "glue/rexglue-sdk-main/gta4-recomp/src/android/android_performance_hint.cpp").read_text()
present = (root / "glue/rexglue-sdk-main/src/ui/vulkan/vulkan_presenter.cpp").read_text()
gradle = (root / "os/android/app/build.gradle.kts").read_text()
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()

assert 'gta4_extra_frames_in_flight, 0' in hook
assert ".range(0, gta4::android::kG925MaxRunAhead)" in hook
assert "G925EffectivePending(pending, extra)" in hook
assert "kG925MaxRunAhead = 4" in policy
assert "static_assert(G925EffectivePending(10, UINT32_MAX) == 6);" in policy
assert 'new String[] {"0", "1", "2", "3", "4"}' in java
assert "G925 STANDARD / 60 FPS + RA4" in java
assert "G925 STUTTER FALLBACK / RA0" in java
assert "initializeG925V5Pacing();" in java
start = java.index("private static final String[][] G925_STANDARD_60_PROFILE")
end = java.index("private static final String[][] G925_SAFE_PACING_PROFILE", start)
profile = dict(re.findall(r'\\{"([^"]+)", "([^"]+)"\\}', java[start:end]))
assert profile["gta4_extra_frames_in_flight"] == "4", profile
assert profile["vulkan_prefer_present_mode_fifo"] == "true", profile
assert profile["gta4_frame_limit"] == "60", profile
assert "vulkan_prefer_present_mode_fifo" in present
assert "Strict FIFO (test)" in java
migration = java[java.index("private void initializeG925V5Pacing()"):
                 java.index("private void restoreG925PreviousGraphics()")]
assert "if (marker.isFile()) return;" in migration
assert 'java.nio.file.Files.copy(graphics.toPath(), backup.toPath())' in migration
assert '--gta4_extra_frames_in_flight=4' in migration
assert '--vulkan_prefer_present_mode_fifo=true' in migration
assert "existing visuals preserved" in migration
assert "title-max-gap-ms" in hook and "title-gaps-over-50ms" in hook
assert "utime + stime >= last->second" in hint
assert "versionCode   = 25" in gradle
assert "-alpha-1.7-g925-runahead4-fifo" in gradle
assert "feature/g925-v5-runahead4-pacing" in workflow
print("PASS: RA4 native range and queue saturation, FIFO switch, safe migration, frametime diagnostics")

#!/usr/bin/env python3
"""Regression checks for the untouched official Mali renderer and safe reset.

Static checks do not establish FPS improvements; measure on device.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
java = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
gradle = (root / "os/android/app/build.gradle.kts").read_text()
native = (root / "glue/rexglue-sdk-main/src/graphics/gta4_native/native_frame_scheduling.h").read_text()
args = (root / "os/android/app/src/main/assets/default_args.txt").read_text()
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()
assert "versionCode   = 29" in gradle
assert "-alpha-2.1-g925-pipeline-stability" in gradle
assert "OFFICIAL MALI / CLEAN PERFORMANCE BASELINE" in java
assert "CLEAN ORIGINAL SETTINGS + 60 FPS" in java
assert "ORIGINAL GRAPHICS / STEADY 40 FPS" in java
assert "ORIGINAL GRAPHICS / 60 FPS CAP" in java
assert "hasLegacyG925Experiment()" in java
assert "restoreOfficialConfig(\"60\")" in java
assert "g925-official-backups" in java
assert "native.toml" in java
assert "getAssets().open(\"default_args.txt\")" in java
assert "perfTestPreferences().edit().clear().commit()" in java
assert "mOverlayBoxes" in java and "checkbox.setChecked(false)" in java
assert 'if (!key.startsWith("gta4_touch")) continue;' in java
assert 'new File(mDataRoot, "driver.txt")' not in java[java.index("private boolean restoreOfficialConfig"):java.index("private static void writeCleanConfig")]
assert 'new File(mDataRoot, "LibertyRecomp"), "native.toml"' in java
assert '--gta4_frame_limit=' in java
assert '--gta4_extra_frames_in_flight=0' in java
assert "kNativeTextureEvictionGraceFrames = 240;" in native
assert "kNativeTextureCacheRetentionFrames = 600;" in native
assert "kNativeBufferCacheRetentionFrames = 600;" in native
assert "kNativeBufferCacheLimitMiB = 256;" in native
assert "--gta4_native_max_queued_frames=3" in args
assert "41363883009" in workflow
assert "feature/g925-native-pipeline-stability-v21" in workflow
assert 'applyG925Preset(new String[][]' not in java
print("PASS: original Mali renderer, no G925 engine hacks, safe complete settings reset and 40/60 A-B")

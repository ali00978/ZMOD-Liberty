#!/usr/bin/env python3
"""Static checks for genuine older native engine + A/B defaults."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
gradle = (root / "os/android/app/build.gradle.kts").read_text()
native = (root / "glue/rexglue-sdk-main/src/graphics/gta4_native/native_frame_scheduling.h").read_text()
args = (root / "os/android/app/src/main/assets/default_args.txt").read_text()
flow = (root / ".github/workflows/build-android-apk.yml").read_text()
assert "versionCode   = 26" in gradle and "-alpha-1.8-stock-stable" in gradle
assert "initializeG925StockStableDefaults();" in src
assert src.index("initializeG925StockStableDefaults();") < src.index("setContentView(buildLayout());")
assert 'if (!DriverBridge.isMaliBuild()) return;' in src
assert 'if (marker.isFile()) return;' in src
assert 'if (!backup.isFile())' in src
assert src.index("java.nio.file.Files.copy(graphics.toPath(), backup.toPath())") < src.index("java.nio.file.Files.move(temporary.toPath(), graphics.toPath()")
assert '"--gta4_frame_limit=30' in src and '"--gta4_extra_frames_in_flight=0' in src
assert 'if (!key.startsWith("gta4_touch")) continue;' in src
assert 'Button stock30 = button("G925 STOCK 30 - SMOOTH"' in src
assert 'Button stock60 = button("G925 STOCK 60 - TEST"' in src
assert 'applyG925StockPreset("30")' in src and 'applyG925StockPreset("60")' in src
assert '{"gta4_extra_frames_in_flight", "0"}' in src
assert 'new String[] {"Default", "0", "1", "2"}' in src
for key, value in {
    "kNativeTextureEvictionGraceFrames": "240",
    "kNativeTextureCacheRetentionFrames": "600",
    "kNativeBufferCacheRetentionFrames": "600",
    "kNativeBufferCacheLimitMiB": "256",
}.items():
    assert f"{key} = {value};" in native
assert "--gta4_dynamic_draw_distance=true" in args
assert "--gta4_traffic_density_scale=0.4" in args
assert "41363883009" in flow and "feature/g925-stock-stable-rollback" in flow
print("PASS: stock native libraries, original visuals, safe migration, 30/60 A/B")

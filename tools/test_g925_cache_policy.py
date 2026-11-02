#!/usr/bin/env python3
"""G925 v2 static safety checks. The native APK build is the compile test."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / "glue/rexglue-sdk-main/src/graphics/gta4_native/native_frame_scheduling.h").read_text(encoding="utf-8")
tests = (root / "glue/rexglue-sdk-main/tests/unit/graphics/native_frame_scheduling_test.cpp").read_text(encoding="utf-8")
gradle = (root / "os/android/app/build.gradle.kts").read_text(encoding="utf-8")
workflow = (root / ".github/workflows/build-android-apk.yml").read_text(encoding="utf-8")

values = {
    "kNativeTextureEvictionGraceFrames": 300,
    "kNativeTextureCacheRetentionFrames": 720,
    "kNativeTextureBudgetReserveDivisor": 10,
    "kNativeTexturePressureEnterPercent": 90,
    "kNativeTexturePressureExitPercent": 85,
    "kNativeBufferCacheRetentionFrames": 720,
    "kNativeBufferCacheLimitMiB": 320,
}
for name, expected in values.items():
    match = re.search(rf"inline constexpr uint32_t {name} = (\d+);", source)
    assert match is not None, name
    assert int(match.group(1)) == expected, (name, match.group(1), expected)

assert "if (referenced)" in source
assert "ShouldReclaimNativeBuffer" in source
assert "UpdateNativeTexturePressure" in source
assert "ShouldRetryNativeTextureAllocation" in source
assert "G925 v2 extended caches preserve safety budget and expiry policy" in tests
assert "versionCode   = 21" in gradle
assert '-alpha-1.5-g925-perf2' in gradle
assert "feature/g925-performance-v2" in workflow
print("PASS: G925 v2 modest cache extension with existing budget/reclaim protections")

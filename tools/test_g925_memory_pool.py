#!/usr/bin/env python3
"""Source-level checks for the experimental G925 Android GPU allocation reuse pool.

This does not replace native compilation or benchmarking on a Mali G925 device.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
header = (root / "glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.h").read_text(encoding="utf-8")
pool = (root / "glue/rexglue-sdk-main/src/graphics/gta4_native/native_image_reuse.h").read_text(encoding="utf-8")
launcher = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text(encoding="utf-8")
options = (root / "glue/rexglue-sdk-main/src/graphics/gta4_native/core/options.cpp").read_text(encoding="utf-8")

assert header.count("texture_allocation_pool_{96ULL * 1024 * 1024, 128}") == 1
assert header.count("texture_allocation_pool_{33554432, 64}") == 1
assert header.index("#if defined(__ANDROID__)") < header.index("texture_allocation_pool_{96ULL * 1024 * 1024, 128}")
assert header.index("texture_allocation_pool_{96ULL * 1024 * 1024, 128}") < header.index("#else")
assert header.index("#else") < header.index("texture_allocation_pool_{33554432, 64}") < header.index("#endif")
assert "byte_limit_(bytes), entry_limit_(entries)" in pool
assert "last_submission <= completed_submission" in pool
assert "bytes_ <= byte_limit_ - bytes" in pool
assert "Trim(uint32_t frame" in pool

assert 'new GraphicsSetting("Anisotropic filtering"' in launcher
assert '"gta4_anisotropic_filtering"' in launcher
for value in ("4x", "8x", "16x"):
    assert '{"gta4_anisotropic_filtering", "' + value + '"}' in launcher
assert "REXCVAR_DEFINE_STRING(gta4_anisotropic_filtering" in options
assert '.allowed({"1x", "2x", "4x", "8x", "16x"})' in options

print("PASS: Android G925 96MiB GPU image reuse ceiling, desktop default, image lifecycle guards, and 4x/8x/16x anisotropy")

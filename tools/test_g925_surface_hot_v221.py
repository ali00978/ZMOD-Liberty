#!/usr/bin/env python3
"""Validate four-way exact surface MRU with per-revision lifetime safety."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
header = (SDK / "src/graphics/gta4_native/native_draw_lookup_cache.h").read_text()
source = (SDK / "src/graphics/gta4_native/graphics_system.cpp").read_text()
assert "hot_.size()" not in header  # fixed two-entry storage, no dynamic allocation
assert "mutable std::array<Entry, 4> hot_{};" in header
assert "hot.epoch == epoch_ && hot.image" in header
assert "hot.registry_revision == registry_revision && hot.key == key" in header
assert "const size_t start = SetIndex(key) * kWays;" in header
assert header.index("hot.registry_revision == registry_revision") < header.index("const size_t start = SetIndex(key) * kWays;")
assert "Remember(key, registry_revision, entry.image);" in header
assert "Forget(entries_[start + victim]);" in header
assert "for (auto& hot : hot_) hot.image = nullptr;" in header
assert "hot_order_ = {0, 1, 2, 3};" in header
assert "PromoteHot(probe);" in header
assert "Forget(entry);" in header
assert "surface_lookup_cache_.Invalidate();" in source
assert "surface_lookup_cache_.Find(" in source

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++20 compiler is required for G925 cache tests")
with tempfile.TemporaryDirectory(prefix="g925-surface-mru-") as temporary:
    exe = Path(temporary) / ("surface.exe" if os.name == "nt" else "surface")
    command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
               "-I" + str(SDK / "src"),
               str(ROOT / "tools/test_g925_surface_hot_v221.cpp"),
               "-o", str(exe)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        command[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(command, check=True)
    subprocess.run([str(exe)], check=True, timeout=40)
print("PASS: surface MRU, guest release invalidation, exact registry revisions and Vulkan lifetime safety")

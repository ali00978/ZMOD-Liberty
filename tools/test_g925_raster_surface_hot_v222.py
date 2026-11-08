#!/usr/bin/env python3
"""Run exact raster cache and GPU surface hot-lookup safety replays."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
exact = (SDK / "src/graphics/gta4_native/native_exact_memo.h").read_text()
draw = (SDK / "src/graphics/gta4_native/native_draw_lookup_cache.h").read_text()
raster = (SDK / "src/graphics/gta4_native/native_draw_preparation.h").read_text()
assert "using NativeRasterAreaMemo = NativeExactMemo<NativeRasterAreaKey, NativeRasterArea>" in raster
body = exact.split("const Value& Get(const Key& key, Compute compute)", 1)[1]
body = body.split("  void Reset()", 1)[0]
assert "Promote(position);" in body
assert "Value computed = compute();" in body
assert "entry.value = std::move(computed);" in body
assert "entry.valid = true;" in body
assert "return Store(key, compute());" not in body
assert "const Value* value = Find(key)" not in body
assert "hot.key.descriptor[0] != key.descriptor[0]" in draw
assert "hot.key.depth != key.depth || hot.key.samples != key.samples" in draw
assert "hot.registry_revision == registry_revision && hot.key == key" in draw
assert "hot.epoch == epoch_ && hot.image" in draw
assert "Forget(entries_[start + victim]);" in draw
assert "for (auto& hot : hot_) hot.image = nullptr;" in draw

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++20 host compiler is required for G925 v2.22")
with tempfile.TemporaryDirectory(prefix="g925-raster-surface-v222-") as folder:
    exe = Path(folder) / ("raster.exe" if os.name == "nt" else "raster")
    command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
               "-I" + str(SDK / "src"),
               str(ROOT / "tools/test_g925_raster_surface_hot_v222.cpp"),
               "-o", str(exe)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        command[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(command, check=True)
    subprocess.run([str(exe)], check=True, timeout=35)
print("PASS: production raster memo and surface lookup prefilter, 150k lookup replays")

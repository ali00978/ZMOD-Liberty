#!/usr/bin/env python3
"""Run real v2.16 native-render cache tests before assembling an Android APK."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
native = SDK / "src/graphics/gta4_native"
pipeline = (native / "native_pipeline_lookup_memo.h").read_text()
surface = (native / "native_draw_lookup_cache.h").read_text()
resolve = (native / "native_resolve_descriptor_cache.h").read_text()
assert "mutable size_t last_hit_" in pipeline
assert "if (matches(last_hit_))" in pipeline
assert "key.descriptor[5] * 0x85EBCA6Bu" in surface
assert "key.descriptor[4] * 0xC2B2AE35u" in surface
assert "key.descriptor[3] * 0x27D4EB2Du" in surface
assert "hot_.valid && hot_.key == key" in resolve
assert "entries_.clear(); hot_.valid = false;" in resolve
compiler = os.environ.get("G925_TEST_CXX") or shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("C++20 compiler required")
with tempfile.TemporaryDirectory(prefix="g925-hotpath-") as tmp:
    exe = Path(tmp) / ("test.exe" if os.name == "nt" else "test")
    args = [
        compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I" + str(SDK / "src"),
        str(ROOT / "tools/test_g925_render_hotpaths_v216.cpp"),
        "-o", str(exe),
    ]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        args[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(args, check=True)
    subprocess.run([str(exe)], check=True, timeout=20)
print("PASS: production pipeline/surface/descriptor fast paths and native replay")

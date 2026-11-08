#!/usr/bin/env python3
"""Check production GPU-frame constant and index conversion hot paths."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
constant = (SDK / "src/graphics/gta4_native/frame_constant_arena.h").read_text()
indices = (SDK / "src/graphics/gta4_native/native_index_conversion.h").read_text()
assert "InsertResult FindOrInsertWith(const Key& key, Factory&& factory)" in constant
assert "std::optional<Value> candidate = factory();" in constant
assert "if (!candidate) return {};" in constant
arena = constant.split("FindOrReserve(const FrameConstantIdentity& identity,", 1)[1]
arena = arena.split("  bool MarkSubmitted(", 1)[0]
assert "reservations_.FindOrInsertWith(identity," in arena
assert "reservations_.Find(identity)" not in arena
assert "reservations_.Insert(identity," not in arena
assert "if (insertion.inserted) byte_cursor_" in arena
assert "stored.byte_size != byte_size" in arena
assert "std::memcpy(dst, src, size_t(key.count) * element);" in indices
assert "if ((value & key.index_mask) == key.restart_index)" in indices
assert "if (value == key.restart_index)" in indices

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++20 compiler is required")
with tempfile.TemporaryDirectory(prefix="g925-constant-index-") as tmp:
    exe = Path(tmp) / ("test.exe" if os.name == "nt" else "test")
    args = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(SDK / "src"),
            str(ROOT / "tools/test_g925_constant_index_v220.cpp"),
            "-o", str(exe)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        args[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(args, check=True)
    subprocess.run([str(exe)], check=True, timeout=45)
print("PASS: production single-pass constant cache and exact restart conversion")

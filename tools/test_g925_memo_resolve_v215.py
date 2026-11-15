#!/usr/bin/env python3
"""Run production exact CPU-cache tests (four-way MRU) on the CI host."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sdk = root / "glue/rexglue-sdk-main"
exact = (sdk / "src/graphics/gta4_native/native_exact_memo.h").read_text()
vertex = (sdk / "src/graphics/gta4_native/native_draw_lookup_cache.h").read_text()
resolve = (sdk / "src/graphics/gta4_native/native_resolve_descriptor_cache.h").read_text()
assert "std::array<Entry, kSlots> entries_" in exact
assert "mutable std::array<size_t, kSlots> order_" in vertex
assert "entries_.erase(entries_.begin())" in resolve
assert "entries_.reserve(kCapacity)" in resolve
assert "entries_.size() >= kCapacity) entries_.clear()" not in resolve

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("A C++20 host compiler is required")
with tempfile.TemporaryDirectory(prefix="g925-memo-resolve-") as directory:
    executable = Path(directory) / ("memo.exe" if os.name == "nt" else "memo")
    command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
               "-I" + str(sdk / "src"),
               str(root / "tools/test_g925_memo_resolve_v215.cpp"),
               "-o", str(executable)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        command[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True, timeout=20)
print("PASS: four-way exact CPU memos and bounded resolve descriptor pool")

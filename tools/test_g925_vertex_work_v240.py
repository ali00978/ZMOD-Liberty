#!/usr/bin/env python3
"""G925 v2.40: no index-span wrap, no wasted tiny/stale vertex work."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sdk = root / "glue/rexglue-sdk-main/src/graphics/gta4_native"
cache = (sdk / "native_vertex_cache.h").read_text(encoding="utf-8")
budget = (sdk / "g925_vertex_job_budget.h").read_text(encoding="utf-8")
workflow = (root / ".github/workflows/build-android-apk.yml").read_text(encoding="utf-8")

assert "G925VertexRangeWorthOptimizing(index_count)" in cache
assert cache.index("G925VertexRangeWorthOptimizing(index_count)") < cache.index("job.source.resize(bytes)")
assert "vertex_span > UINT32_MAX" in cache
assert "uint64_t(maximum) - uint64_t(minimum) + 1u" in cache
assert "job.range.use_count() == 1" in cache
assert "NativeVertexCacheRange::kUnusable" in cache
assert "kG925MinVertexOptimizationTriangles = 96u" in budget
assert "python tools/test_g925_vertex_work_v240.py" in workflow

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("C++20 compiler required for G925 2.40 regression")
with tempfile.TemporaryDirectory(prefix="g925-vertex-v240-") as temp:
    binary = Path(temp) / ("vertex.exe" if os.name == "nt" else "vertex")
    command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
               str(root / "tools/test_g925_vertex_work_v240.cpp"),
               "-o", str(binary)]
    if os.name != "nt":
        command.insert(-2, "-pthread")
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        command[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(command, check=True, timeout=120)
    subprocess.run([str(binary)], check=True, timeout=45)
print("PASS: overflow-safe 16/32-bit mesh replay and lower G925 optimizer CPU work")

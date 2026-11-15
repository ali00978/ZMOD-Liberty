#!/usr/bin/env python3
"""G925 v2.34: test exact mixed sparse/contiguous uploads on the host."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sdk = root / "glue/rexglue-sdk-main"
header = (sdk / "src/graphics/gta4_native/native_sparse_index_batch.h").read_text()
assert "size_t run_first = ranges.front().first;" in header
assert "uint64_t(run_first) + run_count == range.first" in header
assert "flush_run();" in header
assert "NativeSparseTriangleIndexCount(ranges, source.size() / element)" in header
assert "std::memcpy(destination.data() + output_offset," in header

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++20 host compiler is required")
with tempfile.TemporaryDirectory(prefix="g925-coalesced-upload-") as temp:
    binary = Path(temp) / ("test.exe" if os.name == "nt" else "test")
    command = [
        compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I" + str(sdk / "src"),
        str(root / "tools/test_g925_sparse_upload_v234.cpp"),
        "-o", str(binary),
    ]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        command[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True, timeout=45)
print("PASS: exact coalesced sparse uploads (16/32-bit, repeated, overlapping, bounds)")

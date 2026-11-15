#!/usr/bin/env python3
"""G925 v2.36: lossless medium-mesh indexed-batch gate and host replay."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sdk = root / "glue/rexglue-sdk-main"
header = (sdk / "src/graphics/gta4_native/native_sparse_index_batch.h").read_text()
renderer = (sdk / "src/graphics/gta4_native/graphics_system.cpp").read_text()
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()
gradle = (root / "os/android/app/build.gradle.kts").read_text()

assert re.search(r"kNativePairedSparseBatchMaxIndices\s*=\s*1536\s*;", header)
assert re.search(r"kNativeSmallSparseBatchMaxIndices\s*=\s*3072\s*;", header)
assert "sparse_allowed && ShouldBatchNativeSparseIndexDraws(" in renderer
assert "WriteNativeSparseTriangleIndices(" in renderer or "WriteNativeSparseTriangleIndices(" in header
for safety_gate in (
    "!command.fixed_function_state.blend_enable",
    "!command.fixed_function_state.stencil_enable",
    "!command.pipeline_state->vertex_shader_resource->uses_vertex_index",
    "translucent_query_index == UINT32_MAX",
    "water_query_index == UINT32_MAX",
    "other.pipeline_state != command.pipeline_state",
    "other.shader_state != command.shader_state",
    "other.index_buffer != command.index_buffer",
):
    assert safety_gate in renderer, safety_gate
assert "python tools/test_g925_indexed_batch_coverage_v236.py" in workflow
assert (version := re.search(r"versionCode\s*=\s*(\d+)", gradle)) and int(version.group(1)) >= 64
assert re.search(r"-alpha-2\.(?:3[6-9]|[4-9]\d)-g925-[^"]+", gradle)

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++20 compiler is required")
with tempfile.TemporaryDirectory(prefix="g925-medium-batch-v236-") as folder:
    binary = Path(folder) / ("check.exe" if os.name == "nt" else "check")
    command = [
        compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I" + str(sdk / "src"),
        str(root / "tools/test_g925_indexed_batch_coverage_v236.cpp"),
        "-o", str(binary),
    ]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        command[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True, timeout=45)
print("PASS: larger lossless 2/3-draw batch eligibility, byte-exact 16/32-bit geometry, safety guards")

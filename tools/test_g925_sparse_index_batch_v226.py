#!/usr/bin/env python3
"""G925 2.26: production sparse GPU draw batching and exact C++20 replay."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
header = (SDK / "src/graphics/gta4_native/native_sparse_index_batch.h").read_text()
source = (SDK / "src/graphics/gta4_native/graphics_system.cpp").read_text()
graphics_header = (SDK / "src/graphics/gta4_native/graphics_system.h").read_text()
counter_h = (SDK / "src/graphics/gta4_native/native_performance_samples.h").read_text()
counter_cpp = (SDK / "src/graphics/gta4_native/native_performance_samples.cpp").read_text()
assert "kNativeSparseIndexBatchMaxIndices = 65536" in header
assert "WriteNativeSparseTriangleIndices(" in header
assert "uint64_t(range.first) + range.count > source_elements" in header
assert "range.count % 3 != 0" in header
assert "std::memcpy(destination.data() + output_offset," in header
assert "sparse_indexed_range_count_ = 0;" in graphics_header
assert "gta4_native_merge_sparse_indexed_draws" in source
assert "sparse_indexed_ranges_[sparse_count++]" in source
assert "ShouldBatchNativeSparseIndexDraws(" in source
assert "sparse_count, merged_count, sparse_indices, sparse_has_gap" in source
assert "!command.fixed_function_state.blend_enable" in source
assert "!command.fixed_function_state.stencil_enable" in source
assert "!command.pipeline_state->vertex_shader_resource->uses_vertex_index" in source
assert "fire_query == UINT32_MAX" in source
assert "translucent_query_index == UINT32_MAX" in source
assert "water_query_index == UINT32_MAX" in source
assert "RecordIndexedPrimitive(command_buffer, command, target.width," in source
assert "host_index_count = total;" in source
assert "host_start_index = 0;" in source
assert "const bool sparse_batch = sparse_indexed_range_count_ >= 3;" in source
assert "REXCVAR_GET(gta4_native_vertex_cache_order) && !sparse_batch" in source
assert "kSparseIndexedDrawsMerged" in source and "kSparseIndexedDrawsMerged" in counter_h
assert "sparse-indexed-draws-merged" in counter_cpp
# The failed fused call must clear the range override before retrying unchanged.
fragment = source[source.index('const bool use_sparse_batch ='):source.index(
    'trace_deferred_light_draw(command_recorded);', source.index('const bool use_sparse_batch ='))]
assert fragment.index('sparse_indexed_range_count_ = 0;') < fragment.index(
    '// Upload/capacity failure')
assert fragment.index('merged_up_until = command_index + sparse_count;') < fragment.index(
    '// Upload/capacity failure')
compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++20 compiler is required")
with tempfile.TemporaryDirectory(prefix="g925-sparse-v226-") as temp:
    exe = Path(temp) / ("test.exe" if os.name == "nt" else "test")
    args = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(SDK / "src"),
            str(ROOT / "tools/test_g925_sparse_index_batch_v226.cpp"),
            "-o", str(exe)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        args[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(args, check=True)
    subprocess.run([str(exe)], check=True, timeout=45)
print("PASS: exact sparse guest triangle batches and production renderer fallback")

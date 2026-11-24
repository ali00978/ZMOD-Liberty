#!/usr/bin/env python3
"""G925 v2.44: exact opaque vertex-offset batch fusion, no image-quality tradeoff."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
header = (SDK / "src/graphics/gta4_native/native_sparse_index_batch.h").read_text()
renderer = (SDK / "src/graphics/gta4_native/graphics_system.cpp").read_text()
pipeline = (ROOT / ".github/workflows/build-android-apk.yml").read_text()
gradle = (ROOT / "os/android/app/build.gradle.kts").read_text()
launcher = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
report = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/G925NativeProfileReport.java").read_text()
counters_h = (SDK / "src/graphics/gta4_native/native_performance_samples.h").read_text()
counters_cpp = (SDK / "src/graphics/gta4_native/native_performance_samples.cpp").read_text()

for code in (
    "NativeSparseTriangleBatchNeedsRebase(",
    "WriteNativeRebasedSparseTriangleIndices(",
    "std::numeric_limits<int32_t>::max()",
    "const int64_t effective =",
    "const uint32_t index =",
    "destination.data() + written * sizeof(index)",
    "if (effective < 0",
    "int32_t base_vertex = 0;",
):
    assert code in header, code
for code in (
    "REXCVAR_DEFINE_BOOL(gta4_native_merge_rebased_indexed_draws, true",
    "!command.fixed_function_state.blend_enable",
    "!command.fixed_function_state.stencil_enable",
    "!command.pipeline_state->vertex_shader_resource->uses_vertex_index",
    "translucent_query_index == UINT32_MAX",
    "water_query_index == UINT32_MAX",
    "other.vertex_buffers != command.vertex_buffers",
    "other.shader_state != command.shader_state",
    "other.index_buffer != command.index_buffer",
    "other.worker_vertex_constants_offset ==",
    "other.worker_pixel_constants_offset ==",
    "const bool same_base = draw.base_vertex == head.base_vertex;",
    "same_base && uint64_t(draw.start_index) == next_start",
    "sparse_indexed_ranges_[0] = {head.start_index, head.index_count,",
    "sparse_indexed_ranges_[sparse_count++] = {draw.start_index, draw.index_count,",
    "const bool rebased_sparse_batch = sparse_batch &&",
    "NativeSparseTriangleBatchNeedsRebase(",
    "rebased_sparse_batch ? sizeof(uint32_t) : element_size",
    "WriteNativeRebasedSparseTriangleIndices(",
    "if (!copied) return fail(\"sparse-index-rebase\")",
    "if (rebased_sparse_batch) draw.base_vertex = common_base_vertex;",
    "(index32 || rebased_sparse_batch) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16",
    "sparse_indexed_range_count_ = 0;",
    "if (command_recorded) {\n          merged_up_until = command_index + sparse_count;",
):
    assert code in renderer, code

# Same-base raw-index batches and original fallback remain untouched.
assert "WriteNativeSparseTriangleIndices(" in renderer
assert "if (rebased_sparse_batch)" in renderer
assert "gta4_native_merge_indexed_draws" in renderer
assert "if (!same_base &&" in renderer
assert "RecordIndexedPrimitive(command_buffer, command, target.width," in renderer
assert "python tools/test_g925_rebased_index_batch_v244.py" in pipeline
assert "versionCode   = 72" in gradle
assert "-alpha-2.44-g925-rebased" in gradle
assert "ZMOD-Liberty-G925-Native-v3.5-armv9.apk" in pipeline
assert 'new GraphicsSetting("Indexed draw batching"' in launcher
assert 'new String[] {"gta4_native_merge_rebased_indexed_draws"}' in launcher
assert 'new String[] {"true", "false"}' in launcher
assert 'case "Indexed draw batching":' in launcher
assert 'counter_rebased_indexed_draws_merged' in report
assert 'gta4_native_merge_rebased_indexed_draws' in report
assert "kRebasedIndexedDrawsMerged" in counters_h
assert '"rebased-indexed-draws-merged"' in counters_cpp
assert "performance::Counter::kRebasedIndexedDrawsMerged" in renderer
assert not any(key in renderer[renderer.index('REXCVAR_DEFINE_BOOL(gta4_native_merge_rebased_indexed_draws'):
                                 renderer.index('REXCVAR_DEFINE_BOOL(gta4_native_merge_sequential_draws')]
               for key in ("gta4_draw_distance_scale", "gta4_output_height_cap", "gta4_fps_guard"))

tool = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not tool:
    raise SystemExit("C++20 compiler required for index batching correctness")
with tempfile.TemporaryDirectory(prefix="g925-rebased-v244-") as folder:
    exe = Path(folder) / ("rebase.exe" if os.name == "nt" else "rebase")
    cmd = [tool, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
           "-I" + str(SDK / "src"),
           str(ROOT / "tools/test_g925_rebased_index_batch_v244.cpp"),
           "-o", str(exe)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        cmd[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(cmd, check=True, timeout=90)
    subprocess.run([str(exe)], check=True, timeout=90)

print("PASS: 16/32-bit offset-rebased indexed triangles preserve all effective vertices")

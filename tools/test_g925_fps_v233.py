#!/usr/bin/env python3
"""G925 2.33: compiled paired-draw equivalence and production integration."""
from pathlib import Path
import os, re, shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
compiler=os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler: raise SystemExit("C++20 compiler required")
with tempfile.TemporaryDirectory(prefix="g925-fps-v233-") as td:
    exe=Path(td)/("check.exe" if os.name=="nt" else "check")
    subprocess.run([compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
                    str(root/"tools/test_g925_fps_v233.cpp"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=45)
renderer=(root/"glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.cpp").read_text()
header=(root/"glue/rexglue-sdk-main/src/graphics/gta4_native/native_sparse_index_batch.h").read_text()
workflow=(root/".github/workflows/build-android-apk.yml").read_text()
gradle=(root/"os/android/app/build.gradle.kts").read_text()
assert "const bool sparse_batch = sparse_indexed_range_count_ >= 2;" in renderer
assert "REXCVAR_DEFINE_UINT32(gta4_native_merge_up_max, 32," in renderer
assert "kNativePairedSparseBatchMaxIndices = 384" in header
assert "draw_count == 2 && adjacent_count == 1" in header
for guard in ["!command.fixed_function_state.blend_enable",
              "!command.fixed_function_state.stencil_enable",
              "!command.pipeline_state->vertex_shader_resource->uses_vertex_index",
              "translucent_query_index == UINT32_MAX", "water_query_index == UINT32_MAX",
              "other.pipeline_state != command.pipeline_state",
              "other.shader_state != command.shader_state",
              "other.index_buffer != command.index_buffer"]:
    assert guard in renderer, guard
assert re.search(r"versionCode\s*=\s*61",gradle)
assert "-alpha-2.33-g925-paired-batch-fps" in gradle
assert "python tools/test_g925_fps_v233.py" in workflow
print("PASS: exact 2-draw GPU batching, 32-up run cap, rendering safety gates")

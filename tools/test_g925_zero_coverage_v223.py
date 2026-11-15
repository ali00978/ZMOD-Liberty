#!/usr/bin/env python3
"""Production GPU draw-elision wiring and exact host C++ correctness replay."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
main = (SDK / "src/graphics/gta4_native/graphics_system.cpp").read_text()
helper = (SDK / "src/graphics/gta4_native/native_zero_coverage_draw.h").read_text()
header = (SDK / "src/graphics/gta4_native/native_performance_samples.h").read_text()
cpp = (SDK / "src/graphics/gta4_native/native_performance_samples.cpp").read_text()

assert '#include "native_zero_coverage_draw.h"' in main
assert "REXCVAR_DEFINE_BOOL(gta4_native_elide_zero_coverage_draws, true" in main
assert "NativeRasterScissorHasNoPixels(" in main
assert "CanElideNativeZeroCoverageDraw(" in main
assert "fire_query != UINT32_MAX || translucent_query_index != UINT32_MAX" in main
assert "water_query_index != UINT32_MAX" in main
assert "diagnostic_frame || legacy_diagnostics" in main
assert "command.temporal_scene_binding || command.temporal_instance" in main
assert "target.temporal_shader || command.temporal_composite_mode" in main
assert "if (!semantic_lineage_ready)" in main
assert "else if (elide_zero_coverage_draw)" in main
assert "AddNativeGpuProfileCounter(performance::Counter::kZeroCoverageDrawSkips);" in main
assert "kZeroCoverageDrawSkips" in header and '"zero-coverage-draw-skips"' in cpp
assert "NativeRasterScissorHasNoPixels" in helper
# Do not let an active query/diagnostic execute the skip fast path, and
# preserve the earlier render scope and load-op clear operations.
snippet = main.split("const bool potential_zero_coverage_draw =", 1)[1].split(
    "bool command_recorded = false;", 1)[0]
assert snippet.index("NativeRasterScissorHasNoPixels(") < snippet.index(
    "CanElideNativeZeroCoverageDraw(")
assert "REXCVAR_GET(gta4_native_gpu_breadcrumbs)" in snippet
assert "NativeRendererEventTraceEnabled()" in snippet
assert "IsArtificialLightTraceEnabled()" in snippet
assert main.index("const uint32_t fire_query = BeginFireQuery(") < main.index(
    "const bool potential_zero_coverage_draw =")
assert main.index("const bool potential_zero_coverage_draw =") < main.index(
    "else if (elide_zero_coverage_draw)")
assert main.index("vkCmdBeginRendering(command_buffer, &rendering_info)") < main.index(
    "else if (elide_zero_coverage_draw)")

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++20 compiler is mandatory")
with tempfile.TemporaryDirectory(prefix="g925-zero-coverage-v223-") as tmp:
    exe = Path(tmp) / ("coverage.exe" if os.name == "nt" else "coverage")
    cmd = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
           "-I" + str(SDK / "src"),
           str(ROOT / "tools/test_g925_zero_coverage_v223.cpp"),
           str(SDK / "src/graphics/gta4_native/native_performance_samples.cpp"),
           "-o", str(exe)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        cmd[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(cmd, check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
print("PASS: exact no-coverage draw elimination, query/diagnostic guards and GPU lifetimes")

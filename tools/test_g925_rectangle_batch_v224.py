#!/usr/bin/env python3
"""Validate production RectangleList Vulkan draw merge and exact strip replays."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "glue/rexglue-sdk-main"
src = (SDK / "src/graphics/gta4_native/graphics_system.cpp").read_text()
header = (SDK / "src/graphics/gta4_native/native_rectangle_batch.h").read_text()
profile = (SDK / "src/graphics/gta4_native/native_performance_samples.cpp").read_text()
policy = (SDK / "src/graphics/gta4_native/native_pipeline_policy.h").read_text()
assert '#include "native_rectangle_batch.h"' in src
assert "case 6:\n    case 8: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;" in policy
assert "return strip && (guest_restart || moltenvk);" in policy
assert "kNativeRectangleRestartIndex = UINT32_MAX" in header
assert "NativeRectangleStripIndexCount(uint32_t rectangles)" in header
assert "if (!required || indices.size() < required) return false;" in header
assert "rectangle_count - 1" in src
assert '"rectangle-draws-merged"' in profile
# The original per-rectangle fallback must remain intact on allocation miss.
primitive_up = src.split("bool Gta4NativeGraphicsSystem::RecordPrimitiveUp(", 1)[1]
primitive_up = primitive_up.split("bool Gta4NativeGraphicsSystem::RecordPrimitive(", 1)[0]
assert "bool merge_rectangles =" in primitive_up
assert "GetOrCreateDrawPipeline(" in primitive_up
assert "target, draw.stride, merge_rectangles" in primitive_up
assert "NativeRectangleStripIndexCount(rectangle_count)" in primitive_up
assert "WriteNativeRectangleStripIndices(" in primitive_up
assert "rectangle_list_indices.mapping" in primitive_up
assert "rectangle_list_index_count" in primitive_up
assert "vkCmdDrawIndexed(command_buffer, rectangle_list_index_count, 1, 0, 0, 0)" in primitive_up
assert "vkCmdDraw(command_buffer, 4, 1, rectangle * 4, 0)" in primitive_up
assert "kRectangleDrawsMerged" in primitive_up
assert "rectangle_list_index_count) {" in primitive_up
assert "primitive_restart_enable =" in src
# Both the full and sparse upload planners account for the additional index
# data, so no recurring mid-frame buffer growth is introduced.
assert src.count("NativeRectangleStripIndexCount(uint32_t(rectangle_count))") == 2
assert src.count("add_allocation(VkDeviceSize(index_count) * sizeof(uint32_t),") == 2

compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++20 host compiler is mandatory")
with tempfile.TemporaryDirectory(prefix="g925-rectangle-batch-") as folder:
    exe = Path(folder) / ("rectangle.exe" if os.name == "nt" else "rectangle")
    command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
               "-I" + str(SDK / "src"),
               str(ROOT / "tools/test_g925_rectangle_batch_v224.cpp"),
               "-o", str(exe)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        command[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(command, check=True)
    subprocess.run([str(exe)], check=True, timeout=45)
print("PASS: exact indexed RectangleList triangle/winding/order preservation and production integration")

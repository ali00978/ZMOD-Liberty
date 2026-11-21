#!/usr/bin/env python3
"""G925 Native 2.38: GPU draw-stage CPU fast path + lossless vertex replay."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sdk = root / "glue/rexglue-sdk-main"
resolve = (sdk / "src/graphics/gta4_native/native_resolve_descriptor_cache.h").read_text()
vertex = (sdk / "src/graphics/gta4_native/native_vertex_cache.h").read_text()
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()
for required in (
    "std::array<HotEntry, 4> hot_{};",
    "++statistics_.hot_hits;",
    "Forget(victim->first);",
    "for (auto& recent : hot_) recent.valid = false;",
):
    assert required in resolve, required
for required in (
    "G925CanQueueVertexOptimization(",
    "queued_count_.load(std::memory_order_acquire)",
    "queued_count_.store(jobs_.size(), std::memory_order_release);",
    "std::atomic<size_t> queued_count_{0};",
    "queued_bytes_.fetch_add(bytes, std::memory_order_release)",
    "queued_bytes_.fetch_sub(job.source.size(), std::memory_order_release)",
):
    assert required in vertex, required
# The 2.39 combined job-count/byte-count fast guard must run before copying.
assert vertex.index("queued_count_.load(std::memory_order_acquire)") < vertex.index("job.source.resize(")
assert vertex.index("queued_bytes_.load(std::memory_order_acquire)") < vertex.index("job.source.resize(")
assert "python tools/test_g925_fps_cache_v238.py" in workflow
compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++20 compiler is mandatory for G925 2.38 native FPS regression")
with tempfile.TemporaryDirectory(prefix="g925-fps-v238-") as folder:
    exe = Path(folder) / ("fps.exe" if os.name == "nt" else "fps")
    cmd = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
           "-I" + str(sdk / "src"),
           str(root / "tools/test_g925_fps_cache_v238.cpp"), "-o", str(exe)]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        cmd[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(cmd, check=True, timeout=90)
    subprocess.run([str(exe)], check=True, timeout=30)
print("PASS: G925 Native 2.38 4-way Vulkan fast hits, lifetime-safe eviction, and lossless mesh reordering")

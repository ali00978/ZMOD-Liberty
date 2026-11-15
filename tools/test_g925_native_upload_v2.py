#!/usr/bin/env python3
"""G925 Native v2: compile texture upload batching using real NDK Vulkan types."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sdk = root / "glue/rexglue-sdk-main"
header = (sdk / "src/graphics/gta4_native/native_texture_upload_batch.h").read_text()
for fragment in (
    "std::array<std::optional<Upload>, kCapacity> uploads_{};",
    "size_t count_ = 0;",
    "uploads_[count_++].emplace(std::move(upload));",
    "const std::span<const VkImageMemoryBarrier> span(barriers.data(), count_);",
    "emit_barriers(VK_PIPELINE_STAGE_TRANSFER_BIT,",
    "uploads_[i].reset();",
):
    assert fragment in header, fragment
assert "std::vector<Upload> uploads_" not in header
ndk = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT")
sdk_dir = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
candidates = []
if ndk:
    candidates.extend(Path(ndk).glob("toolchains/llvm/prebuilt/*/sysroot/usr/include"))
if sdk_dir:
    candidates.extend(Path(sdk_dir).glob("ndk/*/toolchains/llvm/prebuilt/*/sysroot/usr/include"))
candidates.append(Path("/usr/include"))
include = next((p for p in candidates if (p / "vulkan/vulkan_core.h").is_file()), None)
if include is None:
    raise SystemExit("Android NDK Vulkan headers required for real ABI regression")
compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("C++20 compiler required")
with tempfile.TemporaryDirectory(prefix="g925-native-upload-v2-") as temp:
    exe = Path(temp) / ("upload.exe" if os.name == "nt" else "upload")
    args = [
        compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I" + str(include), "-I" + str(sdk / "src"),
        str(root / "tools/test_g925_native_upload_v2.cpp"), "-o", str(exe),
    ]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        args[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(args, check=True, timeout=60)
    subprocess.run([str(exe)], check=True, timeout=45)
print("PASS: fixed-capacity Vulkan upload batch preserves exact barriers, 128-image order and 1000 frame replays")

#!/usr/bin/env python3
"""End-to-end static integration plus compiled, randomized GPU clear equivalence."""
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
tool=os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not tool: raise SystemExit("C++20 compiler required")
with tempfile.TemporaryDirectory(prefix="g925-gpu-clear-v231-") as tmp:
    out=Path(tmp)/("check.exe" if os.name=="nt" else "check")
    subprocess.run([tool,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
                    str(root/"tools/test_g925_gpu_clear_v231.cpp"),"-o",str(out)],check=True)
    subprocess.run([str(out)],check=True)
code=(root/"glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.cpp").read_text()
h=(root/"glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.h").read_text()
assert '#include "native_color_clear_fastpath.h"' in code
assert "REXCVAR_DEFINE_BOOL(gta4_native_fold_color_clears, true" in code
assert "REXCVAR_DEFINE_BOOL(gta4_native_skip_empty_clears, true" in code
assert "NativeClearRectHasNoPixels(" in code
assert code.index("NativeClearRectHasNoPixels(")<code.index("if (target.uses_presenter && !temporal_presenter_acquired_)")
assert "NativeFullColorClearMask(" in code
assert "color_attachments[index].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;" in code
assert "std::bit_cast<float>(color_clear.color_bits[component])" in code
assert "command_index == folded_color_clear_index ? folded_color_clear_mask : 0u" in code
assert "skip_color_mask & (uint32_t(1) << index)" in code
assert "uint32_t skip_color_mask = 0" in h
assert "vkCmdClearAttachments(" in code
assert "vkCmdBeginRendering(command_buffer, &rendering_info)" in code
gradle=(root/"os/android/app/build.gradle.kts").read_text()
# This test checks the v2.31 renderer functionality across later releases.
# Never pin its integration to the exact APK versionCode / versionSuffix.
version = re.search(r"versionCode\\s*=\\s*(\\d+)", gradle)
assert version and int(version.group(1)) >= 59
assert re.search(r'val versionSuffix\\s*=\\s*"-alpha-2\\.\\d+-g925-[^"]+"', gradle)
workflow=(root/".github/workflows/build-android-apk.yml").read_text()
assert "python tools/test_g925_gpu_clear_v231.py" in workflow
print("PASS: safe full-color loadOp clears and zero-area clear rejection")

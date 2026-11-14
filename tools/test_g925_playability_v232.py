#!/usr/bin/env python3
"""G925 2.32: compiled small-sparse batching + one-time latency policy."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
tool=os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not tool: raise SystemExit("C++20 compiler required")
with tempfile.TemporaryDirectory(prefix="g925-v232-") as tmp:
    binary=Path(tmp)/("check.exe" if os.name=="nt" else "check")
    subprocess.run([tool,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
                    str(root/"tools/test_g925_playability_v232.cpp"),
                    "-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=45)
sdk=root/"glue/rexglue-sdk-main"
renderer=(sdk/"src/graphics/gta4_native/graphics_system.cpp").read_text()
hook=(sdk/"gta4-recomp/src/android/android_frame_wait_hook.cpp").read_text()
activity=(root/"os/android/app/src/main/java/com/libertyrecomp/LibertyActivity.java").read_text()
launcher=(root/"os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
assert "const bool sparse_batch = sparse_indexed_range_count_ >= 3;" in renderer
assert "sparse_allowed && ShouldBatchNativeSparseIndexDraws(" in renderer
assert "sparse_count, merged_count, sparse_indices, sparse_has_gap" in renderer
assert "!command.fixed_function_state.blend_enable" in renderer
assert "!command.fixed_function_state.stencil_enable" in renderer
assert "translucent_query_index == UINT32_MAX" in renderer
assert re.search(r"REXCVAR_DEFINE_UINT32\(gta4_extra_frames_in_flight, 1,",hook)
assert "enableLowLatencyRunAheadOnce(dataRoot);" in activity
assert 'new File(dataRoot, ".g925_runahead1_v232")' in activity
assert 'new File(dataRoot, ".g925_runahead4_v228")' in activity
assert 'value.equals("--gta4_extra_frames_in_flight=4")' in activity
assert 'new String[] {"Default (1)", "0", "1", "2", "3", "4"}' in launcher
assert "python tools/test_g925_playability_v232.py" in (root/".github/workflows/build-android-apk.yml").read_text()
print("PASS: exact small-index batching and 1-frame latency policy")

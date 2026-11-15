#!/usr/bin/env python3
"""G925 v2.35: compile and replay exact role-aware color/depth surface lookups."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sdk = root / "glue/rexglue-sdk-main"
header = (sdk / "src/graphics/gta4_native/native_draw_lookup_cache.h").read_text()
checks = [
    "last_slot_for_role_[size_t(key.depth)]",
    "candidate.registry_revision == registry_revision",
    "candidate.key == key",
    "if (hot_order_[position] == role_slot)",
    "if (slot == role_slot) continue;",
    "last_slot_for_role_ = {kNoHotSlot, kNoHotSlot};",
    "Forget(entries_[start + victim]);",
]
for check in checks:
    assert check in header, check
compiler = os.environ.get("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++20 compiler is required")
with tempfile.TemporaryDirectory(prefix="g925-role-surface-") as temp:
    binary = Path(temp) / ("test.exe" if os.name == "nt" else "test")
    cmd = [
        compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I" + str(sdk / "src"),
        str(root / "tools/test_g925_role_aware_surface_v235.cpp"), "-o", str(binary),
    ]
    if os.environ.get("G925_TEST_SANITIZE") == "1":
        cmd[2:2] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(cmd, check=True)
    subprocess.run([str(binary)], check=True, timeout=45)
print("PASS: role-hot exact color/depth identity, 800k alternations, 120k churn, revision and eviction guards")

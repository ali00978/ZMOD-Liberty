#!/usr/bin/env python3
"""Mirror the APK host regression list, without an NDK or phone.

Read the build workflow's own test commands so adding a Windows APK check
cannot silently leave the Linux lab behind. Report every failing test in
one run; a failure still exits nonzero.
"""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
workflow = (ROOT / ".github/workflows/build-android-apk.yml").read_text()
block = workflow.split("      - name: Run C++ state replay and Java profile regressions", 1)[1]
block = block.split("\n      - name:", 1)[0]
tests = re.findall(r"^\s+python (tools/test_[A-Za-z0-9_]+\.py)(?:\s|$)", block, re.M)
if len(tests) < 30 or len(tests) != len(set(tests)):
    raise SystemExit("APK host suite missing, incomplete or duplicated")
failed = []
for test in tests:
    print(f"=== {test} ===", flush=True)
    try:
        result = subprocess.run([sys.executable, test], cwd=ROOT, timeout=240)
        if result.returncode:
            failed.append(test)
    except subprocess.TimeoutExpired:
        failed.append(test)
        print("Test timed out", flush=True)
print(f"Completed {len(tests)} APK host regressions; {len(failed)} failed", flush=True)
for test in failed:
    print(f"::error::Regression failed: {test}", flush=True)
raise SystemExit(bool(failed))

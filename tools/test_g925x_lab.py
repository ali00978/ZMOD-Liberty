#!/usr/bin/env python3
"""G925 X independent package + lossless device policy tests, offline/no phone needed."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
gradle = (ROOT / "os/android/app/build.gradle.kts").read_text()
launcher = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
activity = (ROOT / "os/android/app/src/main/java/com/libertyrecomp/LibertyActivity.java").read_text()
workflow = (ROOT / ".github/workflows/build-android-apk.yml").read_text()
profile = ROOT / "os/android/app/src/main/java/com/libertyrecomp/G925XDeviceProfile.java"
tests = ROOT / "tools/fixtures/g925/G925XDeviceProfileTest.java"

for needle in (
    'val g925XEdition = libertyEdition == "g925x"',
    '"com.zmodliberty.g925x"',
    '"ZMOD G925 X Lab"',
    'versionCode   = 72',
    'if (g925XEdition) versionCode = 75',
    'buildConfigField("boolean", "G925_X_EDITION", g925XEdition.toString())',
):
    assert needle in gradle, needle
assert 'G925XDeviceProfile.describe(' in launcher
assert 'G925NativeDefaults.apply(arguments, BuildConfig.G925_X_EDITION)' in activity
assert workflow.count("Package independent G925 X APK") == 2, "Both CI paths must package X"
assert workflow.count("Upload independent G925 X APK") == 2, "Both CI paths must publish X"
assert workflow.count("LIBERTY_EDITION=g925x bash") == 2
assert 'ZMOD-G925-X-Lab-v0.5.0-armv9.apk' in workflow
assert "package: name='com.zmodliberty.g925x'" in workflow

javac = os.getenv("G925_TEST_JAVAC") or shutil.which("javac")
java = shutil.which("java")
if not javac or not java:
    raise RuntimeError("Java 17 JDK required to run hardware recognition tests")
with tempfile.TemporaryDirectory(prefix="g925x-device-") as tmp:
    subprocess.run([javac, "-encoding", "UTF-8", "-d", tmp,
                    str(profile), str(tests)], check=True, timeout=60)
    subprocess.run([java, "-cp", tmp, "com.libertyrecomp.G925XDeviceProfileTest"],
                   check=True, timeout=60)

if os.getenv("G925X_BENCHMARK") == "1":
    cxx = os.getenv("G925_TEST_CXX") or shutil.which("clang++") or shutil.which("g++")
    assert cxx, "C++20 compiler required"
    with tempfile.TemporaryDirectory(prefix="g925x-render-") as tmp:
        executable = Path(tmp) / ("g925x_bench.exe" if os.name == "nt" else "g925x_bench")
        subprocess.run([cxx, "-std=c++20", "-O2", "-Wall", "-Wextra",
                        str(ROOT / "tools/bench_g925x_rebased_index.cpp"),
                        "-o", str(executable)], check=True, timeout=60)
        subprocess.run([str(executable)], check=True, timeout=60)
print("PASS: G925 X separate identity, safe target detection, native benchmark wiring")

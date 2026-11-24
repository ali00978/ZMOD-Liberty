#!/usr/bin/env python3
"""G925 Native v1: packaging identity, engine-reuse and Java startup correctness."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
gradle = (root / "os/android/app/build.gradle.kts").read_text(encoding="utf-8")
manifest = (root / "os/android/app/src/main/AndroidManifest.xml").read_text(encoding="utf-8")
launcher = (root / "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text(encoding="utf-8")
activity = (root / "os/android/app/src/main/java/com/libertyrecomp/LibertyActivity.java").read_text(encoding="utf-8")
native_defaults = root / "os/android/app/src/main/java/com/libertyrecomp/G925NativeDefaults.java"
workflow = (root / ".github/workflows/build-android-apk.yml").read_text(encoding="utf-8")
script = (root / "os/android/scripts/build_apk.sh").read_text(encoding="utf-8")

for fragment in (
    'applicationId = if (g925NativeEdition) "com.zmodliberty.g925native" else "com.libertyrecomp"',
    '"-PlibertyEdition must be',
    'val g925NativeEdition = libertyEdition == "g925native"',
    'buildConfigField("boolean", "G925_NATIVE_EDITION", g925NativeEdition.toString())',
    'manifestPlaceholders["libertyAppLabel"]',
    '"ZMOD Liberty G925 Native" else "Liberty Recompiled"',
):
    assert fragment in gradle, fragment
assert 'android:label="${libertyAppLabel}"' in manifest
assert 'BuildConfig.G925_NATIVE_EDITION ? "G925 Native" : "Mali Edition"' in launcher
assert 'G925NativeDefaults.apply(arguments, BuildConfig.G925_NATIVE_EDITION)' in activity
assert 'LIBERTY_EDITION:-classic' in script
assert 'LIBERTY_EDITION=g925native bash os/android/scripts/build_apk.sh debug' in workflow
assert workflow.count("Upload independent G925 Native APK") == 2
assert workflow.count("Test standalone G925 Native package and startup defaults") == 2
assert "package: name='com.zmodliberty.g925native'" in workflow
assert 'ZMOD-Liberty-G925-Native-v3.5-armv9.apk' in workflow
assert 'LIBERTY_EDITION=g925native' in workflow
for forbidden in ("gta4_native_upscaler", "gta4_native_shadow_interval",
                  "gta4_draw_distance_scale", "android_gpu_turbo"):
    assert forbidden not in native_defaults.read_text(encoding="utf-8"), forbidden

javac = os.environ.get("G925_TEST_JAVAC") or shutil.which("javac")
java = shutil.which("java")
if not javac or not java:
    raise SystemExit("A Java 17 JDK is required to verify independent G925 Native APK defaults")
with tempfile.TemporaryDirectory(prefix="g925-native-v1-") as td:
    subprocess.run([javac, "-encoding", "UTF-8", "-d", td,
                    str(native_defaults),
                    str(root / "tools/fixtures/g925/G925NativeDefaultsTest.java")],
                   check=True, timeout=50)
    subprocess.run([java, "-cp", td, "com.libertyrecomp.G925NativeDefaultsTest"],
                   check=True, timeout=50)
print("PASS: G925 standalone ID, dual APK upload and lossless 16 ms ADPF defaults")

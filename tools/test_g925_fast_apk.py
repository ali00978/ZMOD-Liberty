#!/usr/bin/env python3
"""Offline tests for fail-closed fast build validation and APK staging."""
from __future__ import annotations

import importlib.util
import tempfile
from pathlib import Path
from unittest.mock import patch
from zipfile import ZipFile


def load(path: Path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


root = Path(__file__).resolve().parent
validate = load(root / "validate_g925_fast_apk.py")
stage = load(root / "stage_g925_apk_native.py")

assert validate.changed_file_is_launcher_only(
    "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java")
assert validate.changed_file_is_launcher_only("os/android/app/build.gradle.kts")
assert validate.changed_file_is_launcher_only(".github/workflows/build-android-apk.yml")
assert not validate.changed_file_is_launcher_only(
    "glue/rexglue-sdk-main/src/graphics/gta4_native/native_frame_scheduling.h")
assert not validate.changed_file_is_launcher_only(
    "os/android/app/src/main/jniLibs/arm64-v8a/libmain.so")
assert not validate.changed_file_is_launcher_only(
    "os/android/app/src/main/assets/Resources/secret.bin")
assert not validate.changed_file_is_launcher_only("os/android/scripts/build_native.sh")

base_sha, head_sha = "a" * 40, "b" * 40
run = {
    "status": "completed", "conclusion": "success", "name": "Android APK",
    "repository": {"full_name": "ali00978/ZMOD-Liberty"}, "head_sha": base_sha,
}
artifacts = {"artifacts": [{
    "name": "LibertyRecompAndroid-0.5.9.3-mali-alpha-1.5-armv9.apk",
    "size_in_bytes": 40_000_000, "expired": False,
}]}
allowed = {"status": "ahead", "files": [
    {"filename": "os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java"},
    {"filename": "os/android/app/build.gradle.kts"},
]}


def fake_api(path: str, *, token: str, repo: str):
    assert token == "test"
    assert repo == "ali00978/ZMOD-Liberty"
    if path.startswith("actions/runs/") and path.endswith("/artifacts?per_page=100"):
        return artifacts
    if path.startswith("actions/runs/"):
        return run
    if path.startswith("compare/"):
        return allowed
    raise AssertionError(path)


with patch.object(validate, "github_api", side_effect=fake_api):
    assert validate.validate(12345, "ali00978/ZMOD-Liberty", head_sha,
                             "mali", "armv9", "test") == artifacts["artifacts"][0]
    with patch.dict(allowed, {"files": [
        {"filename": "glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.cpp"}
    ]}):
        try:
            validate.validate(12345, "ali00978/ZMOD-Liberty", head_sha,
                              "mali", "armv9", "test")
            raise AssertionError("Native code change was incorrectly allowed")
        except validate.UnsafeFastBuild:
            pass
    with patch.dict(run, {"conclusion": "failure"}):
        try:
            validate.validate(12345, "ali00978/ZMOD-Liberty", head_sha,
                              "mali", "armv9", "test")
            raise AssertionError("Failed base run was incorrectly allowed")
        except validate.UnsafeFastBuild:
            pass

with tempfile.TemporaryDirectory() as folder:
    root_path = Path(folder)
    apk = root_path / "baseline.apk"
    with ZipFile(apk, "w") as archive:
        for lib in ("libmain.so", "librexruntime.so", "libSDL3.so",
                    "libc++_shared.so", "libvulkan.so", "librexgpu-gta4-native.so"):
            archive.writestr(f"lib/arm64-v8a/{lib}", "fake-so")
        archive.writestr("assets/Resources/font_atlases/test.dat", "font")
        archive.writestr("assets/Resources/button_prompts/test.dat", "prompt")
    staged_libs, staged_resources = stage.stage(apk, root_path / "os/android")
    assert staged_libs == 6
    assert staged_resources == 2
    assert (root_path / "os/android/app/src/main/jniLibs/arm64-v8a/libmain.so").read_text() == "fake-so"
    assert (root_path / "os/android/app/src/main/assets/Resources/font_atlases/test.dat").read_text() == "font"

print("PASS: fast APK preflight rejects native changes and APK staging preserves libs/resources")

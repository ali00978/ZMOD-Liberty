#!/usr/bin/env python3
"""Stage reusable native code from a previously successful, validated APK.

This does NOT compile any native code. Safe only when
validate_g925_fast_apk.py approved the source diff against the APK run SHA.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import sys
import zipfile


def stage(apk: Path, app_root: Path) -> tuple[int, int]:
    libs_dir = app_root / "app/src/main/jniLibs/arm64-v8a"
    assets_dir = app_root / "app/src/main/assets/Resources"
    if not apk.is_file():
        raise RuntimeError(f"APK not found: {apk}")
    libraries = {}
    resources = {}
    with zipfile.ZipFile(apk) as archive:
        for member in archive.infolist():
            name = member.filename
            if member.is_dir():
                continue
            if name.startswith("lib/arm64-v8a/") and name.endswith(".so"):
                base = name.rsplit("/", 1)[-1]
                if not re.fullmatch(r"lib[A-Za-z0-9_+.-]+\.so", base):
                    raise RuntimeError(f"Invalid native library path {name}")
                libraries[base] = member
            elif name.startswith("assets/Resources/"):
                relative = name.removeprefix("assets/Resources/")
                if not relative or ".." in Path(relative).parts or "\\" in relative:
                    raise RuntimeError(f"Unsafe assets path: {name}")
                resources[relative] = member

        required = {"libmain.so", "librexruntime.so", "libSDL3.so",
                    "libc++_shared.so", "libvulkan.so"}
        missing = required - libraries.keys()
        if missing:
            raise RuntimeError("APK lacks native game libraries: " + ", ".join(sorted(missing)))
        if not any(name.startswith("librexgpu-") for name in libraries):
            raise RuntimeError("APK is missing renderer plugin")
        if not resources:
            raise RuntimeError("APK has no packaged game resources")

        libs_dir.mkdir(parents=True, exist_ok=True)
        assets_dir.mkdir(parents=True, exist_ok=True)
        # Fresh GitHub checkout has no staged native libs/resources.
        for path in libs_dir.glob("*.so"):
            path.unlink()
        if assets_dir.exists():
            shutil.rmtree(assets_dir)
        assets_dir.mkdir(parents=True, exist_ok=True)
        for base, member in libraries.items():
            with archive.open(member) as source, (libs_dir / base).open("wb") as output:
                shutil.copyfileobj(source, output, length=1024 * 1024)
        for relative, member in resources.items():
            target = assets_dir / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(member) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output, length=1024 * 1024)

    print(f"Staged {len(libraries)} native .so files and {len(resources)} resources")
    print("Reusing existing native game code; packaging only Java + assets.")
    return len(libraries), len(resources)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifact-directory", required=True, type=Path)
    parser.add_argument("--app-directory", required=True, type=Path)
    args = parser.parse_args()
    apks = list(args.artifact_directory.glob("*.apk"))
    if len(apks) != 1:
        raise SystemExit(f"Expected exactly one APK artifact, found {len(apks)}")
    try:
        stage(apks[0], args.app_directory)
        return 0
    except (OSError, RuntimeError, zipfile.BadZipFile) as exc:
        print(f"Cannot stage baseline APK: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Guard a fast Java-only APK build against stale native libraries.

The baseline is an already successful Android APK workflow run of THIS fork.
We compare its exact source commit to GITHUB_SHA using the GitHub REST API,
rejecting every change outside the explicitly allowed launcher/app paths.
Never run this script with an untrusted artifact ID from a different repo.
"""
from __future__ import annotations

import json
import os
import re
import sys
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, urlopen


class UnsafeFastBuild(RuntimeError):
    pass


def github_api(path: str, *, token: str, repo: str) -> dict:
    url = f"https://api.github.com/repos/{repo}/{path}"
    request = Request(url, headers={
        "Accept": "application/vnd.github+json",
        "Authorization": f"Bearer {token}",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "G925-fast-apk-validation",
    })
    try:
        with urlopen(request, timeout=45) as response:
            return json.load(response)
    except HTTPError as exc:
        raise UnsafeFastBuild(f"GitHub returned HTTP {exc.code} for {path}") from exc


def changed_file_is_launcher_only(path: str) -> bool:
    """Allow packaging and launcher edits only; game engine/glue never."""
    safe_app_paths = (
        "os/android/app/src/main/java/",
        "os/android/app/src/main/res/",
        "os/android/app/src/main/assets/launcher/",
    )
    safe_exact = {
        "os/android/app/build.gradle.kts",
        "os/android/app/src/main/AndroidManifest.xml",
        "os/android/app/src/main/assets/default_args.txt",
        ".github/workflows/build-android-apk.yml",
    }
    if path in safe_exact or any(path.startswith(p) for p in safe_app_paths):
        return True
    if path.startswith("docs/") or path in {"README.md", "LICENSE", ".gitignore"}:
        return True
    if path.startswith("tools/test_g925_") and path.endswith(".py"):
        return True
    if path in {"tools/validate_g925_fast_apk.py", "tools/stage_g925_apk_native.py",
                "tools/test_g925_fast_apk.py"}:
        return True
    return False


def validate(run_id: int, repo: str, target: str, gpu: str, cpu: str, token: str) -> dict:
    run = github_api(f"actions/runs/{run_id}", token=token, repo=repo)
    if run.get("status") != "completed" or run.get("conclusion") != "success":
        raise UnsafeFastBuild(f"Base run {run_id} is not a successfully completed workflow")
    if run.get("repository", {}).get("full_name") != repo:
        raise UnsafeFastBuild("Base run must belong to this repository")
    if run.get("name") != "Android APK":
        raise UnsafeFastBuild("Base must be a completed Android APK workflow")
    base = run.get("head_sha", "")
    if not re.fullmatch(r"[a-f0-9]{40}", base):
        raise UnsafeFastBuild("Base run has no valid source SHA")
    if not re.fullmatch(r"[a-f0-9]{40}", target):
        raise UnsafeFastBuild("Target has no valid source SHA")
    if gpu != "mali" or cpu != "armv9":
        raise UnsafeFastBuild("Fast mode currently supports only mali+armv9 builds")

    artifacts = github_api(f"actions/runs/{run_id}/artifacts?per_page=100",
                           token=token, repo=repo).get("artifacts", [])
    found = [a for a in artifacts if a.get("name", "").startswith("LibertyRecompAndroid-")
             and "-mali-" in a.get("name", "")
             and "-armv9.apk" in a.get("name", "")
             and not a.get("expired")
             and a.get("size_in_bytes", 0) > 0]
    if len(found) != 1:
        raise UnsafeFastBuild(
            f"Expected exactly one unexpired Mali ARMv9 APK from base run, got {len(found)}")

    if base != target:
        compare = github_api(
            f"compare/{quote(base)}...{quote(target)}?per_page=100",
            token=token, repo=repo)
        if compare.get("status") != "ahead":
            raise UnsafeFastBuild(
                f"Base run source is not an ancestor of this commit: {compare.get('status')}")
        files = compare.get("files", [])
        if len(files) >= 300:
            raise UnsafeFastBuild("GitHub comparison is too large for safe fast mode")
        disallowed = [f.get("filename", "?") for f in files
                      if not changed_file_is_launcher_only(f.get("filename", ""))]
        if disallowed:
            raise UnsafeFastBuild(
                "Native or unknown files changed since base APK. Use FULL mode. "
                "Examples: " + ", ".join(disallowed[:8]))

    print(f"PASS: base run {run_id} at {base[:12]} is successful and compatible")
    print("PASS: source differences require only launcher/Java APK packaging")
    print(f"PASS: compatible artifact {found[0]['name']}")
    return found[0]


def main() -> int:
    try:
        run_text = os.environ.get("G925_BASE_RUN_ID", "")
        if not re.fullmatch(r"[0-9]{5,20}", run_text):
            raise UnsafeFastBuild(
                "Set a successful baseline run ID in the 'base_run_id' input")
        repo = os.environ.get("GITHUB_REPOSITORY", "")
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo):
            raise UnsafeFastBuild("Invalid repository")
        token = os.environ.get("GH_TOKEN", "")
        if not token:
            raise UnsafeFastBuild("Missing Actions-read GitHub token")
        validate(int(run_text), repo, os.environ.get("GITHUB_SHA", ""),
                 os.environ.get("LIBERTY_GPU", ""), os.environ.get("LIBERTY_CPU", ""), token)
        return 0
    except UnsafeFastBuild as exc:
        print(f"FAST BUILD REJECTED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

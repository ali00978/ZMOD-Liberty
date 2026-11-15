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
from urllib.error import HTTPError, URLError
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
    except (URLError, TimeoutError, ValueError) as exc:
        raise UnsafeFastBuild(f"Could not verify GitHub response for {path}") from exc


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
        "tools/fixtures/g925/G925ProfileReportTest.java",
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
        # A rename out of a native path still changes native sources. Check both
        # sides instead of accepting only the new launcher-looking filename.
        disallowed = [path for f in files
                      for path in [f.get("filename", ""), f.get("previous_filename")]
                      if path is not None and not changed_file_is_launcher_only(path)]
        if disallowed:
            raise UnsafeFastBuild(
                "Native or unknown files changed since base APK. Use FULL mode. "
                "Examples: " + ", ".join(disallowed[:8]))

    print(f"PASS: base run {run_id} at {base[:12]} is successful and compatible")
    print("PASS: source differences require only launcher/Java APK packaging")
    print(f"PASS: compatible artifact {found[0]['name']}")
    return found[0]


def select_build_mode(event: str, requested_mode: str, requested_base: str,
                      repo: str, target: str, branch: str, gpu: str, cpu: str,
                      token: str) -> tuple[str, str]:
    """Use a same-branch APK only after the existing source/artifact guard passes."""
    if event == "workflow_dispatch":
        if requested_mode == "java-only":
            if not re.fullmatch(r"[0-9]{5,20}", requested_base):
                raise UnsafeFastBuild("Java-only mode requires an explicit base run ID")
            return "java-only", requested_base
        return "full", ""
    if event != "pull_request" or gpu != "mali" or cpu != "armv9" or not branch:
        return "full", ""
    try:
        runs = github_api(
            "actions/workflows/build-android-apk.yml/runs?status=success&per_page=3&branch="
            + quote(branch, safe=""), token=token, repo=repo).get("workflow_runs", [])
    except UnsafeFastBuild as exc:
        print(f"Automatic native reuse unavailable; selecting full build: {exc}")
        return "full", ""
    for run in runs[:3]:
        run_id = str(run.get("id", ""))
        if run.get("head_branch") != branch or not re.fullmatch(r"[0-9]{5,20}", run_id):
            continue
        try:
            validate(int(run_id), repo, target, gpu, cpu, token)
        except UnsafeFastBuild as exc:
            print(f"Base run {run_id} cannot be reused: {exc}")
            continue
        return "java-only", run_id
    return "full", ""


def main() -> int:
    try:
        repo = os.environ.get("GITHUB_REPOSITORY", "")
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo):
            raise UnsafeFastBuild("Invalid repository")
        token = os.environ.get("GH_TOKEN", "")
        if not token:
            raise UnsafeFastBuild("Missing Actions-read GitHub token")
        target = os.environ.get("G925_TARGET_SHA") or os.environ.get("GITHUB_SHA", "")
        if not re.fullmatch(r"[a-f0-9]{40}", target):
            raise UnsafeFastBuild("Target has no valid source SHA")
        run_text = os.environ.get("G925_BASE_RUN_ID", "")
        if sys.argv[1:] == ["--select-mode"]:
            mode, base = select_build_mode(
                os.environ.get("GITHUB_EVENT_NAME", ""),
                os.environ.get("G925_REQUESTED_MODE", "full"), run_text,
                repo, target, os.environ.get("G925_HEAD_BRANCH", ""),
                os.environ.get("LIBERTY_GPU", ""), os.environ.get("LIBERTY_CPU", ""), token)
            with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
                output.write(f"build_mode={mode}\nbase_run_id={base}\n")
            print(f"Selected {mode}; compatible base run: {base or 'none'}")
            return 0
        if not re.fullmatch(r"[0-9]{5,20}", run_text):
            raise UnsafeFastBuild(
                "Set a successful baseline run ID in the 'base_run_id' input")
        validate(int(run_text), repo, target,
                 os.environ.get("LIBERTY_GPU", ""), os.environ.get("LIBERTY_CPU", ""), token)
        return 0
    except UnsafeFastBuild as exc:
        print(f"FAST BUILD REJECTED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

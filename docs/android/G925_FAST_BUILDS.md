# Faster G925 APK builds

**Implemented in source branch:** `feature/g925-fast-build-workflow`.

The previous G925 V1, V2 and V3 builds each ran on a different feature
branch. GitHub Actions `actions/cache` caches are scoped by branch; the
actual logs showed **Cache not found**, followed by compilation of ~1,056
native targets. Caching a Ninja build directory does not by itself protect
against changed checkout file timestamps. The new setup addresses both
problems, and adds a fast mode for Java-only changes.

## 1. Full native build (C++/renderer changes)

The `Android APK` workflow retains its existing **full** mode, building and
checking the full ARMv9 Mali project from source. Additionally:

- Installs `mozilla-actions/sccache-action@v0.0.11`.
- Enables the GitHub Actions cache backend and reports compiler cache stats.
- Configures CMake's C and C++ compiler launchers to use sccache.
- Detects launcher/CPU target changes and reconfigures when needed.
- Preserves the ordinary Ninja build-dir cache and native build validations.
- Falls back to ordinary compilation on cache misses.

**First full build using sccache is a cold cache and may still take a long
time.** Future full builds on the same branch can reuse successful C/C++
compilation results. Changes to generated C++/headers/toolchains will still
require recompilation, and cache quotas or GitHub storage restrictions may
reduce the benefit.

## 2. Fast APK mode (Java/UI/packaging only)

The workflow has two manual build modes:

- `full` (default): full native compile, for renderer/C++/driver changes.
- `java-only`: reuses the packaged native libraries and resource files from
  an already completed and successful Android APK run of *this repository*,
  then builds a new APK containing any approved Java/UI/Gradle changes.
  No native compilation is performed.

To use the fast mode safely:

1. Open the repository's **Actions → Android APK → Run workflow**.
2. Choose the **same long-lived G925 development branch** where your changes
   are saved (avoid creating a new branch for every minor revision).
3. Set `build_mode` to `java-only`, `gpu` to `mali`, `cpu` to `armv9`.
4. Supply `base_run_id`: the numeric run ID of the **most recent successfully
   completed full Android APK build from the same branch**, or an ancestor run
   without native differences.
5. Run the workflow and download the resulting APK from **Artifacts**.

The verifier asks GitHub for the baseline's source commit and artifact, checks
that it is an unexpired Mali ARMv9 APK and compares changes between source
commits. If *any native code, driver script, renderer, build system or other
unrecognized path has changed*, it fails immediately with **FULL mode required**.
It cannot accidentally package stale native code into a game update.

The APK contains no GTA IV game assets beyond the LibertyRecomp app's own
required, legally redistributable support resources. Saved games and installed
content are *not* repackaged.

**Important:** This acceleration branch also changes
`os/android/scripts/build_native.sh` to enable sccache. Therefore a full
native build **must succeed on this branch first** before java-only mode can
use that new build as its source baseline. A V3 artifact built before this
native-script change is deliberately rejected as the fast-mode baseline.

This initial full build is **not launched automatically** from this branch.
The separate `G925 Fast Build Checks` job runs only quick offline tests and
does not compile the GTA IV engine.

## 3. Work efficiently

- Keep a single G925 development branch and batch C++ changes.
- For changes limited to launcher code and Android resources, use java-only
  after a compatible full APK build has been successfully published.
- For C++, Vulkan, texture caches, CPU hints or NDK toolchains, always use full.
- Do not run the V2 and V3 native builds in parallel unless actually needed.
- Check cache logs for **sccache hit rate**, not just a completed Actions cache
  restore message.

No fixed build time is guaranteed. Faster builds depend on code changes,
cache hit rates and hosted-runner availability.

## Offline validation

```sh
python tools/test_g925_fast_apk.py
bash -n os/android/scripts/build_native.sh
```

Additionally, GitHub Actions runs the lightweight
`.github/workflows/g925-fast-build-checks.yml`. Passing these checks
validates the fast-mode implementation's *local control flow*, not an actual
APK package operation or end-to-end driver compatibility.

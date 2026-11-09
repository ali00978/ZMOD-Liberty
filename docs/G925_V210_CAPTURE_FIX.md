# G925 2.10: capture identity and partial CPU detail

Version code 38, `0.5.9.3-mali-alpha-2.10-g925-capture-fix`.
This updates the Android capture/report path. The verified 2.9 native engine,
its optimizations and the rendering settings are retained.

## Findings from the supplied captures

The first report labeled 2.9 repeated every timing and slow-frame identity from
the earlier 2.7 report. The corrected 2.9 capture contains new values and the
expected native counters: 180 samples, 35.00 ms mean title interval, 20.73 ms
command recording and 17.99 ms GPU time. Surface lookup reuse averaged 4383.80,
vertex layout reuse 543.58 and skipped original index uploads 384.14 per frame.
Converted index upload reuse was zero in this capture. These are profiling
observations, not a controlled normal-FPS comparison.

Two report issues prevented reliable follow-up:

- The launcher retained the capture start preference across app upgrades and
  printed the installed version over an older CSV. Its final-settings snapshot
  identified the launch time but not the app version.
- `PublishFrame` decides whether to start its CPU recorder before
  `BeginNativeGpuProfileFrame` services the delayed capture trigger. The first
  sampled frame can therefore lack CPU detail. The Java reader required detail
  for every captured frame and discarded all otherwise valid detail rows.

## Resulting behavior

- A capture begins by clearing only the prior primary CSV, CPU-detail CSV and
  applied-settings snapshot, after stopping the previous game process. Failure
  to prepare or persist the launch identity aborts that capture launch.
- The game process atomically records both its launch token and app version with
  the final applied settings. Sharing requires both to match; old/missing build
  identity cannot fall back to the launcher's saved settings. The report shows
  the capture start in UTC.
- Valid CPU-detail frames are reported with explicit coverage, such as
  `n=179/180`. Their average uses 179, excluding missing frames rather than
  counting them as zero. Invalid or duplicate matching rows still reject CPU
  detail; unrelated frame/sequence pairs do not enter the averages.

## Validation and build

The production Java reader passed separate-process tests for upgrade/legacy
snapshot rejection, timestamp handling, fresh-capture cleanup and failure,
new-file completion, partial coverage and its denominator, invalid/duplicate
rows, bounds, settings precedence and privacy. All nine existing APK checks
passed. `git diff --check` passed.

The build guard must verify that changes since successful 2.9 run
`41711808812` are packaging/Java only before reusing its native libraries.
Android packaging is checked by GitHub Actions. No phone-side result for 2.10
or additional FPS improvement is claimed.

After installing, run **CAPTURE CPU + GPU BOTTLENECK**, play for at least 85
seconds, return to the launcher and share the new report. The CPU operation
section is the input for choosing the next renderer optimization.

Rollback: [verified 2.9 APK](https://github.com/ali00978/ZMOD-Liberty/actions/runs/41711808812).

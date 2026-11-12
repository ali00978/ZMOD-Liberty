# G925 v2.30 — remove redundant diagnostic hashing from the normal render hotpath

Base: v2.29, successful full APK run 43184078943.

Earlier v2.28 user capture: ~3,502 Vulkan draws/frame, 23.26 ms native
CPU command recording, 20.34 ms GPU frame. Both exceed 16.67 ms for 60 FPS.

Native command producer capture and render-worker assembly had each serialized
and XXH3-hashed the full fixed-function state on **every** draw, despite these
hashes being used only for integrity verification and light-trace diagnostics.
Recorder-side re-hashing of sampled commands added another pass. New guard:
when native trace/probes, hot-cache validation and GPU breadcrumbs are disabled,
do not calculate those hashes, and do not validate a missing diagnostic hash.

When diagnostics are explicitly enabled, full producer/worker captures remain
available and the 2.29 verification policy still runs. The complete draw state,
shader selection, geometry, texture sampling and Vulkan commands are unchanged.
Native periodic renderer tracing now defaults off, but can be enabled for
troubleshooting.

Mandatory C++ policy test and integration test are in Android APK workflow,
beside all earlier native and Java regressions.

A successful build does not establish FPS gains. This is a targeted CPU fix,
not a claim that the GPU's previously measured ~20 ms cost is eliminated.

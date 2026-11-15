# G925 2.8: native surface and vertex lookup reuse

The user's 180-sample 2.7 capture did not establish a performance improvement:
command recording averaged 22.58 ms, GPU 20.94 ms and title intervals 39.26 ms
(2.6: 22.52 / 20.89 / 38.59 ms). Both captures include profiler overhead.
Pipeline compilation and fence waits were zero. There is no measured 2.8 phone
result yet and no FPS gain is claimed.

## Changes

- Repeated successful surface lookups reuse a bounded 64-entry recorder cache.
  This removes repeated shared texture-registry locking, metadata decoding,
  capability checks and surface bucket searches for the same exact request.
  Keys include every descriptor field, depth/color and requested sample count.
- Producer registry registration, erasure and clearing publish a revision under
  the existing mutex. Entries are usable only for that revision. Reflection
  registration/removal, pending surface release, destruction and renderer reset
  invalidate borrowed pointers. Misses and allocation failures retain the old
  path. Hits still update submission lifetime and GPU flight tracking; image
  layout, placement ownership and content materialization remain authoritative.
- Vertex stream selection is memoized by shader/declaration identities and
  signatures, including when pipeline snapshots are copied. Native draw binding
  iterates only set stream bits. Invalid input layouts remain failures.
- The shared report includes surface/vertex cache hit counts and the eight most
  expensive CPU operations from the detailed capture that was already enabled.
  Self times exclude nested scopes. Detail must match every captured
  frame/sequence; absent, malformed, duplicate or stale detail is unavailable,
  never zero. This introduces no additional profiling during normal play.

Version code 36, `0.5.9.3-mali-alpha-2.8-g925-draw-lookup`. Native Vulkan,
existing quality settings, saved options, 16 ms feedback and Stable policy
are preserved. No default resolution, texture, AA, draw distance or refresh
frequency has been changed. The existing automatic reductions remain as
configured by the user; 2.8 does not add new ones.

## Validation

- Executable C++ regression: all exact key fields, collisions, revision and
  release invalidation, 20,000 lifetime transitions, copied vertex-state memos,
  every one of 131,072 possible 17-stream masks and actual CSV counter names.
- Address/undefined-behavior sanitizer run passed with leak detection disabled
  because this host cannot inspect `/proc` for LeakSanitizer.
- Existing C++ dynamic-state/index/ADPF replay and nine APK regression scripts.
- Actual Java report class compiled and executed: separate-process launch
  snapshots, CSV statistics and matching CPU detail; invalid/missing/stale data
  does not invalidate a good top-level report or produce false zero timings.
- Full ARMv9 native APK compilation and packaging passed. Java-only native
  reuse was rejected for these changes. Phone-side rendering and performance
  remain unverified.

Rollback: 2.7 delivery commit `81cdd89b7183f430283a8119dc9d66de7efa5898`,
GitHub Actions run `41659074125`. Keep the 2.6 baseline branch unchanged.

## Verified delivery

- Source commit: `e318554795a9fa48c8451803b9423e083240078d`.
- [Successful full native build and APK](https://github.com/ali00978/ZMOD-Liberty/actions/runs/41666819194).
- Artifact `13249300498`: `LibertyRecompAndroid-0.5.9.3-mali-alpha-2.8-g925-draw-lookup-armv9.apk`.
- GitHub artifact ZIP SHA-256: `637275f3e22987d3b27aea72125570d6dde8f8c4bc3afd4de4927930ebf00ac0`.
- APK size: 49,484,917 bytes; SHA-256: `2744227011495c898d50bd1a6f4ad5aa6d1020fa6f220e68e11c269a5ea2b71f`.
- APK v2 cryptographic signature and content digest verified. Package remains
  `com.libertyrecomp`, with version code 36 and the same certificate as 2.6/2.7.
- New native renderer binary and CPU-detail report markers are present. All
  11 ARM64 native library names, 472 packaged resources and default arguments
  were checked against the prior verified APK; quality assets/defaults match.
- Compiler cache: 1,002 hits, 11 misses (98.91% hit rate). This is a build statistic,
  not a phone performance measurement.

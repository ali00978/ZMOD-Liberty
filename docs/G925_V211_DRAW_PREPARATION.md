# G925 2.11: combined native draw preparation improvements

Version code 39, `0.5.9.3-mali-alpha-2.11-g925-draw-prep`.

The corrected 2.9 capture reported 20.73 ms mean command recording, 17.99 ms
GPU time and a 35.00 ms title interval. This bundle targets repeated CPU work
in the native draw path. Those overlapping ranges must not be added together.
There is no measured phone-side speedup for this version yet.

## Changes in one build

1. Reuse the derived viewport and scissor, including resolution scales, when
   all guest inputs, target dimensions, device limits and debug coverage match.
   Float keys compare their bits, preserving signed zero and NaN payloads.
   The existing command-state cache still emits required Vulkan state after
   recording resets or external rendering. No draw or clipping rule changes.
2. Reuse the previous shared constant allocation after the exact semantic key
   resolves to the same immutable identity. This avoids another arena lookup.
   Each frame slot owns its memo; arena reset, storage replacement and teardown
   invalidate it. Failed uploads are not memoized. The existing shared-key
   diagnostic switch also bypasses allocation reuse.
3. Retain push-constant addresses across native pipeline binds when layout
   and all three addresses are identical. New command buffers, external paths,
   temporal draws and layout/address changes still update them. Vulkan pipeline
   binds do not erase push constant values; see the
   [Khronos lifetime documentation](https://docs.vulkan.org/guide/latest/push_constants.html#_binding_non_compatible_pipelines).
4. Return before diagnostic texture/mipmap and surface walks when the flight
   recorder is disabled. Actual frame/submission resource lifetime tracking
   remains unconditional. Enabled diagnostics retain their existing behavior.
5. Skip four render-target component-mask calculations when native constant
   alpha blend factors are supported. The portability emulation and rejection
   rules remain available and unchanged.

The earlier surface, vertex-layout and index-upload optimizations and the 2.10
capture/report fixes are included. Graphics settings, assets, draw distance,
reflection/shadow policy, frame cap, native renderer and CPU hint policy are
unchanged. This does not lower image quality to obtain a higher reported FPS.

## Validation

- 200,000 raster replays against the former calculations, including changing
  targets/limits, zero dimensions, clipping, invalid floats and debug coverage.
  Ten repeats per input perform one calculation with identical results.
- 40,000 blend comparisons across native and portability behavior.
- 21,000 push-constant draw replays, including external resets and layout or
  address changes. A synthetic 1,000-pipeline sequence with unchanged addresses
  emits one push instead of 1,000. This is an operation count, not an FPS test.
- 100,000 allocation lookups across 1,000 frame-slot and identity resets.
- New C++ replay passes normally and with AddressSanitizer/UndefinedBehaviorSanitizer.
- Existing 2.7 state/ADPF, 2.8 lookup/lifetime, 2.9 index-conversion and production
  Java report regressions pass, as do all nine existing APK policy checks.
- `git diff --check` passes. GitHub runs the new replay against NDK Vulkan headers
  and builds the full native engine; native libraries must not be reused from 2.9.

Android compilation and packaging are pending in GitHub Actions at publication.
Only one combined APK needs phone validation; no intermediate test builds are
requested. Test normal gameplay when convenient. A new bottleneck capture is
optional for selecting further work, not a prerequisite for this bundle.

Rollback: [verified 2.9 build](https://github.com/ali00978/ZMOD-Liberty/actions/runs/41711808812).

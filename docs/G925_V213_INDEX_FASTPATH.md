# G925 2.13: exact index-conversion fast paths

Based on the verified **2.12** build (GitHub Actions run 41734118290, commit
9a58e551c22922edcf5102b6ff11f926934a2eb0). Android version code 41,
`0.5.9.3-mali-alpha-2.13-g925-index-fastpath`.

## Scope and expected behavior

The 2.11 device profile indicated ~18.48 ms average native command recording,
~12.31 ms GPU timing and ~36.95 ms title frame interval during profiling.
The 2.12 presenter overlap aims to reduce serial presentation waits but has
not yet been measured on this phone. This change does not claim a specific FPS
improvement.

Within the existing `WriteNativeConvertedIndices` helper:

- When a primitive-restart marker is already the native all-ones sentinel,
  copy validated source indices in bulk rather than doing a branch and
  read/write for each index. For 32-bit indices this requires **both**
  a full-width mask and the full-width all-ones marker; 16-bit indices require
  the 16-bit all-ones marker. Masked 32-bit markers still get rewritten.
- Expand four-vertex quads (ABCD to ABC ACD) with exactly three bounded
  non-overlapping byte copies instead of decoding four integers and emitting
  six integers; this works with both 16-bit and 32-bit index elements and
  unaligned buffers.
- Other restart conversion retains exact raw non-restart index values,
  preserving marker replacement semantics.

All bounds, overflow and no-overlap preconditions are checked before these
fast paths run. Geometry, vertex order, shader and graphics settings are
unchanged. Fast 16 ms ADPF response and Stable thread selection remain
available and untouched.

The existing in-frame converted-index upload memo is also unchanged.
**This is not persistent cross-frame GPU-index-buffer caching.** That proposed
work requires explicit GPU fence, allocation ownership, memory budget and
eviction lifetime checks in the renderer; it has not been deployed here.

## Regression / delivery

`tools/test_g925_index_upload_v29.cpp` now covers the all-ones 16/32-bit
paths, masked 32-bit false positives and unaligned input/output guards,
in addition to 20,000 byte-exact randomized conversions and 1,000 scoped
generation resets. The Python wrapper checks the exact production integration.

The next full Android CI build must pass the existing C++, Java, native and
APK packaging checks. No phone-FPS or rendering confirmation is claimed before
the device is tested. Version 2.12 remains a rollback:
https://github.com/ali00978/ZMOD-Liberty/actions/runs/41734118290

## Follow-up, not included in this APK

Benchmark index-conversion counts and recording-stage time before attempting
cross-frame Vulkan buffer retention. The bottleneck report's existing broad
recording time alone does not prove repeated index conversion is dominant.

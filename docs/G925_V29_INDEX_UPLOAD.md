# G925 2.9: select index data before uploading

Version 2.8 was built and verified, but no 2.8 phone capture has been supplied.
The last supplied 2.7 profile averaged 22.58 ms command recording, 20.94 ms GPU
and 39.26 ms title intervals with profiling enabled. This change removes
redundant work visible in the native indexed-draw implementation; no phone
FPS improvement is claimed.

## Changes

- Materialize immutable host-endian index data once. Select fan expansion,
  quad expansion, restart rewriting or ready vertex-cache reordering before
  deciding whether the original GPU index buffer is needed. Upload the original
  only if it remains the final source. In particular, quad/restart draws and
  draws using ready reordered indices avoid an unused original upload/lookup.
- Reuse identical quad/restart conversions and their upload allocations within
  one native recording scope. Exact keys cover immutable source generation,
  start/count, conversion kind, index width, restart marker and index mask.
  Base vertex stays in the Vulkan draw call and is not baked into index data.
- The existing generation map resets the new cache at recording entry and on
  scope exit, including failed recordings. No mapped allocation is borrowed
  from an earlier frame. Existing upload arena and fence lifetime rules apply.
- Quad index expansion now rejects count overflow before allocating or writing.
- The capture report includes converted-upload reuse and skipped-original-upload
  counters. The existing diagnostic CPU breakdown is retained.

Version code 37, `0.5.9.3-mali-alpha-2.9-g925-index-upload`. Vulkan pipeline,
rendering order, vertices, triangles, restart semantics, quality defaults,
16 ms feedback and Stable policy are unchanged. No new Vulkan transfer command
or barrier is introduced in a render pass. Existing fan/MoltenVK handling and
fallback to original indices when asynchronous reordering is unavailable remain.

## Validation

- Production conversion helpers: 20,000 byte-exact randomized comparisons with
  an independent reference, 16/32-bit indices, unaligned storage, source/output
  guards, masked restart values, base vertices and overflow/bounds rejection.
- Every cache-key field and forced hash collisions, plus 1,000 frame resets.
- Address/undefined-behavior sanitizers passed (host leak scanning disabled).
- Existing 2.7/2.8 C++ state/lifetime replay, Java report behavioral tests and all
  nine APK regression scripts passed.
- Full ARMv9 native compilation and APK verification are required before delivery.
  Phone-side speed and rendering have not been measured for this version.

Rollback: 2.8 source/delivery commit `abe970f16820e55ec8a125eeb6290f035e13c2ae`,
[verified 2.8 APK](https://github.com/ali00978/ZMOD-Liberty/actions/runs/41666819194).
The 2.6 baseline branch remains unchanged.

## Repeat APK inspection

`tools/inspect_g925_apk.py` verifies the RSA APK v2 signature and signed ZIP
content digest, checks the manifest version, and compares the update certificate,
default arguments, packaged resources and native library set with the baseline.
It requires Python's `cryptography` package. Example:

```sh
python tools/inspect_g925_apk.py v29.apk --baseline v28.apk \
  --version-code 37 --version-name 0.5.9.3-mali-alpha-2.9-g925-index-upload \
  --native-marker converted-index-upload-reuses \
  --native-marker plain-index-uploads-skipped \
  --dex-marker counter_converted_index_upload_reuses \
  --dex-marker counter_plain_index_uploads_skipped \
  --json-file v29-inspection.json
```

The known-good 2.8 artifact passed this inspection. A copy with one ZIP content
byte changed was rejected for a signed content digest mismatch.

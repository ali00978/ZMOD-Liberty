# G925 2.20 — reduce native command-recording CPU work

Android versionCode **48**, versionName
`0.5.9.3-mali-alpha-2.20-g925-constant-index`.

Base: verified full Android Mali ARMv9 build **2.19**, commit
`10b83ff6b3bbf8983753cff88f10edc8970a47ab`.
https://github.com/ali00978/ZMOD-Liberty/actions/runs/42400752436

## Two recording hot-path changes bundled in one APK

1. **Single-hash constant arena reservations.** Every new frame constant
   allocation previously called `Find(key)` and then `Insert(key,value)`,
   hashing and walking the frame-generation map twice on a cache miss.
   `FindOrInsertWith` now hashes/probes once, performs exact identity
   comparisons and lazily computes the aligned reservation *only* on a
   genuine miss. The factory may reject an allocation without changing
   bucket count, cursor or reservation count. Duplicates still validate
   size and alignment and reuse their original offset, including when the
   arena is full. Generation-stamped reset and the existing frame-slot
   GPU submission fence remain the only lifetime authorities.

2. **Bulk-copy then patch restart indices.** The masked 32-bit/16-bit
   primitive-restart converter previously performed individual index
   writes for every value. After validating all source and output sizes,
   it now bulk-copies host-endian bytes and patches only restart markers
   with Vulkan's native all-ones sentinel. Width-dependent branches
   are moved outside the loop and unaligned accesses remain safe through
   fixed-width memcpy. Existing all-ones fast paths and quad expansion
   are retained unchanged. The output is byte-identical to the original
   conversion: no geometry, shader, draw order or GPU buffer lifetime change.

The release also includes **all 2.7–2.19 native CPU/Vulkan optimizations**:
bounded surface scanning and GPU-fence retirement, bounded presenter overlap,
exact pipeline/surface/descriptor state caches, earlier index paths, and
16ms Android ADPF CPU target. No dynamic graphic-quality reduction, shader
downgrade, driver/clock hacks or unsupported run-ahead.

## Required automated checks

`tools/test_g925_constant_index_v220.cpp` verifies:
- One hash on every normal new-key insertion and exact duplicates; lazy
  construction occurs only after a genuine cache miss.
- Factory rejection without changing size/capacity, growth beyond 50%
  load, collisions across 100 frame generations, preserved GPU-fence and
  arena alignment/byte-capacity checks.
- **200** byte-exact 2048-index reference replays, sparse and dense
  restart markers, 16/32-bit index widths, unaligned input/output,
  bounded destination and untouched guard bytes.

`tools/test_g925_constant_index_v220.py` verifies the production C++
integration and compiles/runs the regression on the CI host. It is a
required step before the **full native Mali ARMv9 APK build**; all
previous regression suites are retained.

These changes remove redundant CPU operations on known paths; the
performance benefit depends on actual draw/constant/index patterns.
A new phone FPS or p95 frametime measurement is required to demonstrate
improvements. Neither a 60 FPS guarantee nor measured speedup is claimed.

## Rollback

Last verified full native APK (2.19):
https://github.com/ali00978/ZMOD-Liberty/actions/runs/42400752436

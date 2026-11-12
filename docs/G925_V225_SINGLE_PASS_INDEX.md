# G925 2.25 — single-pass primitive-restart index conversion

Android `versionCode=53`, `0.5.9.3-mali-alpha-2.25-g925-single-pass-index`.
Based on G925 2.24 branch commit `2a5b22528de39e6364ad1f2e84df5b5d602a359f`.
The older 2.24 branch and its full native build are kept intact.

## What changed

When converting Xenos primitive-restart data that needs masked sentinel
normalization, the native renderer previously copied the full index range
then loaded every original index *again* and overwrote matching markers.
That was two traversals of source memory and redundant work on the mapped
index-upload destination.

The 16- and 32-bit masked restart paths now read every index once, rewrite
a restart marker to the Vulkan full-width sentinel, and store the result once.
Each load/store uses fixed-size `std::memcpy` for unaligned guest and mapped
host memory. The existing optimised **raw bulk memcpy** remains for
already-native full-width restart sentinel data. Quad-list expansion and
all caching, GPU ownership, index offset, restart semantics and draw order
remain unchanged.

## Correctness regression

`python tools/test_g925_single_pass_index_v225.py` is mandatory for
full Android builds. Its C++20 regression exercises varied counts, source
offsets, unaligned source/destination, sparse/multiple restart markers,
16-bit and 32-bit streams, masked upper-bit sentinels, native full-width
sentinels, preserved canaries and invalid requests that must not write
partial output. Older index regressions remain required.

## Performance expectation

On a local x86-64 Clang O3 microbenchmark, the fused conversion took
~0.22 vs 0.37 microseconds for 4,096 masked 32-bit indices and
~0.07 vs 0.15 microseconds for 4,096 16-bit indices (best observed values
on that host). These are **isolated host microbenchmarks**, not Android
frame times and not claims about actual phone FPS. A reduction can only
help when the renderer actually performs masked restart conversion;
other indexed draws are unchanged. The previous measured device frame
recording and GPU bottlenecks can therefore remain dominant.

No reduction in graphics quality, internal resolution, shader precision,
texture quality, shadows or frame cap. Production verification requires a
complete successful Android Mali ARMv9 native APK build, and actual FPS
benefits require same-scene device comparison.

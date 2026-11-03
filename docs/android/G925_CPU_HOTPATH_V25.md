# G925 2.5 - batched native CPU hot-path changes

The 180-sample 2.4 capture measured CPU command recording avg 24.40ms,
render callback avg 31.51ms, GPU timestamp envelope avg 21.64ms, and game
frame interval avg 43.32ms. A steady 60FPS requires 16.67ms/frame.
The profiler adds overhead; CPU ranges overlap.

Changes: avoid per-draw copied state-transport setting in producer capture;
read restart-required PS9 and stencil-face probe settings only once per native
frame; do not reconstruct full vertex constant bytes for disabled PS9 probes;
cache frame-only diagnostic eligibility; avoid redundant fixed-state hash
scratch zeroing (full population and validation are preserved); avoid a
light-color-delta probe configuration copy. Enabled diagnostic light probes
and legacy bypass retain behavior.

Existing optimizations already enabled include draw-state deduplication,
indexed descriptor changed-slot publication, persistent buffer reuse,
constant-ring, draw merging and texture upload batching. No visual quality,
shader, lighting, reflection frequency, game-pacing, Mali driver or GPU queue
semantics are changed. Does not guarantee 60FPS. The existing profiling
capture also exported detailed CPU operation and GPU pass CSVs, useful for
further optimization without another special profiling APK.

Only one branch push should start the single full Android native build.

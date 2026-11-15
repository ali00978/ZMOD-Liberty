# G925 2.32 — conservative 3-draw batching and lower input latency

Last phone report from v2.28: title interval 39.69ms, CPU command recorder
23.26ms, GPU 20.34ms. At 60 FPS the budget is 16.67ms each.

- 4 extra guest run-ahead frames did not fix the measured bottleneck. Default
  is now 1; the first 2.32 launch replaces the old v2.28 auto-selected 4
  with 1 and leaves other explicit 0/1/2/3 values alone. A manually
  reselected 4 after v2.28 is indistinguishable and will be reset once;
  the user can choose 4 again.
- Existing 4+ lossless opaque nonadjacent indexed Vulkan batches are
  unchanged; eligible 3-draw batches save 2 GPU draw commands where all
  captured shader/texture/raster/constant/query guards pass and at most
  1024 indices are copied. No guest triangle or draw order changes.
- Preserves graphical quality (resolution, shadows, textures, shaders,
  reflection, MSAA), system Mali Vulkan driver, saves and installation.
- C++20 randomized 100,000-case 32-bit copy tests plus v2.26 16/32-bit
  regressions. Existing build workflow also compiles and packages Android.

**No on-device improvement is yet verified.** Green CI proves integration,
not 60 FPS or real playability. CPU/GPU bottlenecks may remain.

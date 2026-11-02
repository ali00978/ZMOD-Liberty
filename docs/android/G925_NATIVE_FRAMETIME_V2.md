# G925 v2.0: experimental native CPU feedback and title frame gaps

This **native** branch starts from official-clean v1.9 (original Mali engine).
It leaves Vulkan textures, geometry cache limits, resolution, draw distance,
run-ahead and other native image-quality behavior unchanged.

- Android Performance Hint reports queued title-frame work every **25ms** by
  default rather than the original **100ms**. The OS decides CPU clocks.
  Additional worker wakeups may hurt heat or battery life.
- Launcher Performance tab exposes the **Original (100ms)** feedback cadence
  as a direct A/B control and 16/33ms optional settings. CPU thread sampling
  remains once per second; recycled thread IDs no longer cause unsigned
  counter underflow; ADPF target updates are acknowledged only on success.
- Five-second native rolling frame-gap statistics show **TITLE P95** and
  **MAX GAP** in the in-game Frametime overlay, plus counts of >33ms, >50ms,
  >100ms and pending-frame poll pressure in Android logs.
  TITLE times are game-world submissions, **not displayed Vulkan frames**.

This branch requires a **full native GitHub Actions build**, not APK repackaging.
The full build tests the C++ static asserts in g925_frame_health.h while compiling.

## On-device A/B
Use System Vulkan, the same GTA IV save/scene and original graphics. FPS cap
60 in both runs. Compare 60s stationary and 60s driving using:
(1) CPU response pacing Original (100ms); (2) Default (25ms).
Enable Frametime overlay to inspect title P95 and max gaps; note temperature.
Do not claim improvement until A/B on the actual POCO is complete.

# G925 Android: real presenter host-side timing report

Reason: title FPS declines from ~60 to 26-30 with two frames pending, while
Android thermal headroom stays ~0.5 and Mali GPU sysfs measurements are blocked.
An ADPF session log showed 'now boosting' rather than 'stable threads' although
2.2 exposes the stable option. The former 'power: hint=off' applies only to a
separate disabled legacy hint provider; it does not describe game CPU hints.

This release reuses existing **host** timing points in VulkanPresenter:
swapchain acquire, queue submit, queue present phase, and full paint attempt.
Their five-second summaries log paint-hz and distinct new title frames/hz;
max and mean acquire/submit/present/total CPU time; and unusual Vulkan result
counts. These are not GPU execution measurements and do not prove display
scan-out. Unlike new GPU queries or sync, the counters add no Vulkan work.

The ADPF worker prints effective enabled/stable/target/report cadence/session
membership every 10s. The other power thread now labels its separate ADPF
as 'legacy-hint', avoiding a false conclusion that game CPU hints are off.
The app SHARE STUTTER REPORT includes presenter summaries.

For testing, use the same city route, resolution and System Vulkan. Compare
CPU hint Stable vs Original mode at similar temperature. Observe whether
paint-hz falls with title FPS and which host paint phases are blocked. When
acquire/present CPU phases stay low, further work needs native render-worker
or GPU timestamp attribution, not more CPU boost. Do not claim improvements
without Android hardware measurements.

No changes to shader quality, texture caches, runahead, graphics presets or
Vulkan synchronization. No guaranteed FPS uplift.

# G925 2.17 — target frame-time spikes, not quality

Android versionCode **45**, `0.5.9.3-mali-alpha-2.17-g925-stutter-guard`.
This branch contains every 2.7–2.16 CPU/Vulkan optimization, and adds
**two concrete maintenance spike controls**. The user's last on-phone 2.11
detailed profile suggested a refresh/presenter hot spot; 2.12 added bounded
render/present overlap and 2.16 added shader/state/descriptor hit-path
optimizations. No newer phone capture establishes the root cause of remaining
drops, so these changes specifically address plausible *periodic CPU*
spikes without making unsupported FPS promises.

## New work

1. **Amortized Vulkan surface destruction:** The normal native housekeeping
   phase retires at most **12** GPU-completed, pending surface images per
   presentation call, rather than destroying an unlimited number of
   VkImageViews, VkImages and VkDeviceMemory allocations at once. Every
   pending lifetime remains **not reusable** by new draws. Release still
   requires `last_used_submission <= completed_command_buffer_submission_`.
   Deferred completed releases remain in the pending set for the next frame.
   On an explicit release while the title has no frame or submitted command
   buffer in flight, a `force_drain` call retires every completed pending
   surface; in-flight surfaces stay pending. Final shutdown still destroys
   all remaining resources using the existing path.

2. **Smaller background host-buffer cleanup bursts:** At or below the
   256 MiB native buffer-cache budget, a background reclamation sweep
   now limits work to **16 resources / 4 MiB** per pass rather than
   64 resources / 16 MiB. The existing pending-sweep mechanism resumes
   cleanup on subsequent frames. If retained bytes exceed the cache
   budget, the previous more aggressive **64 resources / 16 MiB** policy
   is used to reduce memory pressure. No extra allocated memory and
   no disabling of texture pressure monitoring.

Neither path removes GPU-fence completion, descriptor lifetime checks or
resource owner accounting. Vulkan shaders, rendering resolution, graphics
assets, UI, adaptive visual settings, Fast 16 ms CPU feedback, Stable ADPF
selection, frame cap and bounded presenter overlap are unchanged.

## Verification

`tools/test_g925_stutter_retirement_v217.cpp` simulates 300 pending
GPU-backed image lifetimes, verifies at most 12 completed retirements per
normal frame, never frees a still-in-flight resource, drains 1,000 completed
lifetimes when the title is idle, and exercises both memory-pressure
buffer-work budgets. `tools/test_g925_stutter_retirement_v217.py`
verifies real integration sites and executes the C++ test under the
existing host CI toolchain. The full Mali ARMv9 Android build continues
to run every earlier native/Java regression plus this new gate.

This is a **frame-spike hypothesis**, not an identified root cause from a
2.16 CPU detail capture. Real performance must be evaluated after a built
APK, preferably with one repeatable gameplay scene, without profiler
overhead for the FPS check and then one profiler capture if needed.

## Rollback

The 2.15 Android APK is fully built and verified:
https://github.com/ali00978/ZMOD-Liberty/actions/runs/41763984704

The v2.16 CI native APK compilation was still running when this 2.17
development branch was prepared. It is retained intact until a
clean successor is ready.

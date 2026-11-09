# G925 2.27 — Android notification shade / foreground stutter recovery

Android `versionCode 55`, `0.5.9.3-mali-alpha-2.27-g925-notification-resume`.
Base: successful [full 2.26 Mali ARMv9 APK](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42541842724).

## User-reported failure

On the POCO X8 Pro Max, opening the Android notification shade or receiving a
notification can cause large, persistent GTA IV frame hitches **and audio stutter**.
The existing v2.26 workload optimizations do not address this focus/lifecycle path.

## Concrete correctness fixes

1. In `WindowSDL::HandleAppLifecycleEvent`, the Android renderer used to
   detach its Vulkan `ANativeWindow` surface on BACKGROUND/HIDDEN/MINIMIZED.
   It reattached only on FOREGROUND/RESTORED/SHOWN. Android notification
   overlays can produce FOCUS_GAINED or EXPOSED without a matching RESTORED,
   leaving the surface permanently detached. Now either event reattaches
   only when `surface_detached_` is true **and** there is a valid native
   window. No unconditional swapchain rebuild on notification arrival,
   no new GPU fences or waits, and no change to graphics quality.
2. When a notification causes FOCUS_GAINED **without detaching the native
   surface**, the Android UI thread also requests a presentation-timeline
   resync. An atomic flag is consumed only by the exclusive
   `Presenter::PaintAndPresent` owner, preventing a race with the guest
   output painter. This resets stale phase observations without recreating
   a healthy swapchain or forcing a GPU wait.
3. In `AudioSystem::WorkerThreadMain`, a 500ms timed semaphore wait was
   followed by an extra 500ms sleep on timeout. If SDL playback stopped
   supplying audio callbacks during a notification transition, that extra
   sleep delayed recognition of newly available credits even after Android
   returned to the game. A timeout now immediately rearms the *blocking*
   semaphore wait; an unexpected wait failure gets a bounded 5ms backoff
   to avoid a busy loop.

## Validation

`tools/test_g925_notification_resume_v227.py` asserts the actual SDL window lifecycle event integration, guarded reattachment,
atomic paint-owner frame phase reset, and the audio worker's timeout/wakeup behavior. The test is mandatory in full native
APK CI, together with all older C++/Java regressions.

This addresses two real recovery vulnerabilities, but **is not proof** that
they were the only cause of the user's notification-related slowdown.
Phone reproduction and logs are needed to confirm the issue is gone; do not
claim a measured FPS increase or guaranteed restoration of 60 FPS.

Rollback: [2.26 build](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42541842724).

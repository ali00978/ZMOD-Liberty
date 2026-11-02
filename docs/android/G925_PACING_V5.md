# G925 v5: experimental run-ahead 4 and Vulkan FIFO pacing

This builds from the last validated v1.6 Mali ARMv9 APK, but changes native
code. Therefore **a full native build is required**; fast Java-only mode
intentionally rejects it. Do not bypass the native-source compatibility guard.

## What changed
- GTA IV title's optional extra-frames-in-flight range now permits 0–4.
- The default Standard 60 profile uses RA4 and strict FIFO on Mali,
  preserving all engine-default visual settings, but this may INCREASE
  input latency and stutter on some devices.
- A one-time v5 migration updates ONLY the two pacing keys in graphics.txt,
  retains all other user settings and backs up the file.
- **G925 STUTTER FALLBACK / RA0** restores 0 and mailbox pacing with one tap;
  Save changes and restart the game. Reset to default restores the RA4
  experiment. To A/B test only Run-ahead, switch only that selector.
- A title-frame diagnostic log every two seconds includes maximum game
  update gap, updates over 50 ms and outstanding render pending count.
  These are not the same as GPU or SurfaceFlinger displayed frame timings.
- ADPF busy-thread sampling now prevents unsigned tick-counter underflow when
  an Android thread ID is reused.

## How to measure before making more engine changes
1. Keep the SAME scene, graphics, display refresh rate and phone temperature.
2. Record FPS and ms for 60 s stationary and 60 s driving at RA0, RA2 and
   RA4 with strict FIFO, then repeat with mailbox. Restart when changing FIFO.
3. Compare the 1% lows / large frame gaps, not only maximum FPS.
4. Run 15 minutes to identify thermal throttling.
5. If title gaps are smooth but screen stutters, profile Vulkan present and
   SurfaceFlinger using adb/Perfetto rather than raising Run-ahead again.

No artificial 60 FPS guarantee: frame-time <16.7 ms is required for 60 FPS.

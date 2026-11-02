# G925 2.0 Native Smooth Lab: controlled CPU / frame-stability experiment

Based on the **original Mali-compatible Android native engine**, not
G925 1.7's modified run-ahead/FIFO experiment or G925 1.8's experimental
app settings. Full native recompilation is required for these changes.

## Measured hypotheses, not claimed results

1. The Mali-specific performance-hint recorder previously swapped a
   vector every 100 ms, forcing its next game-frame insertion to make a
   new heap allocation. The game-thread recorder now uses a fixed
   256-sample array with no per-frame allocations. Dropping surplus
   samples is safe; it cannot change game output or physics.
2. Android power reports sample Linux thread CPU counters every second.
   Counter underflow after a recycled thread ID could classify a thread
   incorrectly as busy. The new guard prevents that.
3. The extra Mali frame-wait hook calls clock_gettime from a potentially
   tight pending-frame polling loop, and wrote to logcat every two
   seconds. It now samples only new title frames (plus every 64 polls)
   and logs heavy frame-gap diagnostics only when explicitly enabled.
4. The upstream official Android CPU governor and the Mali fork's
   replacement CPU hint use different strategies. The launcher now
   offers an A/B switch; only one is enabled at a time, with a 20 ms
   requested CPU work target. This is not an overclock and may perform
   differently under thermal throttling.

All renderer quality, shaders, reflections, population, texture cache
retention, and run-ahead max (0–2) remain original.

## How to benchmark

From a clean APK installation, select the "CLEAN ORIGINAL SETTINGS +
60 FPS" button to back up and clear legacy G925 experiments. Compare
"ORIGINAL CPU HINT / 20 MS" and "MALI CPU HINT / 20 MS" with the same
graphics, driver, temperature, location, battery level and display
refresh rate. Save and restart between tests. Enable Frame pacing
diagnostic only when collecting debug logs; it reports title submitted
frame gaps and does not imply GPU presentation timing. For true display
jank, use Vulkan/SurfaceFlinger Perfetto tracing on the device.

A successful build is a syntax/integration test, not proof of smooth
gameplay or guaranteed 60 FPS. Avoid changing display quality and
CPU policy simultaneously during A/B tests.

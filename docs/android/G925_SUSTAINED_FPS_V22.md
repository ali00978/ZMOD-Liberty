# G925 sustained performance 2.2: response to captured 2.1 device log

The POCO's report shows game-frame submission 59.6 FPS initially, then a
sustained ~18-30 FPS with two renderer frames pending and repeated ADPF
setThreads updates virtually every second. Title-frame gaps reached 252ms.
This demonstrates real render backpressure, not which GPU or CPU step is
slow. The Android power governor's session membership was visibly unstable.

Changes: give priority to Main XThread and GtaRecorder even when they are
blocked; retain still-alive selected threads over two missed 1s samples and
replace on the third; remove dead TIDs immediately. Expose Stable/Original
CPU hint selection and Android CPU performance hints On/Off for A/B testing.
The 10-second power log includes MediaTek GPU utilization, battery temperature
and Android thermal headroom; unavailable values remain n/a. The launcher
Share Stutter Report includes the power and thermal lines.

User experiment (same save, same graphics and driver, equal cooldown):
(A) stable hint ON, (B) CPU hints OFF, optionally (C) original switching.
Inspect title FPS/p95, GPU busy, headroom and battery temperature at 10s
intervals. If GPU busy is near 100% while CPU hints OFF makes no difference,
look at native Vulkan GPU stage timings next; no guarantee of sustained 60 FPS.
Do not confuse title submissions with displayed frames. No overclock,
visual-quality reduction, renderer cache changes or extra runahead.

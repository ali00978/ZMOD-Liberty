# G925 Native CPU/GPU bottleneck profiling 2.4

**Why**: 2.3 showed title frames dropping from 60 to ~30 after one minute
despite roughly 60 CPU-side Vulkan presentation attempts per second. The
swapchain acquire CPU phase averaged 0.2ms and native ADPF stable=0. The user
separately tested Stable and reported the same ~30FPS after one minute.
The hint selector is not an established fix.

The renderer has *already* shipped an opt-in GPU timestamp and CPU stage
profiler, but the ordinary Android Play (Debug Log) path configures only the
'logging' diagnostics category, so no native timing export exists. This release
exposes that existing profiler in a dedicated launch button.

## Use

In the launcher select CAPTURE 30 FPS BOTTLENECK. It starts debug logging,
enables native-profiler for **this launch only**, and arms native capture
65 seconds after the first game frame. It captures 180 consecutive frames at
native interval=1 with 64 GPU queries per frame, detailed native CPU and GPU
timings, then exports to:
Android/data/com.libertyrecomp/files/LibertyRecomp/Diagnostics/
native-performance-latest.csv. Keep playing for at least 85 seconds on the
same street, then return to launcher and press SHARE NATIVE CPU + GPU REPORT.

The launcher exports a bounded text summary of GPU frame time (if available),
CPU frame interval, fence wait, native command recording/finalize, texture
preparation, render callback, queue submit, pipeline waits and compilation,
and top eight slow sampled intervals. No game assets or raw diagnostics paths
are shared. The report checks that the CSV was generated after the current
capture started, preventing accidental reuse of yesterday's profile.

GPU profiling introduces instrumentation overhead **only during capture**.
It does not turn on by default. No changes to resolution, textures, shaders,
runahead, presentation mode or CPU governor. The goal is root-cause
classification, not a guaranteed FPS increase.

# G925 X v0.9 mobile FSR and frame interpolation experiment

The Android APK adds a 720p-output Quality FSR1 option (853x480 input for 16:9). It leaves the saved FPS cap and other graphics options unchanged. The GPU can render fewer pixels, but CPU-bound performance may remain unchanged.

The two standalone Vulkan compute shaders in tools/experiments/g925x_framegen implement an initial low-resolution block-motion search and confidence-gated midpoint reconstruction. They are **not linked or invoked by the APK**. The desktop FidelityFX SDK selects VK_X64 and the Android build does not currently stage the required temporal archive. Turning on the desktop cvar alone would not implement mobile frame generation.

CI compiles and validates both shaders and checks the CPU reference; it does not demonstrate real GPU execution or latency. Before runtime integration: provide two immutable consecutive HUD-free frames, build/allocate low-resolution motion resources, dispatch with correct Vulkan barriers, detect cuts and invalid history, composite HUD separately, submit and pace real/generated frames via FIFO with bounded lifetime and measure actual generated vs real FPS on G925. Disable if GPU overhead or timing is unacceptable. This experiment is not AMD FSR3 or DLSS.

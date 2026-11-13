# G925 v2.31 — lossless Mali Vulkan clear fast paths

Baseline v2.30: full native build passed; handset GPU still measured
~20.34 ms on v2.28 (180 sampled frames), above the 16.67 ms 60 FPS budget.

1. A **new rendering scope** beginning with a guest full-area color clear
   can set the matching Vulkan color attachment loadOp=CLEAR, avoiding the
   separate vkCmdClearAttachments. Each float color component is reinterpreted
   from the same captured guest bits; attachment masks preserve guest intent.
   Depth/stencil folding already existed and remains independently controlled.
2. The explicit color clear is masked off **only** for attachment slots folded
   into that exact command's new pass. If other aspects need clearing, they
   still use the original Vulkan clear path. The producer command timeline
   and surface-content tracking are unchanged.
3. Empty clear rectangles are rejected before acquiring the presenter or
   starting an implicit attachment load-op. The calculation matches the
   original logical-scissor clamp used by RecordClear.
4. Special diagnostic, phone/TV, fire and readback paths do not take the
   load-op fast path, preserving exact capture ordering and observability.
5. Both features have launch-time CVars and can be disabled for regression.
   No shaders, render resolution, MSAA, shadows, geometry or textures change.

Randomized C++ tests verify 250,000 rectangles, 16-bit masks, partial/oversized
cases and zero-area equivalence. Android APK workflow executes integration
checks alongside existing regressions.

Build success tests code integration, NOT a handset frame-time improvement.
The actual number of full clears and Mali savings depends on gameplay;
measure real GPU p95 without heavy profiling before claiming 60 FPS.

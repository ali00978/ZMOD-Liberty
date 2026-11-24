# G925 Native v2.44 / standalone v3.5: CPU/GPU draw-call fusion

## Why a renderer change?

Measured in the user's 2026-11-24 v3.3 capture:
- Title interval mean 34.80 ms (28.7 FPS-equivalent), p95 45.74 ms.
- Native command recording mean 19.25 ms, p95 28.26 ms.
- GPU envelope mean 17.31 ms, p95 25.42 ms.
- 3538.7 recorded commands and 3063.5 indexed draws/frame.
- Presenter publication wait self-time: 5.118 ms/frame. Blindly removing this can drop visible frames.
These figures are from an instrumented, quality-reduced capture. Do NOT claim a same-quality FPS gain before a controlled phone test.

Arm's official GPU Best Practices strongly recommends reducing draw calls in CPU-bound Mali graphics. We targeted native draw count rather than resolution, reflections, shadows, draw distance or traffic.

## Before

The existing native sparse indexed batching already fused identical opaque triangle-list draws with matching pipeline, shader constants, descriptors, vertex/index buffers, render state, textures, and no trace/query/stencil/blend/vertex-ID side effects. However, it refused candidates with different baseVertex despite all those other state fields matching.

## After

A new opt-out cvar gta4_native_merge_rebased_indexed_draws (default On) extends only that safe opaque-only path:
- Each guest range carries its original signed baseVertex.
- A two-pass, allocation-bounded index converter prevalidates all 16/32-bit indices before writing anything. Negative effective vertex indices, signed overflow, bad ranges and small destinations reject without partial writes.
- Chooses the lowest original signed baseVertex as a shared Vulkan vertexOffset. Converted 32-bit output index always satisfies: newIndex + sharedBase == originalIndex + originalBase.
- Submits one vkCmdDrawIndexed instead of separate draws only after all invariants succeed.
- Triangle order, winding, vertex fetch, shaders, attachments and effects are preserved. No vertex is skipped.
- Existing same-base fast memcpy batching remains untouched.
- Allocation or conversion failure falls back to the original draw and leaves later queued draws intact.
- Adds counter_rebased_indexed_draws_merged to the shared profiler for the actual Vulkan draw-call savings.
- Adds a Graphics & Controls > Performance menu switch: Indexed draw batching: On/Off for same-scene A/B testing.

## Tests and release

New test_g925_rebased_index_batch_v244.py compiles a C++20 test with >14,000 random 16- and 32-bit batches, including overlapping/out-of-order ranges, negative base offsets, overflow, output bounds, source preservation and exact per-vertex identity. It also checks integration gates (opaque-only, immutable state, no shader vertex-ID use), the correct Vulkan output index type, optional toggle and profiler counter. Existing sparse batching, native renderer state, and Java APK regressions must also pass.

Independent Android package: com.zmodliberty.g925native, versionCode 72, G925 Native v3.5 Armv9/Mali. The full-quality repair from v3.4 remains in place.

## Honest success criterion

Perform multiple A/B game runs on the real POCO with the exact same save, scene, native resolution, anti-aliasing, full draw distance, reflection refresh, traffic density, device temperature and 60-second warm gameplay. Compare actual title frames, p95, CPU recorder, GPU envelope, audio/input latency and frame artifacts. If rebased indexed draw merges are low, the new path cannot provide much FPS in that scene. If performance/quality degrades, disable it.

A 16.67 ms/frame budget is needed for real 60 FPS. A successful APK build proves correct compilation, not higher real-world FPS.

References: Arm GPU Best Practices (https://documentation-service.arm.com/static/67a62b17091bfc3e0a947695), Khronos Vulkan indexed vertex offset semantics (https://docs.vulkan.org/spec/latest/chapters/drawing.html), user's 2026-11-24 frame report.

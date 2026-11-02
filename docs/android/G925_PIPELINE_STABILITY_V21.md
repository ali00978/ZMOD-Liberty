# G925 Native Pipeline Stability 2.1 (controlled stutter fix)

Base: successfully rebuilt 2.0 native engine (commit
`5451fd7b2d222c378ac6e906c8a85a9dfa405bcf`).
Quality, system Vulkan driver, original native texture/buffer cache policy,
render queue depth, run-ahead and visual settings are unchanged.

## Confirmed source bug
`NativePipelineCompiler::Enqueue` previously always discarded
`queue_.back()` to accept a demanded pipeline when bounded admission was full,
even if that queued job was **already demanded**. Duplicate requests for a
previously speculative job returned true without elevating its priority. The
result was a correctness and scheduling risk: exact first-use pipelines could
be silently removed, causing a missed async compilation and later synchronous
fallback in a newly visited game scene.

The fix:
- marks exact-demand jobs in the admission queue;
- promotes existing speculative jobs when they become demanded;
- protects jobs while a caller awaits completion;
- evicts only **unstarted speculative** jobs, never demanded ones;
- rejects excess demand when all capacity is consumed by demanded jobs,
  preserving the existing fallback; does not enlarge the cache or queue.

Two separate C++ regressions check preservation, refusal at full demand,
speculative eviction, promotion and deduplication. The full native build
compiles the header; unit test suite is in
`tests/unit/graphics/native_pipeline_compiler_test.cpp`.

## Honest performance scope
This fix cannot guarantee 60 FPS and cannot eliminate
slow DXT texture streaming, vendor driver shader compilation,
CPU-heavy city scenes or thermal throttling. No real phone measurement has
yet demonstrated the bottleneck. Run identical scene tests at CPU response
25ms and 100ms, and share the Android Play (Debug Log) output with the
`title-fps:` lines to classify remaining stalls.

A success status from GitHub Actions confirms the build, not device FPS.

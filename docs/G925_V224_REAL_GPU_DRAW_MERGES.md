# G925 2.24 — eliminate actual GPU drawcalls without graphics-quality reduction

Android `versionCode=52`, `0.5.9.3-mali-alpha-2.24-g925-real-draw-merge`.
Base: verified [G925 2.23 full native Mali ARMv9 APK](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42438143367) (commit `0901071acfbed645fe8d1677f01eea3e730af0e1`).

## What changed, beyond CPU lookup optimizations

The user's 180-frame G925 diagnostic: title interval avg **36.11ms**,
GPU timestamp avg **21.39ms** and CPU command recording avg **20.27ms**.
The recorder issues roughly **3,565 Vulkan draws/frame**. Both CPU and
GPU exceed the 16.67ms 60 FPS budget. Previous 2.21/2.22 optimizations
only reduced CPU lookup overhead; 2.23 elides provably zero-fragment
draws. This release additionally **removes redundant real drawcalls**.

### 1. Xenos RectangleList — one indexed draw instead of N draws

The native renderer already reconstructed a fourth corner for each
three-vertex RectangleList item. It then issued one
`vkCmdDraw(4,1,rectangle*4,0)` for *every* rectangle. Now a command
with at least two valid rectangles may allocate `5*N-1` 32-bit indices
and issue **one** `vkCmdDrawIndexed` using the unchanged triangle-strip
pipeline and Vulkan primitive restart. Index stream:

`0,1,2,3,FFFFFFFF,4,5,6,7,FFFFFFFF,8,9,10,11,...`

Restart markers prevent strips from joining, retaining exactly the
same two triangles, winding, per-rectangle order, fourth-corner
interpolation, material state, vertex bytes, shade and render target.
No shader recompile, reduced resolution or lower graphics settings.

Disabled for temporal shaders and vertex shaders reading
`gl_VertexIndex` / instance index; required pipeline prewarmed. If the
restart pipeline is unavailable, the old one is selected. If the new
index upload fails, the original individual unindexed draws still
execute with their unchanged geometry. Both full and sparse upload
capacity planners now include this extra index stream.

### 2. Consecutive native triangle-list DrawPrimitive commands

A run of *separate commands* with precisely adjacent vertex ranges
in the same buffers (and vertex counts divisible by three) may be
emitted as **one larger vkCmdDraw**. Early cheap checks reject gaps,
overlaps, split triangles and integer overflow; exact comparisons of
shaders, pipeline snapshot, textures, vertex buffers, descriptor page,
per-stage constant hashes, fetch state, fixed-function state, lighting
and render phase still gate every candidate. Special queries,
temporal, fire, phone/TV, diagnostics and vertex-index shaders remain
on the original path. Run length is capped at **64** commands.

A failed merged draw retries the first original draw and preserves
the remaining commands. Guest draw snapshots are immutable. This
generalizes the already-existing adjacent-index and DrawPrimitiveUp
merge logic, and keeps all original triangle ordering/winding.

### Performance measurement and rollback

Default-enabled optional CVars:
- `gta4_native_merge_rectangle_draws` (can disable without rebuilding)
- `gta4_native_merge_sequential_draws` (can disable without rebuilding)

New frame-profile counters:
- `rectangle-draws-merged`: actual per-rectangle Vulkan drawcalls saved
- `sequential-draws-merged`: actual adjacent nonindexed draws saved

A count of **zero** indicates no GPU drawcall savings from that optimization
in the measured scene. The CPU/GPU impact depends on game content and
will require an on-device same-scene benchmark. Even with fewer drawcalls,
fragment-shader GPU work is largely unchanged; **60 FPS is not guaranteed**.

### Verification

The mandatory `tools/test_g925_rectangle_batch_v224.py` compiles
`test_g925_rectangle_batch_v224.cpp` as C++20 and verifies:
- Up to 1024 independently drawn RectangleList strips produce the **exact
  same triangle index triplets, winding and order** when combined by
  restart markers, with guard bytes and overflow/bounds rejection.
- 15,872 deterministic contiguous triangle-list runs preserve the exact
  original vertex/index ordering; invalid vertex ranges/partial
  triangles/overflow are rejected.
- Production integration: two existing upload planners, command
  dispatch, pipeline prewarm/restart, safe fallbacks, exact state checks,
  feature toggles and both frame performance counters.
- All prior C++/Java native regressions must pass before the full
  native Android Mali ARMv9 APK is considered built.

Last stable rollback: [2.23](https://github.com/ali00978/ZMOD-Liberty/actions/runs/42438143367).

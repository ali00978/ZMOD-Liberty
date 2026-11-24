# G925 Native v2.43 / standalone v3.4 — restore graphics fidelity

User report from standalone G925 Native v3.3 (2026-11-24, 180 measured frames):
- Actual game-frame interval 34.80ms average, 45.74ms p95 (~28.7 title FPS equivalent while profiling).
- GPU timestamp envelope 17.31ms average, 25.42ms p95.
- Native CPU command recording 19.25ms average.
- Most expensive *CPU self-time* includes presenter-publication-wait 5.118ms/frame, record-frame 3.633ms, record-command 2.950ms, record-indexed 2.344ms, common-draw-bindings 1.905ms, vk-render-pass-boundary 1.419ms.
- User did not like draw distance / quality. Report showed \`gta4_dynamic_draw_distance=true\`, \`gta4_dynamic_draw_distance_min=0.80\`, and \`gta4_fps_guard=2\`, despite the older 2.42 launcher button only preserving graphics options and never undoing previous reduced-visual settings.
- Four guest run-ahead frames and native queue limit of four were configured. Increasing queue depth can change latency; these settings are not guaranteed FPS gains.

## User-visible correction

In the standalone app only, the Graphics & Controls card now has a separate:

**RESTORE FULL DRAW DISTANCE + REFLECTIONS**

On explicit tap, with no unsaved UI changes, it performs a **lossless merge**
that sets only:
- \`gta4_dynamic_draw_distance=false\` — no automatic draw-distance degradation;
- \`gta4_draw_distance_scale=1.0\` — 100% original console draw distance;
- \`gta4_fps_guard=0\` — no adaptive reflection refresh suppression.

It backs up the previous \`graphics.txt\` once to
\`graphics-before-g925-full-visual-restore.txt\` and preserves every
other line/cvar, including resolution, upscaler, AA, textures, shadows,
population, frame cap, CPU hints and all unknown custom options. The
preset is opt-in to avoid silently overwriting users who intentionally
prefer adaptive visuals. It keeps the existing v2.42 quality-neutral
CPU FPS boost separate.

The old \`gta4_dynamic_draw_distance_min=0.80\` setting may still be
present but cannot lower distance while adaptive scaling is disabled.
The user may optionally choose 120% draw distance in the normal menu
after saving/restarting, at a significant possible CPU/FPS cost.

**Immediate fix without updating:** On v3.3, Graphics & Controls →
Performance: Automatic draw distance = Fixed (keep quality);
Draw distance = 100% (console); Reflection FPS guard = Off (keep quality);
Save changes, then restart Play.

## Build / validation

VersionCode 71, version suffix \`alpha-2.43-g925-fidelity\`, standalone
package \`com.zmodliberty.g925native\`,
\`ZMOD-Liberty-G925-Native-v3.4-armv9.apk\`.

Java regression verifies stale/multiple visual overrides are replaced,
unknown graphics and comments preserved, stable repeated application,
and launcher UI recovers exactly the three named visual cvars.
The deep profiler changes on this branch also report exact-frame
transport, GPU pass, and shader-phase costs; they do not change rendering.
Use identical save/scene for A/B measurement. The FPS may **decrease**
after restoring quality because the previous FPS data had quality-lowering
guards enabled. No claim of same-quality FPS improvement without a
real G925 device comparison.

Normal anti-aliasing, upscaler and texture settings are not automatically
increased or reset; this patch targets the specific quality regressions
actually confirmed by the user's capture.

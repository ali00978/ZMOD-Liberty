# G925 Native Max V3 — maximum native image quality baseline

**Target:** POCO X8 Pro Max, Immortalis-G925 Mali GPU, Dimensity 9500s,
System Vulkan driver. Experimental edition; compiled successfully only when
its corresponding GitHub Actions run says **Success**. Stable performance and
absence of rendering stutter cannot be verified without on-device testing.

## Intent

The user reports transient gameplay freezes that resume, rather than a crash,
with former graphics options on High. They explicitly prefer **all high and
maximum device native resolution as defaults**, and want renderer optimizations
to be attempted without secretly lowering those visual settings.

Therefore the V3 launcher **selects maximum visual options by default** on the
first launch of this experimental Mali build. It does not silently reset these
values on subsequent launches; user preferences remain editable and persistent.

## One-time new default selections

| Option | Value | Meaning |
|---|---|---|
| Output height cap | `0` | Use full device output resolution (no height cap) |
| Upscaler | `native` | Do not render internally below output resolution |
| AA | `smaa` | Highest standard AA choice in the launcher |
| Anisotropic filtering | `16x` | Maximum menu anisotropic filtering |
| Frame limit | `60` | 60 FPS ceiling, **not** 60 FPS guarantee |
| Draw distance | `1.2` | 120% of console distance |
| All four population scales | `1.0` | 100% traffic, parked cars, pedestrians, scenarios |
| Shadow map base size | `512` | Highest currently exposed setting |
| Reflection resolution | `full` | Full reflection resolution |
| Reflection capture distance | `far` | Largest reflection capture distance |
| Reflection AA | `original` | Preserve reflection AA |
| Dynamic draw distance | `false` | Do not silently reduce draw distance |
| Reflection FPS guard | `0` | Do not reduce reflection update quality |
| Extra title run-ahead | `0` | Avoid extra input latency |
| Performance Hint target | `20` ms | Performance-oriented Android CPU hint, not overclock |
| Thermal guard | `false` | Do not automatically lower draw distance |
| Motion blur | `false` | Sharp static detail, not a detail-level limit |

**Warning:** This is a much heavier load than native 720p or FSR Quality and
may produce worse FPS or thermal throttling, especially with full, far
reflections. This edition honors the requested high-resolution baseline; it
does not claim to reach 60 FPS or fix all stutters by default. Running at
full screen resolution does not always mean a 1080p render on high-DPI phones.

## Performance improvements retained from V2

- More forgiving texture image eviction window (240 → 300 frames).
- Longer retained texture cache (600 → 720 frames).
- Cached reusable CPU-side buffer cap (256 → 320 MiB) and retention
  (600 → 720 frames).
- Original Vulkan budget-pressure hysteresis, allocation retry, and
  resource-lifetime protections retained.
- Optional Extreme 16ms CPU Performance Hint, not forced by max profile.

**None of the above reserves arbitrary RAM or overclocks the device.**
Cache changes may *potentially* reduce repeat uploads when returning to
previously visited streets; there is no proof they fix the reported stalls.

## Backup, migration and rollback

At the **first launcher start** with this APK, and only for Mali builds:

1. Check the `.g925_native_max_v3_initialized` marker under
   `Android/data/com.libertyrecomp/files/`.
2. If there is already a `graphics.txt`, copy it into
   `graphics.before_g925_max_v3.txt` before writing new defaults.
3. Write the max graphics choices into a staging file and replace
   `graphics.txt` only after that succeeds.
4. Create the marker. **Later launches do not override graphics settings.**

The graphics menu has **G925 NATIVE MAX / ALL HIGH** to reselect max quality,
**RESTORE PREVIOUS GRAPHICS** to reload the pre-v3 backup (if one exists),
and still offers the V2 **HIGH SMOOTH** and **NATIVE DETAIL** fallbacks.

Back up installed game assets and savegames yourself before installing the APK.
Do not uninstall the existing working package to fix an install issue. Android
normally blocks app-version downgrades without data loss risk.

## Baseline performance testing

With System Vulkan selected and no PanVK overrides:

1. Start the new APK. In GRAPHICS & CONTROLS confirm Resolution =
   Screen resolution, Upscaler = Off/native, 16x filtering, High shadows,
   Full/Far reflection, Draw distance 120%, Population 100%.
2. Turn on FPS, frame-time and memory overlay, then **Save changes**.
3. Run a repeatable 60–120 second route, recording FPS lows and size/frequency
   of temporary freezes. Compare after game has warmed up.
4. If stutters persist on max quality, use the launcher **Play With Debug Log**
   and collect the log plus any GPU driver warnings to find the bottleneck.
5. Compare V2 High Smooth only as a separate diagnostic experiment; it lowers
   internal render resolution and is **not** the default for this v3 edition.

The heaviest cases may be shader creation, texture upload, Vulkan allocation
pressure, or CPU renderer queues. Resource caching is not a universal fix.

## Static checks

```sh
python tools/test_g925_graphics_lab.py
python tools/test_g925_cpu_hint.py
python tools/test_g925_cache_policy.py
python tools/test_g925_native_max_v3.py
```

Native and Java compilation must succeed and on-device high-detail gameplay
must be tested before claiming the stutter issue is fixed.

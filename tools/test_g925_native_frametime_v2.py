#!/usr/bin/env python3
"""Guard native Android title-pacing experiment. Runtime FPS requires hardware."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
android=r/"glue/rexglue-sdk-main/gta4-recomp/src/android"
hint=(android/"android_performance_hint.cpp").read_text()
hook=(android/"android_frame_wait_hook.cpp").read_text()
health=(android/"g925_frame_health.h").read_text()
overlay=(android/"android_perf_overlay.cpp").read_text()
launcher=(r/"os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
flow=(r/".github/workflows/build-android-apk.yml").read_text()
native=(r/"glue/rexglue-sdk-main/src/graphics/gta4_native/native_frame_scheduling.h").read_text()
assert 'REXCVAR_DEFINE_UINT32(android_performance_hint_report_interval_ms, 25' in hint
assert '.range(16, 100);' in hint
assert 'std::chrono::milliseconds(interval)' in hint
assert 'scan_now - last_thread_scan >= std::chrono::seconds(1)' in hint
assert 'busy.Sample(self);  // Establish initial CPU tick baseline.' in hint
assert 'utime + stime >= last->second' in hint
assert 'api.update_target(session, target) == 0' in hint
assert 'std::array<int64_t, 256> g_frame_durations{};' in hint
assert 'std::copy_n(g_frame_durations.begin(), duration_count, durations.begin());' in hint
assert 'g_frame_durations.push_back' not in hint
assert 'durations.swap(g_frame_durations)' not in hint
assert 'if (!REXCVAR_GET(android_performance_hint)) return;' in hint
assert '#include <cstddef>' in health
assert 'class G925FrameHealthWindow' in health
assert 'static_assert([] {' in health
assert 'std::array<uint32_t, 11> histogram_' in health
assert 'health.ObservePoll(pending)' in hook
assert '((++poll_counter & 63u) == 0u)' in hook
assert 'if (new_frame) health.ObserveTitleFrame(now_ns)' in hook
assert 'elapsed >= 5.0' in hook
assert 'metrics.over_50ms' in hook
assert 'g_liberty_title_p95_ms.store' in hook
assert 'TITLE P95  %.0fms' in overlay
assert 'MAX GAP  %.0fms' in overlay
assert 'std::array<Line, 8> lines{};' in overlay
assert 'std::vector<Line> lines;' not in overlay
assert 'if (line_count >= lines.size()) return;' in overlay
assert 'for (size_t i = 0; i < line_count; ++i)' in overlay
assert '1000.0f / fps' not in overlay
assert 'new GraphicsSetting("CPU response pacing",' in launcher
assert 'new String[] {"16", "33", "100"}' in launcher
assert 'case "CPU response pacing":' in launcher
assert 'kNativeTextureEvictionGraceFrames = 240;' in native
assert 'kNativeTextureCacheRetentionFrames = 600;' in native
assert 'kNativeBufferCacheRetentionFrames = 600;' in native
assert 'kNativeBufferCacheLimitMiB = 256;' in native
assert 'branches: [feature/g925-native-frametime-v2]' in flow
assert "github.event_name == 'push' ||" in flow
print("PASS: native ADPF feedback A/B, title gap histogram, original Mali renderer preserved")

package com.libertyrecomp;

import java.util.ArrayList;
import java.util.List;

public final class G925NativeDefaultsTest {
    private static void check(boolean value) {
        if (!value) throw new AssertionError("G925 native edition defaults regression");
    }

    public static void main(String[] args) {
        List<String> classic = new ArrayList<>(List.of("--gta4_native_max_queued_frames=4"));
        G925NativeDefaults.apply(classic, false);
        check(classic.size() == 1);
        List<String> nativeArgs = new ArrayList<>(List.of(
                "--gta4_native_max_queued_frames=4",
                "--gta4_native_anti_aliasing=smaa",
                "--android_performance_hint_target_ms=20",
                "--android_performance_hint=false"));
        G925NativeDefaults.apply(nativeArgs, true);
        check(nativeArgs.contains("--android_performance_hint_target_ms=20"));
        check(!nativeArgs.contains("--android_performance_hint_target_ms=16"));
        check(nativeArgs.contains("--android_performance_hint=false"));
        check(!nativeArgs.contains("--android_performance_hint=true"));
        check(nativeArgs.contains("--android_performance_hint_report_interval_ms=16"));
        check(nativeArgs.contains("--android_performance_hint_stable_threads=true"));
        check(nativeArgs.contains("--gta4_frame_wait_sleep=true"));
        check(nativeArgs.contains("--gta4_native_anti_aliasing=smaa"));
        check(nativeArgs.contains("--gta4_native_max_queued_frames=4"));
        int previous = nativeArgs.size();
        G925NativeDefaults.apply(nativeArgs, true);
        check(previous == nativeArgs.size());
        List<String> disabled = new ArrayList<>(List.of(
                "--no-android_performance_hint",
                "--gta4_frame_wait_sleep=false"));
        G925NativeDefaults.apply(disabled, true);
        check(disabled.contains("--no-android_performance_hint"));
        check(!disabled.contains("--android_performance_hint=true"));
        check(disabled.contains("--gta4_frame_wait_sleep=false"));
        check(!disabled.contains("--gta4_frame_wait_sleep=true"));
        for (String arg : disabled) {
            check(!arg.contains("shadow") && !arg.contains("upscale"));
        }
        System.out.println("PASS: G925 Native launcher preserves user settings and graphics");
    }
}

package com.libertyrecomp;

import java.util.List;

public final class G925FullQualityCvarMergeTest {
    public static void main(String[] args) {
        String[][] cvars = {
                {"gta4_frame_limit", "60"},
                {"gta4_extra_frames_in_flight", "1"},
                {"android_performance_hint_target_ms", "16"},
                {"android_performance_hint_report_interval_ms", "16"},
                {"android_performance_hint_stable_threads", "true"},
                {"android_performance_hint", "true"},
        };
        List<String> graphics = List.of(
                "# Original visuals",
                "--gta4_output_height_cap=1440",
                "--gta4_draw_distance_scale=1.2",
                "--gta4_traffic_density_scale=1.0",
                "--gta4_shadow_map_base_size=512",
                "--gta4_reflection_aa=on",
                "--custom_visual_quality=ultra",
                "  --gta4_frame_limit=30",
                "--no-android_performance_hint",
                "--android_performance_hint_target_ms=25",
                "--gta4_frame_limit=40",
                "--android_performance_hint_report_interval_ms=100",
                "--gta4_frame_limit_extra=keep",
                "# Unknown visual controls must survive untouched",
                "");
        String output = G925FullQualityCvarMerge.merge(graphics, cvars);
        for (String visual : graphics.subList(0, 7))
            check(output.contains(visual + "\n"), "lost visual: " + visual);
        check(output.contains("--gta4_frame_limit_extra=keep"), "prefix collision");
        check(output.contains("# Unknown visual controls must survive untouched"), "lost comment");
        check(!output.contains("--gta4_frame_limit=30"), "old FPS cap remains");
        check(!output.contains("--gta4_frame_limit=40"), "duplicate FPS cap remains");
        check(!output.contains("--no-android_performance_hint"), "conflicting boolean remains");
        check(!output.contains("--android_performance_hint_target_ms=25"), "old CPU target remains");
        for (String[] item : cvars) {
            String keyValue = "--" + item[0] + "=" + item[1] + "\n";
            check(output.indexOf(keyValue) == output.lastIndexOf(keyValue),
                    "duplicate CPU cvar: " + keyValue);
            check(output.contains(keyValue), "missing CPU cvar: " + keyValue);
        }
        String second = G925FullQualityCvarMerge.merge(List.of(output.split("\n")), cvars);
        check(second.equals(output), "not idempotent");
        System.out.println("PASS: full-quality CPU cvar merge keeps all visual settings");
    }

    private static void check(boolean result, String message) {
        if (!result) throw new AssertionError(message);
    }
}

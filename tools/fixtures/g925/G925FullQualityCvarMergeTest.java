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
        String[][] recoverQuality = {
                {"gta4_dynamic_draw_distance", "false"},
                {"gta4_draw_distance_scale", "1.0"},
                {"gta4_fps_guard", "0"}
        };
        List<String> degraded = List.of(
                "# Preserve custom visual and performance options",
                "--gta4_dynamic_draw_distance=true",
                "--gta4_draw_distance_scale=0.6",
                "--gta4_fps_guard=2",
                "--gta4_fps_guard=3",
                "--gta4_output_height_cap=1080",
                "--gta4_native_anti_aliasing=smaa",
                "--gta4_shadow_map_base_size=512",
                "--gta4_traffic_density_scale=1.0",
                "--gta4_extra_frames_in_flight=4",
                "--gta4_dynamic_draw_distance_min=0.80",
                "--custom_visual_quality=ultra",
                "--gta4_fps_guard_extra=do_not_touch");
        String recovered = G925FullQualityCvarMerge.merge(degraded, recoverQuality);
        for (String[] restored : recoverQuality) {
            String line = "--" + restored[0] + "=" + restored[1] + "\n";
            check(recovered.contains(line), "missing restored visual cvar: " + line);
            check(recovered.indexOf(line) == recovered.lastIndexOf(line), "duplicate: " + line);
        }
        for (String previous : List.of(
                "--gta4_dynamic_draw_distance=true",
                "--gta4_draw_distance_scale=0.6",
                "--gta4_fps_guard=2",
                "--gta4_fps_guard=3"))
            check(!recovered.contains(previous + "\n"), "degraded visual remains: " + previous);
        for (String unchanged : degraded.subList(5, degraded.size()))
            check(recovered.contains(unchanged + "\n"), "user choice lost: " + unchanged);
        check(recovered.contains("# Preserve custom visual and performance options\n"),
                "comment lost");
        check(recovered.equals(G925FullQualityCvarMerge.merge(
                List.of(recovered.split("\n")), recoverQuality)), "visual restore not idempotent");
        System.out.println("PASS: CPU boost and explicit visual restore preserve every other option");
    }

    private static void check(boolean result, String message) {
        if (!result) throw new AssertionError(message);
    }
}

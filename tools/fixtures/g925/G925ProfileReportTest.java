package com.libertyrecomp;
import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public final class G925ProfileReportTest {
    private static void check(boolean passed) {
        if (!passed) throw new AssertionError("G925 report regression");
    }
    public static void main(String[] args) throws Exception {
        File root = new File(args[0]);
        Path nativeDir = root.toPath().resolve("LibertyRecomp");
        Files.createDirectories(nativeDir);
        Files.writeString(nativeDir.resolve("native.toml"),
                "android_performance_hint_target_ms = 20\n"
                + "android_performance_hint_report_interval_ms = 100\n"
                + "android_thermal_guard = true\n"
                + "private_path = \"PRIVATE_SENTINEL\"\n");
        Files.writeString(root.toPath().resolve("args.txt"),
                "--android_performance_hint_report_interval_ms=25\n--gta4_dynamic_draw_distance=true\n");
        Files.writeString(root.toPath().resolve("graphics.txt"),
                "--android_performance_hint_report_interval_ms=16\n");
        String fallback = G925NativeProfileReport.captureSettings(root);
        check(fallback.contains("CPU feedback interval (ms): 16"));
        check(fallback.contains("Quality note:"));
        String applied = G925NativeProfileReport.captureAppliedSettings(root, List.of(
                "--android_performance_hint_report_interval_ms=16",
                "--android_performance_hint_stable_threads=true",
                "--android_thermal_guard=false", "--gta4_frame_limit=60",
                "--private_path=PRIVATE_SENTINEL", "--gta4_dynamic_draw_distance=false",
                "--gta4_fps_guard=0", "--gta4_output_height_cap=/PRIVATE_SENTINEL",
                "bad", "--", "--=bad"));
        check(applied.contains("CPU feedback interval (ms): 16"));
        check(applied.contains("CPU hint target (ms): 20"));
        check(applied.contains("Stable CPU hint threads: true"));
        check(applied.contains("App heat guard (reduces draw distance): false"));
        check(!applied.contains("Quality note:"));
        check(!applied.contains("PRIVATE_SENTINEL"));
        check(G925NativeProfileReport.captureAppliedSettings(root, List.of("--gta4_fps_guard=2"))
                .contains("Quality note:"));
        Path diagnostics = nativeDir.resolve("Diagnostics");
        Files.createDirectories(diagnostics);
        Path csv = diagnostics.resolve("native-performance-latest.csv");
        Files.writeString(csv, "frame,gpu_frame_ms,cpu_frame_interval_ms\n1,20,40\n2,,50\n3,30,60\n");
        String report = G925NativeProfileReport.summarize(root, 0, applied);
        check(report.contains("Samples: 3"));
        check(report.contains("GPU frame: avg=25.00 p95=30.00 max=30.00 (n=2)"));
        check(report.contains("Title frame interval: avg=50.00 p95=60.00 max=60.00 (n=3)"));
        check(report.contains("GPU fence wait (CPU): unavailable"));
        check(G925NativeProfileReport.summarize(root, csv.toFile().lastModified() + 10000, applied) == null);
        check(G925NativeProfileReport.summarize(root, Long.MAX_VALUE, applied) == null);
        System.out.println("PASS: final launch precedence, saved settings, quality warnings, privacy whitelist, CSV and stale capture");
    }
}

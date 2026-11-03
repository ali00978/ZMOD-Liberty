package com.libertyrecomp;
import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public final class G925ProfileReportTest {
    private static void check(boolean passed) {
        if (!passed) throw new AssertionError("G925 report regression");
    }
    private static void publishInGameProcess(File root, long started, int interval) throws Exception {
        Process writer = new ProcessBuilder(
                Path.of(System.getProperty("java.home"), "bin", "java").toString(),
                "-cp", System.getProperty("java.class.path"),
                G925ProfileReportTest.class.getName(), "--write-snapshot",
                root.getAbsolutePath(), Long.toString(started), Integer.toString(interval))
                .inheritIO().start();
        check(writer.waitFor() == 0);
    }
    public static void main(String[] args) throws Exception {
        if (args.length == 4 && "--write-snapshot".equals(args[0])) {
            G925NativeProfileReport.saveAppliedSettings(new File(args[1]), Long.parseLong(args[2]),
                    List.of("--android_performance_hint_report_interval_ms=" + args[3],
                            "--gta4_frame_limit=60"));
            return;
        }
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
        long started = 1234567890000L;
        publishInGameProcess(root, started, 16);
        String report = G925NativeProfileReport.summarize(root, started, "STALE_PROCESS_CACHE");
        check(report.contains("CPU feedback interval (ms): 16"));
        check(!report.contains("STALE_PROCESS_CACHE"));
        Path snapshot = root.toPath().resolve("g925-native-launch-settings.txt");
        // Coarse file timestamps must not hide a valid same-session snapshot.
        Files.setLastModifiedTime(snapshot, java.nio.file.attribute.FileTime.fromMillis(1));
        check(G925NativeProfileReport.summarize(root, started, "STALE_PROCESS_CACHE")
                .contains("CPU feedback interval (ms): 16"));
        // Even a fresh timestamp must not admit settings from another launch.
        Files.setLastModifiedTime(snapshot, java.nio.file.attribute.FileTime.fromMillis(
                System.currentTimeMillis() + 86400000L));
        String olderSnapshot = G925NativeProfileReport.summarize(root, started + 1, "LABELED_FALLBACK");
        check(olderSnapshot.contains("LABELED_FALLBACK"));
        check(!olderSnapshot.contains("final applied overrides"));
        publishInGameProcess(root, started + 1, 25);
        check(G925NativeProfileReport.summarize(root, started + 1, "STALE_PROCESS_CACHE")
                .contains("CPU feedback interval (ms): 25"));
        Files.writeString(snapshot, "capture_started=" + (started + 1) + "\n");
        check(G925NativeProfileReport.summarize(root, started + 1, "LABELED_FALLBACK")
                .contains("LABELED_FALLBACK"));
        Files.writeString(snapshot, "x".repeat(16385));
        check(G925NativeProfileReport.summarize(root, started + 1, "LABELED_FALLBACK")
                .contains("LABELED_FALLBACK"));
        Files.delete(snapshot);
        Files.createDirectory(snapshot);
        try {
            G925NativeProfileReport.saveAppliedSettings(root, started + 1, List.of());
            throw new AssertionError("Replacing a directory must fail");
        } catch (java.io.IOException expected) {
            try (var files = Files.list(root.toPath())) {
                check(files.noneMatch(p -> p.getFileName().toString().startsWith("g925-launch-")));
            }
        }
        Files.delete(snapshot);
        check(G925NativeProfileReport.summarize(root, started + 1, "LABELED_FALLBACK")
                .contains("LABELED_FALLBACK"));
        check(report.contains("Samples: 3"));
        check(report.contains("GPU frame: avg=25.00 p95=30.00 max=30.00 (n=2)"));
        check(report.contains("Title frame interval: avg=50.00 p95=60.00 max=60.00 (n=3)"));
        check(report.contains("GPU fence wait (CPU): unavailable"));
        check(G925NativeProfileReport.summarize(root, csv.toFile().lastModified() + 10000, applied) == null);
        check(G925NativeProfileReport.summarize(root, Long.MAX_VALUE, applied) == null);
        System.out.println("PASS: separate-process snapshots, exact launch identity, atomic replacement, bounded reads, precedence, privacy, CSV and stale capture");
    }
}

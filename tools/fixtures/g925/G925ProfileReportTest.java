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
        String currentSnapshot = Files.readString(snapshot);
        // Reproduce the upgrade bug: old measurements and launch preference,
        // freshly touched CSV, but an old/missing captured build identity.
        Files.setLastModifiedTime(csv, java.nio.file.attribute.FileTime.fromMillis(
                System.currentTimeMillis() + 86400000L));
        Files.writeString(snapshot, currentSnapshot.replace(
                "capture_build=" + BuildConfig.VERSION_NAME, "capture_build=older-app"));
        check(G925NativeProfileReport.summarize(root, started, "CURRENT_LAUNCHER_SETTINGS") == null);
        Files.writeString(snapshot, currentSnapshot.replace(
                "capture_build=" + BuildConfig.VERSION_NAME + "\n", ""));
        check(G925NativeProfileReport.summarize(root, started, "CURRENT_LAUNCHER_SETTINGS") == null);
        Files.writeString(snapshot, currentSnapshot);
        check(report.contains("Capture started (UTC): " + java.time.Instant.ofEpochMilli(started)));
        // Coarse file timestamps must not hide a valid same-session snapshot.
        Files.setLastModifiedTime(snapshot, java.nio.file.attribute.FileTime.fromMillis(1));
        check(G925NativeProfileReport.summarize(root, started, "STALE_PROCESS_CACHE")
                .contains("CPU feedback interval (ms): 16"));
        // Even a fresh timestamp must not admit settings from another launch.
        Files.setLastModifiedTime(snapshot, java.nio.file.attribute.FileTime.fromMillis(
                System.currentTimeMillis() + 86400000L));
        String olderSnapshot = G925NativeProfileReport.summarize(root, started + 1, "LABELED_FALLBACK");
        check(olderSnapshot == null);
        publishInGameProcess(root, started + 1, 25);
        check(G925NativeProfileReport.summarize(root, started + 1, "STALE_PROCESS_CACHE")
                .contains("CPU feedback interval (ms): 25"));
        Files.writeString(snapshot, "capture_started=" + (started + 1) + "\n");
        check(G925NativeProfileReport.summarize(root, started + 1, "LABELED_FALLBACK") == null);
        Files.writeString(snapshot, "x".repeat(16385));
        check(G925NativeProfileReport.summarize(root, started + 1, "LABELED_FALLBACK") == null);
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
        check(G925NativeProfileReport.summarize(root, started + 1, "LABELED_FALLBACK") == null);
        check(report.contains("Samples: 3"));
        check(report.contains("GPU frame: avg=25.00 p95=30.00 max=30.00 (n=2)"));
        check(report.contains("Title frame interval: avg=50.00 p95=60.00 max=60.00 (n=3)"));
        check(report.contains("GPU fence wait (CPU): unavailable"));
        check(G925NativeProfileReport.summarize(root, csv.toFile().lastModified() + 10000, applied) == null);
        check(G925NativeProfileReport.summarize(root, Long.MAX_VALUE, applied) == null);
        check(G925NativeProfileReport.summarize(root, 0, applied) == null);
        // Detail must belong to exactly the same frame/sequence identities.
        publishInGameProcess(root, started, 16);
        Files.writeString(csv, "frame,capture_sequence,gpu_frame_ms,cpu_frame_interval_ms\n"
                + "1,7,20,40\n2,8,,50\n3,9,30,60\n");
        Path cpu = diagnostics.resolve("native-performance-cpu.csv");
        String header = "frame,sequence,phase,operation,calls,inclusive_ms,self_ms,max_call_ms\n";
        String details = "1,7,record,record-command,10,30,6,5\n"
                + "1,7,record,surface-image-lookup,20,8,8,1\n"
                + "2,8,record,record-command,20,40,9,6\n"
                + "3,9,record,record-command,30,50,12,7\n"
                + "999,999,record,unrelated-capture,1,999,999,999\n";
        long detailTime = System.currentTimeMillis();
        Files.writeString(cpu, header + details);
        Files.setLastModifiedTime(cpu, java.nio.file.attribute.FileTime.fromMillis(detailTime));
        Files.setLastModifiedTime(csv, java.nio.file.attribute.FileTime.fromMillis(detailTime - 1));
        String detailed = G925NativeProfileReport.summarize(root, started, applied);
        check(detailed.contains("record / record-command: self=9.000 ms calls/frame=20.0 peak-call=7.000 ms"));
        check(detailed.contains("record / surface-image-lookup: self=2.667 ms calls/frame=6.7 peak-call=1.000 ms"));
        check(!detailed.contains("unrelated-capture"));
        check(detailed.contains("n=3/3):"));
        // The production delayed trigger arms after the first callback's CPU
        // recorder decision. Keep frames 2/3, with denominator 2 (never 3).
        String partial = details.lines().filter(line -> !line.startsWith("1,7,"))
                .collect(java.util.stream.Collectors.joining("\n", "", "\n"));
        Files.writeString(cpu, header + partial);
        String subset = G925NativeProfileReport.summarize(root, started, applied);
        check(subset.contains("n=2/3):"));
        check(subset.contains("CPU detail missing for 1 captured frame(s)"));
        check(subset.contains("self=10.500 ms calls/frame=25.0 peak-call=7.000 ms"));
        check(!subset.contains("unrelated-capture"));
        Files.writeString(cpu, header + "999,999,record,unrelated-capture,1,999,999,999\n");
        check(G925NativeProfileReport.summarize(root, started, applied)
                .contains("CPU operation detail: unavailable"));
        for (String bad : List.of(
                details + "1,7,record,record-command,10,30,6,5\n",
                details.replace(",30,6,", ",30,NaN,"),
                details.replace(",30,6,", ",30,,"),
                details.replace(",30,6,", ",30,-1,"),
                details.replace(",30,6,", ",30,31,"),
                details.replace("record-command", "PRIVATE/SENTINEL"))) {
            Files.writeString(cpu, header + bad);
            Files.setLastModifiedTime(cpu, java.nio.file.attribute.FileTime.fromMillis(detailTime));
            String invalid = G925NativeProfileReport.summarize(root, started, applied);
            check(invalid.contains("CPU operation detail: unavailable"));
            check(invalid.contains("GPU frame: avg=25.00"));
            check(!invalid.contains("PRIVATE/SENTINEL"));
        }
        Files.writeString(cpu, header + details);
        Files.setLastModifiedTime(cpu, java.nio.file.attribute.FileTime.fromMillis(started - 1));
        check(G925NativeProfileReport.summarize(root, started, applied)
                .contains("CPU operation detail: unavailable"));
        // The primary CSV is written before detail, then renamed last. Its
        // mtime may precede valid detail even though publication is atomic.
        Files.setLastModifiedTime(cpu, java.nio.file.attribute.FileTime.fromMillis(detailTime + 2));
        check(G925NativeProfileReport.summarize(root, started, applied)
                .contains("Top CPU operations"));
        Files.writeString(cpu, header + "x".repeat(2049));
        Files.setLastModifiedTime(cpu, java.nio.file.attribute.FileTime.fromMillis(detailTime));
        check(G925NativeProfileReport.summarize(root, started, applied)
                .contains("CPU operation detail: unavailable"));
        // Fresh captures remove only the old report inputs, including files
        // with future timestamps. Config and unrelated diagnostics are kept.
        Path keep = diagnostics.resolve("native-performance-latest.log");
        Files.writeString(keep, "keep old diagnostic log");
        String config = Files.readString(nativeDir.resolve("native.toml"));
        Files.setLastModifiedTime(csv, java.nio.file.attribute.FileTime.fromMillis(
                System.currentTimeMillis() + 86400000L));
        long fresh = G925NativeProfileReport.beginCapture(root);
        check(fresh > 0 && !Files.exists(csv) && !Files.exists(cpu) && !Files.exists(snapshot));
        check(Files.readString(keep).equals("keep old diagnostic log"));
        check(Files.readString(nativeDir.resolve("native.toml")).equals(config));
        check(G925NativeProfileReport.summarize(root, fresh, applied) == null);
        publishInGameProcess(root, fresh, 16);
        check(G925NativeProfileReport.summarize(root, fresh, applied) == null);
        Files.writeString(csv, "frame,gpu_frame_ms,cpu_frame_interval_ms\n1,18,35\n");
        Files.setLastModifiedTime(csv, java.nio.file.attribute.FileTime.fromMillis(fresh + 1000));
        check(G925NativeProfileReport.summarize(root, fresh, applied)
                .contains("GPU frame: avg=18.00"));
        Files.delete(csv);
        Files.createDirectory(csv);
        Files.writeString(csv.resolve("keep.txt"), "not a capture file");
        try {
            G925NativeProfileReport.beginCapture(root);
            throw new AssertionError("Invalid capture input must abort preparation");
        } catch (java.io.IOException expected) {
            check(Files.exists(csv.resolve("keep.txt")));
        }
        System.out.println("PASS: missing first CPU detail frame excluded with explicit coverage; matching, invalid and duplicate rows checked");
        System.out.println("PASS: old-build/missing-build captures rejected; fresh capture cleanup, failure and new completion checked");
        System.out.println("PASS: separate-process snapshots, exact launch identity, atomic replacement, bounded reads, precedence, privacy, CSV and stale capture");
    }
}

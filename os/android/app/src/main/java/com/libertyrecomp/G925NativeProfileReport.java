package com.libertyrecomp;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;

/** Read-only, bounded native profiling summary; never reads game files. */
final class G925NativeProfileReport {
    private G925NativeProfileReport() {}

    private static final String[][] METRICS = {
        {"gpu_frame_ms", "GPU frame"},
        {"cpu_frame_interval_ms", "Title frame interval"},
        {"cpu_fence_wait_ms", "GPU fence wait (CPU)"},
        {"cpu_command_recording_ms", "Native command recording"},
        {"counter_surface_lookup_reuses", "Surface lookup reuses / frame"},
        {"counter_vertex_layout_reuses", "Vertex layout reuses / frame"},
        {"counter_converted_index_upload_reuses", "Converted index uploads reused / frame"},
        {"counter_plain_index_uploads_skipped", "Unused original index uploads skipped / frame"},
        {"cpu_command_finalize_ms", "Native command finalize"},
        {"cpu_texture_preparation_ms", "Native texture preparation"},
        {"cpu_render_callback_ms", "Native render callback"},
        {"cpu_housekeeping_ms", "Native housekeeping"},
        {"cpu_queue_submit_ms", "Native queue submit (CPU)"},
        {"cpu_pipeline_wait_ms", "Pipeline wait"},
        {"cpu_pipeline_compile_jobs_ms", "Pipeline compile work"},
        {"cpu_outside_renderer_ms", "Outside renderer"},
        {"cpu_presenter_total_ms", "Presenter (CPU)"},
        {"counter_pipeline_creates", "Pipelines created / frame"},
        {"counter_texture_upload_bytes", "Texture bytes / frame"}
    };

    // Snapshot explicit performance settings at launch, not when sharing.
    // Read only whitelisted keys; never include paths or device identifiers.
    private static final String[][] CAPTURE_SETTINGS = {
        {"android_performance_hint", "Android performance hints"},
        {"android_performance_hint_target_ms", "CPU hint target (ms)"},
        {"android_performance_hint_report_interval_ms", "CPU feedback interval (ms)"},
        {"android_performance_hint_stable_threads", "Stable CPU hint threads"},
        {"gta4_frame_limit", "Launcher frame cap"},
        {"gta4_output_height_cap", "Launcher output height"},
        {"gta4_native_anti_aliasing", "Launcher anti-aliasing"},
        {"gta4_draw_distance_scale", "Draw distance scale"},
        {"gta4_dynamic_draw_distance", "Automatic draw distance reduction"},
        {"gta4_dynamic_draw_distance_min", "Automatic draw distance floor"},
        {"android_thermal_guard", "App heat guard (reduces draw distance)"},
        {"gta4_fps_guard", "FPS guard (reduces reflection frequency)"},
        {"gta4_native_shadow_interval", "Shadow refresh interval"},
        {"gta4_native_water_reflection_interval", "Water reflection refresh interval"},
        {"gta4_native_environment_reflection_interval", "Environment reflection refresh interval"}
    };

    private static void readSetting(Map<String, String> values, String line, boolean argument) {
        line = line.trim();
        int comment = line.indexOf('#');
        if (comment >= 0) line = line.substring(0, comment).trim();
        int equals = line.indexOf('=');
        if (equals <= 0 || (argument && !line.startsWith("--"))) return;
        String key = line.substring(argument ? 2 : 0, equals).trim().replace("\"", "");
        String value = line.substring(equals + 1).trim().replace("\"", "");
        for (String[] setting : CAPTURE_SETTINGS) {
            if (setting[0].equals(key) && value.matches("[a-zA-Z0-9_.-]{1,32}")) {
                values.put(key, value);
                return;
            }
        }
    }

    private static void readSettingsFile(Map<String, String> values, File file, boolean argument) {
        if (!file.isFile()) return;
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                new FileInputStream(file), StandardCharsets.UTF_8))) {
            String line;
            int lines = 0;
            while (lines++ < 2000 && (line = reader.readLine()) != null) {
                readSetting(values, line, argument);
            }
        } catch (IOException ignored) {
            // A missing config must not prevent launching or sharing the capture.
        }
    }

    static String captureSettings(File root) {
        Map<String, String> values = new LinkedHashMap<>();
        if (root != null) {
            for (String fileName : new String[] {"args.txt", "graphics.txt"}) {
                readSettingsFile(values, new File(root, fileName), true);
            }
        }
        return formatSettings(values, "Launch-time config (explicit launcher/args overrides only):");
    }

    // Called AFTER performance-test Intent overrides and option deduplication.
    // Saved native values are lower priority than these actual launch arguments.
    static String captureAppliedSettings(File root, List<String> arguments) {
        Map<String, String> values = new LinkedHashMap<>();
        if (root != null) {
            readSettingsFile(values, new File(new File(root, "LibertyRecomp"), "native.toml"), false);
        }
        for (String argument : arguments) readSetting(values, argument, true);
        return formatSettings(values,
                "Launch-time config (saved native config + final applied overrides):");
    }

    private static String formatSettings(Map<String, String> values, String heading) {
        StringBuilder snapshot = new StringBuilder();
        snapshot.append(heading).append('\n');
        for (String[] setting : CAPTURE_SETTINGS) {
            snapshot.append(setting[1]).append(": ")
                    .append(values.getOrDefault(setting[0], "default / in-game"))
                    .append('\n');
        }
        if ("true".equals(values.get("gta4_dynamic_draw_distance"))
                || "true".equals(values.get("android_thermal_guard"))
                || Arrays.asList("1", "2", "3").contains(values.get("gta4_fps_guard"))) {
            snapshot.append("Quality note: automatic reductions are enabled; ")
                    .append("draw distance or reflection frequency may change during this run.\n");
        }
        snapshot.append("16 ms CPU feedback is NOT an FPS cap; in-game options may differ.\n");
        return snapshot.toString();
    }

    // The game and launcher use different Android processes. SharedPreferences
    // caches are process-local, so publish the final snapshot through an atomic
    // file replacement and read it fresh only for this capture.
    private static final String APPLIED_SETTINGS_FILE = "g925-native-launch-settings.txt";
    private static final int MAX_SETTINGS_BYTES = 16384;

    // Called after stopping the previous game process and before starting a
    // capture. A failed/new capture must never expose a previous capture's CSV.
    static long beginCapture(File root) throws IOException {
        if (root == null || !root.isDirectory()) throw new IOException("Missing capture root");
        File diagnostics = new File(new File(root, "LibertyRecomp"), "Diagnostics");
        for (File file : new File[] {new File(root, APPLIED_SETTINGS_FILE),
                new File(diagnostics, "native-performance-latest.csv"),
                new File(diagnostics, "native-performance-cpu.csv")}) {
            if (file.isDirectory()) throw new IOException("Invalid capture file");
            Files.deleteIfExists(file.toPath());
        }
        return System.currentTimeMillis();
    }

    private static String captureHeader(long started) {
        return "capture_started=" + started + '\n'
                + "capture_build=" + BuildConfig.VERSION_NAME + '\n';
    }

    static void saveAppliedSettings(File root, long captureStartedAt, List<String> arguments)
            throws IOException {
        if (root == null || captureStartedAt <= 0 || captureStartedAt == Long.MAX_VALUE) return;
        File temporary = File.createTempFile("g925-launch-", ".tmp", root);
        try {
            String snapshot = captureHeader(captureStartedAt)
                    + captureAppliedSettings(root, arguments);
            Files.write(temporary.toPath(), snapshot.getBytes(StandardCharsets.UTF_8));
            Files.move(temporary.toPath(), new File(root, APPLIED_SETTINGS_FILE).toPath(),
                    StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        } finally {
            temporary.delete();
        }
    }

    private static String readAppliedSettings(File root, long captureStartedAt) throws IOException {
        // File mtimes on emulated/shared storage can be rounded. Match the exact
        // launch token, not the snapshot's mtime, and bound the actual read too.
        File snapshot = new File(root, APPLIED_SETTINGS_FILE);
        if (!snapshot.isFile() || snapshot.length() > MAX_SETTINGS_BYTES) return null;
        byte[] bytes = new byte[MAX_SETTINGS_BYTES + 1];
        int used = 0;
        try (FileInputStream input = new FileInputStream(snapshot)) {
            while (used < bytes.length) {
                int count = input.read(bytes, used, bytes.length - used);
                if (count < 0) break;
                used += count;
            }
        }
        if (used > MAX_SETTINGS_BYTES) return null;
        String content = new String(bytes, 0, used, StandardCharsets.UTF_8);
        String header = captureHeader(captureStartedAt);
        if (!content.startsWith(header)) return null;
        String settings = content.substring(header.length());
        return settings.startsWith("Launch-time config (") && settings.endsWith("\n")
                ? settings : null;
    }

    static String summarize(File dataRoot, long captureStartedAt, String launchSettings)
            throws IOException {
        if (dataRoot == null || captureStartedAt <= 0 || captureStartedAt == Long.MAX_VALUE) return null;
        File csv = new File(new File(new File(dataRoot, "LibertyRecomp"),
                "Diagnostics"), "native-performance-latest.csv");
        if (!csv.isFile() || csv.lastModified() < captureStartedAt
                || csv.length() > 16 * 1024 * 1024) return null;
        try {
            // A stored launcher preference survives app upgrades. Require the
            // game process's exact launch AND build identity before labeling CSV
            // measurements with this version. The old fallback cannot prove it.
            launchSettings = readAppliedSettings(dataRoot, captureStartedAt);
            if (launchSettings == null) return null;
        } catch (IOException ignored) {
            return null;
        }
        List<Double>[] metrics = new List[METRICS.length];
        for (int i = 0; i < metrics.length; ++i) metrics[i] = new ArrayList<>();
        ArrayList<String> slowFrames = new ArrayList<>();
        Set<String> captureFrames = new HashSet<>();
        int count = 0;
        String[] headers;
        int[] columns;
        int frameColumn;
        int sequenceColumn;
        int intervalColumn;
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                new FileInputStream(csv), StandardCharsets.UTF_8))) {
            String line = reader.readLine();
            if (line == null) return null;
            headers = line.split(",", -1);
            columns = new int[METRICS.length];
            for (int i = 0; i < METRICS.length; ++i) {
                columns[i] = Arrays.asList(headers).indexOf(METRICS[i][0]);
            }
            frameColumn = Arrays.asList(headers).indexOf("frame");
            sequenceColumn = Arrays.asList(headers).indexOf("capture_sequence");
            intervalColumn = Arrays.asList(headers).indexOf("cpu_frame_interval_ms");
            if (frameColumn < 0 || intervalColumn < 0
                    || Arrays.asList(headers).indexOf("gpu_frame_ms") < 0) return null;
            while ((line = reader.readLine()) != null && count < 600) {
                String[] values = line.split(",", -1);
                if (values.length != headers.length) continue;
                ++count;
                String frameKey = captureKey(values, frameColumn, sequenceColumn);
                if (frameKey != null) captureFrames.add(frameKey);
                for (int i = 0; i < METRICS.length; ++i) {
                    double value = number(values, columns[i]);
                    if (Double.isFinite(value)) metrics[i].add(value);
                }
                double interval = number(values, intervalColumn);
                if (Double.isFinite(interval) && interval > 0) {
                    String frame = values[frameColumn].replaceAll("[^0-9]", "");
                    slowFrames.add(String.format(Locale.US,
                            "%010.3f ms | frame %s", interval, frame));
                }
            }
        }
        if (count == 0) return null;
        StringBuilder result = new StringBuilder();
        result.append("ZMOD Liberty G925 NATIVE CPU/GPU bottleneck analysis\n")
                .append("One-shot late capture, 65s after first rendered frame.\n")
                .append("Profiling can add timing overhead; this is not a normal FPS test.\n")
                .append("GPU frame time is a Vulkan timestamp if supported. ")
                .append("Blank measurements are NOT zero.\n")
                .append("CPU ranges may overlap: do not sum them.\n")
                .append("Build: ").append(BuildConfig.VERSION_NAME).append('\n')
                .append("Capture started (UTC): ").append(Instant.ofEpochMilli(captureStartedAt))
                .append('\n')
                .append("Samples: ").append(count).append("\n");
        if (launchSettings != null && !launchSettings.isEmpty()) {
            result.append(launchSettings);
        }
        result.append('\n');
        for (int i = 0; i < METRICS.length; ++i) {
            List<Double> values = metrics[i];
            if (values.isEmpty()) {
                result.append(METRICS[i][1]).append(": unavailable\n");
                continue;
            }
            Collections.sort(values);
            double total = 0;
            for (double v : values) total += v;
            double mean = total / values.size();
            int rank = (int) Math.ceil(values.size() * .95) - 1;
            double p95 = values.get(Math.max(0, rank));
            double max = values.get(values.size() - 1);
            result.append(String.format(Locale.US, "%s: avg=%.2f p95=%.2f max=%.2f "
                    + "(n=%d)\n", METRICS[i][1], mean, p95, max, values.size()));
        }
        String detail = null;
        if (captureFrames.size() == count) {
            try {
                detail = summarizeCpuOperations(csv, captureStartedAt, captureFrames);
            } catch (IOException ignored) {
                // The valid top-level capture remains usable without optional detail.
            }
        }
        result.append(detail != null ? detail : "\nCPU operation detail: unavailable\n");
        slowFrames.sort(Collections.reverseOrder());
        result.append("\nSlowest sampled game intervals:\n");
        for (int i = 0; i < Math.min(8, slowFrames.size()); ++i) {
            result.append(slowFrames.get(i)).append('\n');
        }
        result.append("\nInterpretation: a slow GPU frame with low CPU recording ")
                .append("points toward native GPU work; high CPU recording or fence ")
                .append("wait points to a different stage. Both may coexist.\n");
        return result.toString();
    }

    private static String captureKey(String[] values, int frame, int sequence) {
        if (frame < 0 || sequence < 0 || frame >= values.length || sequence >= values.length)
            return null;
        if (!values[frame].matches("[0-9]{1,20}") || !values[sequence].matches("[0-9]{1,20}"))
            return null;
        return values[frame] + "/" + values[sequence];
    }

    private static String summarizeCpuOperations(File capture, long started, Set<String> frames)
            throws IOException {
        File csv = new File(capture.getParentFile(), "native-performance-cpu.csv");
        if (!csv.isFile() || csv.lastModified() < started
                || csv.length() > 16 * 1024 * 1024)
            return null;
        Map<String, double[]> groups = new LinkedHashMap<>();
        Set<String> seenRows = new HashSet<>();
        Set<String> seenFrames = new HashSet<>();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                new FileInputStream(csv), StandardCharsets.UTF_8))) {
            String line = reader.readLine();
            if (line == null || line.length() > 2048) return null;
            List<String> headers = Arrays.asList(line.split(",", -1));
            int frame = headers.indexOf("frame"), sequence = headers.indexOf("sequence");
            int phase = headers.indexOf("phase"), operation = headers.indexOf("operation");
            int calls = headers.indexOf("calls"), self = headers.indexOf("self_ms");
            int inclusive = headers.indexOf("inclusive_ms"), peak = headers.indexOf("max_call_ms");
            if (frame < 0 || sequence < 0 || phase < 0 || operation < 0 || calls < 0
                    || self < 0 || inclusive < 0 || peak < 0) return null;
            int rows = 0;
            while ((line = reader.readLine()) != null) {
                if (++rows > 120000 || line.length() > 2048) return null;
                String[] values = line.split(",", -1);
                if (values.length != headers.size()) return null;
                String frameKey = captureKey(values, frame, sequence);
                if (frameKey == null || !frames.contains(frameKey)) continue;
                if (!values[phase].matches("[a-z0-9-]{1,64}")
                        || !values[operation].matches("[a-z0-9-]{1,64}")) return null;
                double selfMs = number(values, self), inclusiveMs = number(values, inclusive);
                double count = number(values, calls), peakMs = number(values, peak);
                if (!Double.isFinite(selfMs) || !Double.isFinite(inclusiveMs)
                        || !Double.isFinite(count) || !Double.isFinite(peakMs)
                        || selfMs < 0 || inclusiveMs < selfMs - 0.000001 || count <= 0
                        || count != Math.rint(count) || peakMs < 0) return null;
                String group = values[phase] + " / " + values[operation];
                if (!seenRows.add(frameKey + "/" + group)) return null;
                seenFrames.add(frameKey);
                double[] total = groups.computeIfAbsent(group, ignored -> new double[3]);
                if (groups.size() > 512) return null;
                total[0] += selfMs;
                total[1] += count;
                total[2] = Math.max(total[2], peakMs);
                if (!Double.isFinite(total[0]) || !Double.isFinite(total[1])) return null;
            }
        }
        // The delayed native trigger can arm inside the first callback, after
        // its CPU recorder decision. Keep the other measured frames and expose
        // coverage; missing frames never enter the denominator as zero cost.
        if (seenFrames.isEmpty() || groups.isEmpty()) return null;
        List<Map.Entry<String, double[]>> sorted = new ArrayList<>(groups.entrySet());
        sorted.sort((a, b) -> Double.compare(b.getValue()[0], a.getValue()[0]));
        StringBuilder result = new StringBuilder("\nTop CPU operations (self time excludes child scopes; "
                + "averages per frame with CPU detail, n=" + seenFrames.size() + "/"
                + frames.size() + "):\n");
        if (seenFrames.size() != frames.size()) {
            result.append("CPU detail missing for ").append(frames.size() - seenFrames.size())
                    .append(" captured frame(s); excluded from these averages.\n");
        }
        for (int i = 0; i < Math.min(8, sorted.size()); ++i) {
            Map.Entry<String, double[]> entry = sorted.get(i);
            double[] total = entry.getValue();
            result.append(String.format(Locale.US,
                    "%s: self=%.3f ms calls/frame=%.1f peak-call=%.3f ms\n",
                    entry.getKey(), total[0] / seenFrames.size(), total[1] / seenFrames.size(), total[2]));
        }
        return result.toString();
    }

    private static double number(String[] columns, int index) {
        if (index < 0 || index >= columns.length || columns[index].isEmpty())
            return Double.NaN;
        try {
            return Double.parseDouble(columns[index]);
        } catch (NumberFormatException e) {
            return Double.NaN;
        }
    }
}

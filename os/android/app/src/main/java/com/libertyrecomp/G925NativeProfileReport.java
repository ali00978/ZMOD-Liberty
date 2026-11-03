package com.libertyrecomp;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.LinkedHashMap;
import java.util.Map;

/** Read-only, bounded native profiling summary; never reads game files. */
final class G925NativeProfileReport {
    private G925NativeProfileReport() {}

    private static final String[][] METRICS = {
        {"gpu_frame_ms", "GPU frame"},
        {"cpu_frame_interval_ms", "Title frame interval"},
        {"cpu_fence_wait_ms", "GPU fence wait (CPU)"},
        {"cpu_command_recording_ms", "Native command recording"},
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

    // Snapshot explicit performance settings when Capture is tapped, not later
    // when the report is shared. Read only whitelisted keys from local launcher
    // configuration; never include paths, user content or device identifiers.
    private static final String[][] CAPTURE_SETTINGS = {
        {"android_performance_hint", "Android performance hints"},
        {"android_performance_hint_target_ms", "CPU hint target (ms)"},
        {"android_performance_hint_report_interval_ms", "CPU feedback interval (ms)"},
        {"android_performance_hint_stable_threads", "Stable CPU hint threads"},
        {"gta4_frame_limit", "Launcher frame cap"},
        {"gta4_output_height_cap", "Launcher output height"},
        {"gta4_native_anti_aliasing", "Launcher anti-aliasing"}
    };

    static String captureSettings(File root) {
        Map<String, String> values = new LinkedHashMap<>();
        if (root != null) {
            for (String fileName : new String[] {"args.txt", "graphics.txt"}) {
                File file = new File(root, fileName);
                if (!file.isFile()) continue;
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                        new FileInputStream(file), StandardCharsets.UTF_8))) {
                    String line;
                    int lines = 0;
                    while (lines++ < 2000 && (line = reader.readLine()) != null) {
                        line = line.trim();
                        int equals = line.indexOf('=');
                        if (!line.startsWith("--") || equals <= 2) continue;
                        String key = line.substring(2, equals);
                        boolean allowed = false;
                        for (String[] setting : CAPTURE_SETTINGS) {
                            if (setting[0].equals(key)) { allowed = true; break; }
                        }
                        if (!allowed) continue;
                        String value = line.substring(equals + 1).trim();
                        // Only small, simple settings values are shareable.
                        if (value.matches("[a-zA-Z0-9_.-]{1,32}")) values.put(key, value);
                    }
                } catch (IOException ignored) {
                    // The report still works when configuration is unavailable.
                }
            }
        }
        StringBuilder snapshot = new StringBuilder();
        snapshot.append("Launch-time config (explicit launcher/args overrides only):\n");
        for (String[] setting : CAPTURE_SETTINGS) {
            snapshot.append(setting[1]).append(": ")
                    .append(values.getOrDefault(setting[0], "default / in-game"))
                    .append('\n');
        }
        snapshot.append("16 ms CPU feedback is NOT an FPS cap; in-game options may differ.\n");
        return snapshot.toString();
    }

    static String summarize(File dataRoot, long captureStartedAt, String launchSettings)
            throws IOException {
        if (dataRoot == null || captureStartedAt == Long.MAX_VALUE) return null;
        File csv = new File(new File(new File(dataRoot, "LibertyRecomp"),
                "Diagnostics"), "native-performance-latest.csv");
        if (!csv.isFile() || csv.lastModified() < captureStartedAt) return null;
        List<Double>[] metrics = new List[METRICS.length];
        for (int i = 0; i < metrics.length; ++i) metrics[i] = new ArrayList<>();
        ArrayList<String> slowFrames = new ArrayList<>();
        int count = 0;
        String[] headers;
        int[] columns;
        int frameColumn;
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
            intervalColumn = Arrays.asList(headers).indexOf("cpu_frame_interval_ms");
            if (frameColumn < 0 || intervalColumn < 0
                    || Arrays.asList(headers).indexOf("gpu_frame_ms") < 0) return null;
            while ((line = reader.readLine()) != null && count < 600) {
                String[] values = line.split(",", -1);
                if (values.length != headers.length) continue;
                ++count;
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

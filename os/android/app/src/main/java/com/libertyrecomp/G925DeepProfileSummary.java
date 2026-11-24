package com.libertyrecomp;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Read-only detailed frame diagnostics. Never uploads or reads game data,
 * identifiers, filesystem paths or unrecognized CSV text.
 *
 * Native CSVs are only consulted when the caller has already verified the
 * main capture's launch/build token and supplies the exact frame/sequence set.
 */
final class G925DeepProfileSummary {
    private G925DeepProfileSummary() {}

    private static final int MAX_FILE_BYTES = 16 * 1024 * 1024;
    private static final int MAX_ROWS = 120_000;
    private static final int MAX_ROW_LENGTH = 2048;
    private static final int MAX_GROUPS = 8192;

    static String summarize(File captureDir, long captureStarted, Set<String> frameKeys) {
        if (captureDir == null || frameKeys == null || frameKeys.isEmpty()) return "";
        StringBuilder text = new StringBuilder();
        text.append(transport(captureDir, captureStarted, frameKeys));
        text.append(gpu(captureDir, captureStarted, frameKeys));
        text.append(shaders(captureDir, captureStarted, frameKeys));
        return text.toString();
    }

    private interface RowConsumer {
        boolean accept(String[] fields, Map<String, Integer> columns, String frameKey);
    }

    private static int col(Map<String, Integer> columns, String name) {
        Integer i = columns.get(name);
        return i == null ? -1 : i;
    }

    private static String cell(String[] fields, Map<String, Integer> cols, String name) {
        int i = col(cols, name);
        return i < 0 || i >= fields.length ? "" : fields[i];
    }

    private static double decimal(String text) {
        if (text.isEmpty()) return Double.NaN;
        try {
            double n = Double.parseDouble(text);
            return Double.isFinite(n) && n >= 0 && n <= 1e10 ? n : Double.NaN;
        } catch (NumberFormatException ignored) {
            return Double.NaN;
        }
    }

    private static boolean safeLabel(String label) {
        return label.matches("[a-z0-9-]{1,64}");
    }

    // Only matching full frame + capture sequence keys are accepted.
    // Malformed CSVs fail closed without contaminating the main report.
    private static int read(File root, String filename, long started, Set<String> keys,
                            String[] required, RowConsumer consumer) throws IOException {
        if (started <= 0 || root == null) return 0;
        File file = new File(root, filename);
        if (!file.isFile() || file.lastModified() < started ||
            file.length() > MAX_FILE_BYTES || file.length() < 16) return 0;
        try (BufferedReader input = new BufferedReader(new InputStreamReader(
                new FileInputStream(file), StandardCharsets.UTF_8))) {
            String first = input.readLine();
            if (first == null || first.length() > MAX_ROW_LENGTH) return 0;
            String[] columns = first.split(",", -1);
            Map<String, Integer> indexed = new HashMap<>();
            for (int i = 0; i < columns.length; ++i) {
                if (indexed.putIfAbsent(columns[i], i) != null) return 0;
            }
            for (String column : required) if (!indexed.containsKey(column)) return 0;
            if (!indexed.containsKey("frame") || !indexed.containsKey("sequence")) return 0;
            String line;
            int processed = 0;
            while ((line = input.readLine()) != null) {
                if (++processed > MAX_ROWS || line.length() > MAX_ROW_LENGTH) return 0;
                String[] data = line.split(",", -1);
                if (data.length != columns.length) return 0;
                String frame = cell(data, indexed, "frame");
                String sequence = cell(data, indexed, "sequence");
                if (!frame.matches("[0-9]{1,20}") ||
                    !sequence.matches("[0-9]{1,20}")) return 0;
                String key = frame + "/" + sequence;
                if (!keys.contains(key)) continue;
                if (!consumer.accept(data, indexed, key)) return 0;
            }
            return processed;
        }
    }

    private static final class Tally {
        double ms, calls, worst;
        int rows;
        void add(double elapsed, double count) {
            ms += elapsed;
            calls += count;
            worst = Math.max(worst, elapsed);
            ++rows;
        }
    }

    private static String transport(File root, long started, Set<String> frames) {
        String[] timeCols = {
            "producer_capture_sum_ms", "backpressure_sum_ms", "queue_lock_sum_ms",
            "queue_dwell_sum_ms", "worker_assembly_ms", "worker_idle_ms",
            "internal_flush_ms", "paint_total_ms"
        };
        Map<String, Tally> totals = new HashMap<>();
        Set<String> observed = new HashSet<>();
        try {
            int found = read(root, "native-performance-frames.csv", started, frames,
                    new String[] {"commands", "worker_assembly_ms", "backpressure_sum_ms"},
                    (row, cols, key) -> {
                        if (!observed.add(key)) return false;
                        for (String column : timeCols) {
                            double value = decimal(cell(row, cols, column));
                            if (!Double.isFinite(value)) continue;
                            totals.computeIfAbsent(column, missing -> new Tally()).add(value, 1);
                        }
                        double commands = decimal(cell(row, cols, "commands"));
                        if (Double.isFinite(commands))
                            totals.computeIfAbsent("commands", missing -> new Tally())
                                    .add(commands, 1);
                        return totals.size() <= timeCols.length + 1;
                    });
            if (found == 0 || observed.isEmpty() || totals.isEmpty()) return "";
            StringBuilder out = new StringBuilder("\nFrame transport (averages; elapsed CPU phases may overlap; not additive):\n");
            out.append(String.format(Locale.US, "coverage=%d/%d matching frames\n",
                    observed.size(), frames.size()));
            for (String name : timeCols) {
                Tally value = totals.get(name);
                if (value == null) continue;
                out.append(String.format(Locale.US, "%s: avg=%.3f ms (n=%d)\n",
                        name, value.ms / value.rows, value.rows));
            }
            Tally commands = totals.get("commands");
            if (commands != null) out.append(String.format(Locale.US,
                    "commands: avg=%.1f/frame\n", commands.ms / commands.rows));
            return out.toString();
        } catch (IOException ignored) {
            return "";
        }
    }

    private static String gpu(File root, long started, Set<String> frames) {
        Map<String, Tally> totals = new HashMap<>();
        Set<String> observed = new HashSet<>();
        final int[] unavailable = {0};
        try {
            int readRows = read(root, "native-performance-gpu-passes.csv", started, frames,
                    new String[] {"range", "status", "gpu_ms", "draws"},
                    (row, cols, key) -> {
                        String status = cell(row, cols, "status");
                        if (!safeLabel(status)) return false;
                        observed.add(key);
                        if (!"approximate".equals(status)) {
                            ++unavailable[0];
                            return true;
                        }
                        String range = cell(row, cols, "range");
                        double elapsed = decimal(cell(row, cols, "gpu_ms"));
                        double draws = decimal(cell(row, cols, "draws"));
                        if (!safeLabel(range) || !Double.isFinite(elapsed)
                                || !Double.isFinite(draws)) return false;
                        if (!totals.containsKey(range) && totals.size() >= MAX_GROUPS)
                            return false;
                        totals.computeIfAbsent(range, missing -> new Tally()).add(elapsed, draws);
                        return true;
                    });
            if (readRows == 0 || observed.isEmpty()) return "";
            List<Map.Entry<String, Tally>> ranked = new ArrayList<>(totals.entrySet());
            ranked.sort((a, b) -> Double.compare(b.getValue().ms, a.getValue().ms));
            StringBuilder out = new StringBuilder(
                    "\nGPU pass regions (approximate timestamp attribution, NOT summed GPU frame time):\n");
            out.append(String.format(Locale.US,
                    "coverage=%d/%d frames; unavailable/collapsed regions=%d\n",
                    observed.size(), frames.size(), unavailable[0]));
            for (int i = 0; i < Math.min(ranked.size(), 8); ++i) {
                Map.Entry<String, Tally> entry = ranked.get(i);
                Tally t = entry.getValue();
                out.append(String.format(Locale.US,
                        "%s: measured-time=%.3f ms/frame draws=%.1f/frame regions=%d\n",
                        entry.getKey(), t.ms / observed.size(),
                        t.calls / observed.size(), t.rows));
            }
            if (ranked.isEmpty()) out.append("No usable GPU pass times; do not treat missing timings as zero.\n");
            return out.toString();
        } catch (IOException ignored) {
            return "";
        }
    }

    private static String shaders(File root, long started, Set<String> frames) {
        Map<String, Tally> totals = new HashMap<>();
        Set<String> observed = new HashSet<>();
        try {
            int found = read(root, "native-performance-cpu-shaders.csv", started, frames,
                    new String[] {"render_phase", "vertex_shader", "pixel_shader", "commands", "self_ms"},
                    (row, cols, key) -> {
                        String phase = cell(row, cols, "render_phase");
                        String vs = cell(row, cols, "vertex_shader");
                        String ps = cell(row, cols, "pixel_shader");
                        if (!phase.matches("[0-9]{1,10}") ||
                            !vs.matches("(?:0x)?[0-9a-fA-F]{1,16}") ||
                            !ps.matches("(?:0x)?[0-9a-fA-F]{1,16}")) return false;
                        double commands = decimal(cell(row, cols, "commands"));
                        double self = decimal(cell(row, cols, "self_ms"));
                        if (!Double.isFinite(commands) || !Double.isFinite(self)) return false;
                        String label = "phase=" + phase + " VS=" + vs + " PS=" + ps;
                        if (!totals.containsKey(label) && totals.size() >= MAX_GROUPS) return false;
                        observed.add(key);
                        totals.computeIfAbsent(label, missing -> new Tally()).add(self, commands);
                        return true;
                    });
            if (found == 0 || observed.isEmpty() || totals.isEmpty()) return "";
            List<Map.Entry<String, Tally>> ranked = new ArrayList<>(totals.entrySet());
            ranked.sort((a, b) -> Double.compare(b.getValue().ms, a.getValue().ms));
            StringBuilder out = new StringBuilder(
                    "\nTop CPU render-phase shader costs (self time; NOT GPU shader time):\n");
            out.append(String.format(Locale.US, "coverage=%d/%d frames\n",
                    observed.size(), frames.size()));
            for (int i = 0; i < Math.min(ranked.size(), 8); ++i) {
                Map.Entry<String, Tally> entry = ranked.get(i);
                Tally t = entry.getValue();
                out.append(String.format(Locale.US,
                        "%s: self=%.3f ms/frame commands=%.1f/frame\n",
                        entry.getKey(), t.ms / observed.size(), t.calls / observed.size()));
            }
            return out.toString();
        } catch (IOException ignored) {
            return "";
        }
    }
}

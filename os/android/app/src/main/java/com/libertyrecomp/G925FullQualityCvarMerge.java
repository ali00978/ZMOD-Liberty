package com.libertyrecomp;

import java.util.List;

/**
 * Apply CPU scheduling changes without rewriting a single unrelated graphics
 * option. The standalone app may have custom visual cvars absent from the UI.
 *
 * Pure Java so it can be regression-tested without an Android emulator.
 */
final class G925FullQualityCvarMerge {
    private G925FullQualityCvarMerge() {}

    static String merge(List<String> lines, String[][] overrides) {
        StringBuilder result = new StringBuilder();
        for (String line : lines) {
            String trimmed = line.trim();
            boolean replaced = false;
            for (String[] item : overrides) {
                String key = item[0];
                if (trimmed.equals("--" + key)
                        || trimmed.startsWith("--" + key + "=")
                        || trimmed.equals("--no-" + key)
                        || trimmed.startsWith("--no-" + key + "=")) {
                    replaced = true;
                    break;
                }
            }
            if (!replaced) result.append(line).append('\n');
        }
        for (String[] item : overrides)
            result.append("--").append(item[0]).append('=').append(item[1]).append('\n');
        return result.toString();
    }
}

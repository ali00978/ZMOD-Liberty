package com.libertyrecomp;

import java.util.List;

/**
 * Explicit G925 X native backend routing. Unlike G925 Native v3.5 this
 * separate package has its own CPU-work avoidance policy. No downscaling,
 * texture replacement, shadow skipping, reflection skipping or GPU overclock.
 * User-supplied arguments and native config always take precedence.
 */
final class G925XNativeDefaults {
    private G925XNativeDefaults() {}

    static void apply(List<String> args, boolean g925x) {
        if (!g925x) return;
        addUnlessConfigured(args, "gta4_native_g925x_adaptive_sparse");
        // Default on only in G925 X. Native eligibility checks may fall back.
        addUnlessConfigured(args, "gta4_mobile_frame_generation");
    }

    private static void addUnlessConfigured(List<String> args, String name) {
        final String positive = "--" + name;
        final String negative = "--no-" + name;
        for (String argument : args) {
            if (argument.equals(positive) || argument.startsWith(positive + "=")
                    || argument.equals(negative) || argument.startsWith(negative + "="))
                return;
        }
        args.add(positive + "=true");
    }
}

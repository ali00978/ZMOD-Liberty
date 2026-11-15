package com.libertyrecomp;

import java.util.List;

/**
 * The independent G925 edition's initial CPU scheduling profile.
 *
 * This is intentionally not a rewritten renderer. It uses the verified
 * Armv9/Mali native game core while work on a different Vulkan backend is
 * isolated from the user's classic installation. No graphics-quality cvars
 * and no unverified kernel/clock overrides are ever applied here.
 *
 * User choices in args.txt, graphics.txt and overlay.txt take precedence.
 */
final class G925NativeDefaults {
    private G925NativeDefaults() {}

    static void apply(List<String> arguments, boolean nativeEdition) {
        if (!nativeEdition) return;
        addUnlessConfigured(arguments, "android_performance_hint", "true");
        addUnlessConfigured(arguments, "android_performance_hint_target_ms", "16");
        // 16 ms instead of the normal 25 ms feedback cadence requests faster
        // ADPF governor feedback on supported devices. Android owns clocks.
        addUnlessConfigured(arguments, "android_performance_hint_report_interval_ms", "16");
        addUnlessConfigured(arguments, "android_performance_hint_stable_threads", "true");
        addUnlessConfigured(arguments, "gta4_frame_wait_sleep", "true");
        // Do not change shaders, pixel count, frame queue, reflections,
        // anti-aliasing, texture quality or graphics slider values.
    }

    private static void addUnlessConfigured(List<String> arguments, String name, String value) {
        String positive = "--" + name;
        String negative = "--no-" + name;
        for (String argument : arguments) {
            if (argument.equals(positive) || argument.startsWith(positive + "=")
                    || argument.equals(negative) || argument.startsWith(negative + "=")) {
                return;
            }
        }
        arguments.add(positive + "=" + value);
    }
}

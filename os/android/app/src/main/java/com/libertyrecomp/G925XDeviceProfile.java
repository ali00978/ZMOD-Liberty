package com.libertyrecomp;

import java.util.Locale;

/**
 * Hardware identification for the independently installable G925 X lab.
 *
 * Pure Java, with explicit inputs, so CI can test the exact matching logic.
 * This is deliberately a detection/reporting guard, not a fake GPU driver and
 * not a reason to disable the game's graphics on an unrecognized handset.
 */
final class G925XDeviceProfile {
    private G925XDeviceProfile() {}

    private static String normalized(String input) {
        return input == null ? "" : input.toLowerCase(Locale.ROOT).trim();
    }

    static boolean matchesTarget(String manufacturer, String model, String socModel) {
        final String modelName = normalized(model);
        final String soc = normalized(socModel);
        final String maker = normalized(manufacturer);
        // Model code of the user's POCO X8 Pro Max international device.
        // SOC_MODEL is sometimes empty on production Android firmware.
        final boolean device = modelName.contains("poco x8 pro max")
                || modelName.contains("2602bpc18g");
        final boolean family = maker.contains("xiaomi") || maker.contains("poco");
        return device && (family || soc.contains("9500s"));
    }

    static String describe(String manufacturer, String model, String socModel) {
        if (matchesTarget(manufacturer, model, socModel))
            return "POCO X8 Pro Max detected. G925 X ARMv9/Mali target profile.";
        if (normalized(socModel).contains("9500s"))
            return "Dimensity 9500s family detected; exact phone model not verified.";
        return "Different/unknown device: G925 X optimizations not hardware-validated.";
    }
}

package com.libertyrecomp;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class G925XNativeDefaultsTest {
    private static void check(boolean b, String error) {
        if (!b) throw new AssertionError(error);
    }
    public static void main(String[] args) {
        List<String> classic = new ArrayList<>();
        G925XNativeDefaults.apply(classic, false);
        check(classic.isEmpty(), "must never change regular app");

        List<String> x = new ArrayList<>(Arrays.asList(
            "--gta4_draw_distance_scale=1.0",
            "--gta4_fps_guard=0",
            "--gta4_native_shadow_interval=1"));
        G925XNativeDefaults.apply(x, true);
        G925XNativeDefaults.apply(x, true);
        check(x.size() == 5, "must be idempotent");
        check(x.contains("--gta4_mobile_frame_generation=false"), "X defaults to stable real frames");
        check(x.contains("--gta4_native_g925x_adaptive_sparse=true"), "must enable dedicated scheduler");
        check(x.contains("--gta4_draw_distance_scale=1.0"), "must preserve full draw distance");
        check(x.contains("--gta4_fps_guard=0"), "must preserve reflect refresh");
        check(x.contains("--gta4_native_shadow_interval=1"), "must preserve shadows");

        List<String> explicitOff = new ArrayList<>(List.of(
            "--gta4_native_g925x_adaptive_sparse=false"));
        G925XNativeDefaults.apply(explicitOff, true);
        check(explicitOff.size() == 2 && explicitOff.contains("--gta4_native_g925x_adaptive_sparse=false"), "must honor explicit off");

        List<String> negative = new ArrayList<>(List.of(
            "--no-gta4_native_g925x_adaptive_sparse"));
        G925XNativeDefaults.apply(negative, true);
        check(negative.size() == 2 && negative.contains("--no-gta4_native_g925x_adaptive_sparse"), "must honor negation");
        List<String> fgOff = new ArrayList<>(List.of("--gta4_mobile_frame_generation=false"));
        G925XNativeDefaults.apply(fgOff, true);
        check(fgOff.size() == 2 && !fgOff.contains("--gta4_mobile_frame_generation=true"), "must honor FG off");
        List<String> fgOn = new ArrayList<>(List.of("--gta4_mobile_frame_generation=true"));
        G925XNativeDefaults.apply(fgOn, true);
        check(fgOn.size() == 2 && !fgOn.contains("--gta4_mobile_frame_generation=false"), "must honor explicit FG on");
        System.out.println("PASS: independent G925 X scheduler settings are lossless and user-controllable");
    }
}

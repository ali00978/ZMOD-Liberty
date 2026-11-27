package com.libertyrecomp;

/** Explicit opt-in CPU-first comparison preset; no driver or thermal overrides. */
final class G925XStableProfile {
    private G925XStableProfile() {}
    static String[][] overrides() {
        return new String[][] {
            {"gta4_mobile_frame_generation", "false"},
            {"gta4_frame_limit", "30"},
            {"gta4_extra_frames_in_flight", "1"},
            {"gta4_draw_distance_scale", "0.6"},
            {"gta4_dynamic_draw_distance", "false"},
            {"gta4_traffic_density_scale", "0.4"},
            {"gta4_parked_car_density_scale", "0.4"},
            {"gta4_ped_density_scale", "0.4"},
            {"gta4_scenario_ped_density_scale", "0.4"},
            {"gta4_output_height_cap", "720"},
            {"gta4_native_upscaler", "fsr1"},
            {"gta4_fsr1_quality", "quality"}
        };
    }
}

package com.libertyrecomp;
import java.util.List;
import java.util.HashMap;
public final class G925XStableProfileTest {
    public static void main(String[] args) {
        String[][] overrides=G925XStableProfile.overrides();
        HashMap<String,String> values=new HashMap<>();
        for(String[] kv:overrides) {
            if(values.put(kv[0],kv[1])!=null) throw new AssertionError("duplicate key");
        }
        if(values.size()!=12 || !"false".equals(values.get("gta4_mobile_frame_generation"))
            || !"30".equals(values.get("gta4_frame_limit"))
            || !"1".equals(values.get("gta4_extra_frames_in_flight"))
            || !"0.6".equals(values.get("gta4_draw_distance_scale"))
            || !"false".equals(values.get("gta4_dynamic_draw_distance")))
            throw new AssertionError("incorrect CPU-first settings");
        String merged=G925FullQualityCvarMerge.merge(List.of(
            "--gta4_extra_frames_in_flight=4","--gta4_mobile_frame_generation=true",
            "--gta4_native_shadow_interval=1","--android_performance_hint=true"),overrides);
        if(merged.contains("--gta4_extra_frames_in_flight=4") ||
            merged.contains("--gta4_mobile_frame_generation=true") ||
            !merged.contains("--gta4_native_shadow_interval=1") ||
            !merged.contains("--android_performance_hint=true"))
            throw new AssertionError("merge lost custom settings");
        System.out.println("PASS: CPU-first stability preset");
    }
}

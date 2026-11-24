package com.libertyrecomp;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.attribute.FileTime;
import java.util.Set;

public final class G925DeepProfileSummaryTest {
    private static void check(boolean yes, String what) {
        if (!yes) throw new AssertionError(what);
    }

    public static void main(String[] args) throws Exception {
        Path directory = Files.createTempDirectory("g925-deep-");
        File folder = directory.toFile();
        Set<String> frames = Set.of("1/7", "2/8");

        Path transport = directory.resolve("native-performance-frames.csv");
        Files.writeString(transport, "frame,sequence,commands,producer_capture_sum_ms,"
                + "backpressure_sum_ms,queue_lock_sum_ms,queue_dwell_sum_ms,"
                + "worker_assembly_ms,worker_idle_ms,internal_flush_ms,paint_total_ms\n"
                + "1,7,3500,4,12,1,8,10,0,1,5\n"
                + "2,8,3700,7,4,1,3,9,0,1,6\n"
                + "999,999,999999,999,999,999,999,999,999,999,999\n");
        Path gpu = directory.resolve("native-performance-gpu-passes.csv");
        Files.writeString(gpu, "frame,sequence,range,status,draws,gpu_ms\n"
                + "1,7,resolve,approximate,30,2\n"
                + "1,7,deferred-light-volumes,approximate,1100,7\n"
                + "2,8,deferred-light-volumes,approximate,1200,9\n"
                + "2,8,resolve,approximate,35,3\n"
                + "2,8,other,unavailable,0,\n"
                + "999,999,private-sensitive,approximate,50000,500\n");
        Path shaders = directory.resolve("native-performance-cpu-shaders.csv");
        Files.writeString(shaders, "frame,sequence,render_phase,vertex_shader,pixel_shader,"
                + "commands,inclusive_ms,self_ms,max_command_ms,worst_command\n"
                + "1,7,15,0x1234,0xABCD,200,10,6,1,100\n"
                + "2,8,15,0x1234,0xABCD,300,11,8,1,200\n"
                + "999,999,15,0x1234,0xABCD,100000,999,999,1,100\n");
        String report = G925DeepProfileSummary.summarize(folder, 1L, frames);
        check(report.contains("Frame transport"), "transport absent");
        check(report.contains("coverage=2/2 matching frames"), "incorrect join coverage");
        check(report.contains("backpressure_sum_ms: avg=8.000 ms"), "backpressure wrong");
        check(report.contains("commands: avg=3600.0/frame"), "commands wrong");
        check(report.contains("deferred-light-volumes: measured-time=8.000 ms/frame"), "GPU wrong");
        check(report.contains("resolve: measured-time=2.500 ms/frame"), "GPU resolve wrong");
        check(report.contains("unavailable/collapsed regions=1"), "GPU missing-label wrong");
        check(report.contains("Top CPU render-phase shader costs"), "shader list absent");
        check(report.contains("phase=15 VS=0x1234 PS=0xABCD: self=7.000 ms/frame"), "shader wrong");
        check(!report.contains("private-sensitive"), "old/unknown text leaked");

        // Bad labels must never be emitted into a share report.
        Files.writeString(gpu, "frame,sequence,range,status,draws,gpu_ms\n"
                + "1,7,not/a-safe-label,approximate,10,999\n");
        String invalid = G925DeepProfileSummary.summarize(folder, 1L, frames);
        check(!invalid.contains("GPU pass regions"), "malformed GPU data accepted");
        check(invalid.contains("Frame transport"), "malformed GPU data harmed other report");
        // A valid CSV from a previous run is excluded by its mtime.
        Files.setLastModifiedTime(transport, FileTime.fromMillis(1));
        String outdated = G925DeepProfileSummary.summarize(folder, 2L, frames);
        check(!outdated.contains("Frame transport"), "stale transport admitted");
        System.out.println("PASS: deep G925 profiler ranks matching transport, approximate GPU passes and CPU shaders safely");
    }
}

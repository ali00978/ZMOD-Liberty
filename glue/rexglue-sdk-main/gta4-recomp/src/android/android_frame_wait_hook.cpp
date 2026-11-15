// sub_82A46D70(device r3, out r4) writes {0, submitted frame, submitted -
// completed} for the device. Its only caller, sub_828BF420, polls it in a
// loop without sleeping while two frames are pending, i.e. while the title
// waits for the renderer to submit a frame: a busy-wait that held a big core
// at ~7% of the main game thread's samples. Sleep briefly when the title is
// about to spin; the game's own loop still decides when to stop waiting.
//
// The same two-frame limit caps throughput: one frame passes the command
// worker, the recorder and the GPU in sequence (~75 ms at the bridge), so two
// in flight give ~38 ms per frame while every stage has slack. Reporting
// fewer pending frames lets the title run that many frames further ahead; the
// renderer's own queue still bounds the total.
#include <time.h>

#include <android/log.h>

#include "android/android_performance_hint.h"
#include "android/g925_frame_health.h"
#include "android/g925_run_ahead_policy.h"

#include <atomic>

#include <rex/cvar.h>

#include "gta4_init.h"

// No frame rate change, but the spin held the main game thread at ~97% of a
// big core while it waited (17% with the sleep): the whole process used 2.16
// cores at the save point with the spin and 1.37 without (Retroid Pocket 5).
// It was off for a while because the game-code crash in sub_8296E480 (stale
// object pointer) first appeared in builds that had it; that crash has its
// own guard now.
REXCVAR_DEFINE_BOOL(gta4_frame_wait_sleep, true, "GTA IV/Performance",
                    "Sleep instead of spinning while the title waits for a frame slot");
REXCVAR_DEFINE_UINT32(gta4_extra_frames_in_flight, 1, "GTA IV/Performance",
                      "Additional guest frames ahead of the original two-frame wait limit "
                      "(0-4; higher throughput potential, higher memory and input latency)")
    .range(0, 4);

// Latest game update rate, shown by the in-game "Show FPS" overlay.
std::atomic<float> g_liberty_title_fps{0.0f};
std::atomic<float> g_liberty_title_p95_ms{0.0f};
std::atomic<float> g_liberty_title_max_gap_ms{0.0f};

extern "C" void sub_82A46D70(PPCContext& ctx, uint8_t* base) {
  const uint32_t out = ctx.r4.u32;
  __imp__sub_82A46D70(ctx, base);
  uint32_t pending = REX_LOAD_U32(out + 8);
  if (pending >= 0x80000000u) return;
  // The overlay now displays ACTUAL title submission intervals. We do not
  // infer render/present frame times from average game FPS.
  {
    static uint32_t last_submitted = 0;
    static uint32_t submitted_count = 0;
    static timespec window_start{};
    static gta4::android::G925FrameHealthWindow health;
    // This hook also runs during frequent title wait-loop polling. Time is
    // sampled for each real game-frame submission plus every 64th poll,
    // rather than forcing a clock syscall on every blocked iteration.
    static uint32_t poll_counter = 0;
    health.ObservePoll(pending);
    const uint32_t submitted = REX_LOAD_U32(out + 4);
    const bool new_frame = submitted != last_submitted;
    if (new_frame) {
      last_submitted = submitted;
      ++submitted_count;
      gta4::android::NoteTitleFrame();
    }
    if (new_frame || ((++poll_counter & 63u) == 0u)) {
      timespec now{};
      clock_gettime(CLOCK_MONOTONIC, &now);
      const uint64_t now_ns = uint64_t(now.tv_sec) * 1'000'000'000ULL +
                              uint64_t(now.tv_nsec);
      if (new_frame) health.ObserveTitleFrame(now_ns);
    if (window_start.tv_sec == 0) window_start = now;
    const double elapsed = double(now.tv_sec - window_start.tv_sec) +
                           double(now.tv_nsec - window_start.tv_nsec) / 1e9;
    if (elapsed >= 0.5) {
      g_liberty_title_fps.store(float(submitted_count / elapsed),
                                 std::memory_order_relaxed);
    }
    if (elapsed >= 5.0) {
      const auto metrics = health.Take();
      const float fps = float(submitted_count / elapsed);
      const float max_gap_ms = float(metrics.maximum_gap_ns / 1.0e6);
      g_liberty_title_fps.store(fps, std::memory_order_relaxed);
      g_liberty_title_p95_ms.store(float(metrics.p95_upper_ms),
                                   std::memory_order_relaxed);
      g_liberty_title_max_gap_ms.store(max_gap_ms, std::memory_order_relaxed);
      // Five-second log cadence, no extra GPU queries or Vulkan sync.
      __android_log_print(ANDROID_LOG_WARN, "LibertyRecomp",
          "title-fps: %.1f title-p95-ms-bucket: %u (251=>250) "
          "title-max-gap-ms: %.1f gaps>33ms: %u gaps>50ms: %u gaps>100ms: %u "
          "blocked-title-polls: %u/%u max-render-pending: %u "
          "(game updates, NOT displayed frames)",
          fps, metrics.p95_upper_ms, max_gap_ms, metrics.over_33ms,
          metrics.over_50ms, metrics.over_100ms, metrics.blocked_polls,
          metrics.total_polls, metrics.max_pending);
      submitted_count = 0;
      window_start = now;
    }
    }  // frame submission or sampled waiting poll
  }
  const uint32_t extra = REXCVAR_GET(gta4_extra_frames_in_flight);
  if (extra) {
    // The guest sees the relaxed wait budget; the real native pending
    // counter and Vulkan GPU completion remain unchanged. The renderer's
    // independent, bounded submission queue still applies backpressure.
    pending = gta4::android::G925AdjustedPendingFrames(pending, extra);
    REX_STORE_U32(out + 8, pending);
  }
  if (pending >= 2 && REXCVAR_GET(gta4_frame_wait_sleep)) {
    const timespec delay{0, 250 * 1000};
    nanosleep(&delay, nullptr);
  }
}

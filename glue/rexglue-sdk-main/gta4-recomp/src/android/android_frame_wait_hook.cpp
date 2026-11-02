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
REXCVAR_DEFINE_UINT32(gta4_extra_frames_in_flight, 0, "GTA IV/Performance",
                      "Frames the title may run ahead beyond its own limit of two pending "
                      "frames (more throughput, one frame of input latency each)")
    .range(0, 2);

REXCVAR_DEFINE_BOOL(gta4_frame_pacing_log, false, "GTA IV/Diagnostics",
                    "Opt-in title frame-gap statistics in logcat, once every two seconds");

// Latest game update rate, shown by the in-game "Show FPS" overlay.
std::atomic<float> g_liberty_title_fps{0.0f};

extern "C" void sub_82A46D70(PPCContext& ctx, uint8_t* base) {
  const uint32_t out = ctx.r4.u32;
  __imp__sub_82A46D70(ctx, base);
  uint32_t pending = REX_LOAD_U32(out + 8);
  if (pending >= 0x80000000u) return;
  // Title FPS is not the same as displayed Vulkan frames.
  // The title polls this hook at very high frequency while blocked on
  // pending frames. Do not call clock_gettime or Android logcat on every
  // poll: sample each submitted frame, plus one out of 64 waiting polls.
  {
    static uint32_t last_submitted = 0;
    static uint32_t submitted_count = 0;
    static uint32_t polls = 0;
    static uint32_t slow_over_33 = 0;
    static uint32_t slow_over_50 = 0;
    static uint32_t slow_over_100 = 0;
    static uint64_t previous_frame_ns = 0;
    static uint64_t largest_gap_ns = 0;
    static timespec window_start{};

    const uint32_t submitted = REX_LOAD_U32(out + 4);
    const bool is_new_frame = submitted != last_submitted;
    if (is_new_frame) {
      last_submitted = submitted;
      ++submitted_count;
      gta4::android::NoteTitleFrame();
    }

    if (is_new_frame || ((++polls & 63u) == 0u)) {
      timespec now{};
      clock_gettime(CLOCK_MONOTONIC, &now);
      const uint64_t now_ns =
          uint64_t(now.tv_sec) * 1'000'000'000ULL + uint64_t(now.tv_nsec);
      const bool collect_spikes = REXCVAR_GET(gta4_frame_pacing_log);
      if (is_new_frame) {
        if (collect_spikes && previous_frame_ns && now_ns > previous_frame_ns) {
          const uint64_t gap = now_ns - previous_frame_ns;
          if (gap > 33'333'333ULL) ++slow_over_33;
          if (gap > 50'000'000ULL) ++slow_over_50;
          if (gap > 100'000'000ULL) ++slow_over_100;
          if (gap > largest_gap_ns) largest_gap_ns = gap;
        }
        previous_frame_ns = now_ns;
      }
      if (window_start.tv_sec == 0) window_start = now;
      const double elapsed = double(now.tv_sec - window_start.tv_sec) +
                             double(now.tv_nsec - window_start.tv_nsec) / 1e9;
      if (elapsed >= 0.5) {
        g_liberty_title_fps.store(float(submitted_count / elapsed),
                                  std::memory_order_relaxed);
      }
      if (elapsed >= 2.0) {
        if (collect_spikes) {
          __android_log_print(
              ANDROID_LOG_WARN, "LibertyRecomp",
              "title-fps: %.1f max-gap-ms: %.1f gaps-over-33ms: %u "
              "gaps-over-50ms: %u gaps-over-100ms: %u "
              "(title submissions, not screen presents)",
              submitted_count / elapsed, largest_gap_ns / 1.0e6,
              slow_over_33, slow_over_50, slow_over_100);
        }
        submitted_count = 0;
        slow_over_33 = slow_over_50 = slow_over_100 = 0;
        largest_gap_ns = 0;
        window_start = now;
      }
    }
  }
  const uint32_t extra = REXCVAR_GET(gta4_extra_frames_in_flight);
  if (extra) {
    pending = pending > extra ? pending - extra : 0;
    REX_STORE_U32(out + 8, pending);
  }
  if (pending >= 2 && REXCVAR_GET(gta4_frame_wait_sleep)) {
    const timespec delay{0, 250 * 1000};
    nanosleep(&delay, nullptr);
  }
}

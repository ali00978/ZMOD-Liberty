#pragma once

// Low-overhead Android-only presentation diagnostics. Existing renderer
// instrumentation supplies host ticks; no GPU timestamp queries, queue waits,
// memory allocations, or frame scheduling changes are introduced here.
#if defined(__ANDROID__)
#include <algorithm>
#include <cstdint>
#include <android/log.h>
#include <rex/chrono/clock.h>

namespace rex::ui::vulkan {

class G925PaintWindow {
 public:
  void Observe(uint64_t acquire_ticks, uint64_t submit_ticks,
               uint64_t present_ticks, uint64_t total_ticks,
               uint64_t end_tick, uint32_t guest_frame, int32_t result) {
    if (!start_tick_ || end_tick < start_tick_) start_tick_ = end_tick;
    ++paint_attempts_;
    if (guest_frame && guest_frame != last_frame_) {
      ++distinct_guest_frames_;
      last_frame_ = guest_frame;
    }
    if (result != 0) ++nonzero_results_;
    acquire_sum_ += acquire_ticks;
    acquire_max_ = std::max(acquire_max_, acquire_ticks);
    submit_max_ = std::max(submit_max_, submit_ticks);
    present_max_ = std::max(present_max_, present_ticks);
    total_max_ = std::max(total_max_, total_ticks);

    const uint64_t hz = rex::chrono::Clock::QueryHostTickFrequency();
    if (!hz || (end_tick - start_tick_) < hz * 5) return;
    const double seconds = double(end_tick - start_tick_) / double(hz);
    const double millis = 1000.0 / double(hz);
    __android_log_print(ANDROID_LOG_WARN, "LibertyRecomp",
        "g925-paint: paint-hz=%.1f new-title-hz=%.1f "
        "acquire-avg-ms=%.2f acquire-max-ms=%.2f "
        "submit-max-ms=%.2f present-max-ms=%.2f total-max-ms=%.2f "
        "nonzero-result=%u HOST-CPU-only",
        double(paint_attempts_) / seconds,
        double(distinct_guest_frames_) / seconds,
        (double(acquire_sum_) / double(paint_attempts_)) * millis,
        double(acquire_max_) * millis, double(submit_max_) * millis,
        double(present_max_) * millis, double(total_max_) * millis,
        nonzero_results_);
    start_tick_ = end_tick;
    paint_attempts_ = 0;
    distinct_guest_frames_ = 0;
    nonzero_results_ = 0;
    acquire_sum_ = acquire_max_ = submit_max_ = present_max_ = total_max_ = 0;
  }

 private:
  uint64_t start_tick_ = 0;
  uint32_t last_frame_ = 0;
  uint32_t paint_attempts_ = 0;
  uint32_t distinct_guest_frames_ = 0;
  uint32_t nonzero_results_ = 0;
  uint64_t acquire_sum_ = 0;
  uint64_t acquire_max_ = 0;
  uint64_t submit_max_ = 0;
  uint64_t present_max_ = 0;
  uint64_t total_max_ = 0;
};

}  // namespace rex::ui::vulkan
#endif

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace gta4::android {

// Measures guest title-submission gaps, not Vulkan GPU times or displayed frames.
// No heap allocations, file I/O or locks on the game's frame-wait hot path.
struct G925FrameHealthSample {
  uint32_t intervals = 0;
  uint32_t over_33ms = 0, over_50ms = 0, over_100ms = 0;
  uint32_t p95_upper_ms = 0;
  uint32_t total_polls = 0, blocked_polls = 0, max_pending = 0;
  uint64_t maximum_gap_ns = 0;
};

class G925FrameHealthWindow {
 public:
  constexpr void ObservePoll(uint32_t pending) noexcept {
    ++sample_.total_polls;
    if (pending >= 2) ++sample_.blocked_polls;
    sample_.max_pending = std::max(sample_.max_pending, pending);
  }

  constexpr void ObserveTitleFrame(uint64_t now_ns) noexcept {
    if (previous_ns_ && now_ns > previous_ns_) {
      const uint64_t gap = now_ns - previous_ns_;
      ++sample_.intervals;
      sample_.maximum_gap_ns = std::max(sample_.maximum_gap_ns, gap);
      if (gap > 33'333'333ULL) ++sample_.over_33ms;
      if (gap > 50'000'000ULL) ++sample_.over_50ms;
      if (gap > 100'000'000ULL) ++sample_.over_100ms;
      for (size_t i = 0; i < kUpperNs.size(); ++i) {
        if (gap <= kUpperNs[i]) {
          ++histogram_[i];
          break;
        }
      }
    }
    previous_ns_ = now_ns;
  }

  constexpr G925FrameHealthSample Take() noexcept {
    G925FrameHealthSample result = sample_;
    if (result.intervals) {
      const uint32_t rank = (result.intervals * 95u + 99u) / 100u;
      uint32_t observed = 0;
      for (size_t i = 0; i < histogram_.size(); ++i) {
        observed += histogram_[i];
        if (observed >= rank) {
          result.p95_upper_ms = kUpperMs[i];
          break;
        }
      }
    }
    sample_ = {};
    histogram_ = {};
    // Retain previous_ns_ to count a frame gap across reporting windows.
    return result;
  }

 private:
  inline static constexpr std::array<uint64_t, 11> kUpperNs{
      16'666'667ULL, 20'000'000ULL, 25'000'000ULL, 33'333'333ULL,
      40'000'000ULL, 50'000'000ULL, 75'000'000ULL, 100'000'000ULL,
      150'000'000ULL, 250'000'000ULL, UINT64_MAX};
  // 251 is a sentinel for an unbounded tail: >250ms.
  inline static constexpr std::array<uint32_t, 11> kUpperMs{
      17, 20, 25, 34, 40, 50, 75, 100, 150, 250, 251};
  uint64_t previous_ns_ = 0;
  G925FrameHealthSample sample_{};
  std::array<uint32_t, 11> histogram_{};
};

static_assert([] {
  G925FrameHealthWindow health;
  health.ObservePoll(2);
  health.ObservePoll(0);
  health.ObserveTitleFrame(1'000'000'000ULL);
  health.ObserveTitleFrame(1'016'000'000ULL);
  health.ObserveTitleFrame(1'076'000'000ULL);
  auto first = health.Take();
  if (first.intervals != 2 || first.over_50ms != 1 ||
      first.maximum_gap_ns != 60'000'000ULL || first.p95_upper_ms != 75 ||
      first.blocked_polls != 1 || first.total_polls != 2 ||
      first.max_pending != 2) return false;
  health.ObserveTitleFrame(1'116'000'000ULL);
  auto second = health.Take();
  return second.intervals == 1 && second.over_33ms == 1 &&
         second.maximum_gap_ns == 40'000'000ULL;
}(), "G925 frame health rolling histogram must remain correct.");

}  // namespace gta4::android

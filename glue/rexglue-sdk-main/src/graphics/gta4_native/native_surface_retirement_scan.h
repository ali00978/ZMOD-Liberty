#pragma once

#include <algorithm>
#include <cstddef>

namespace rex::graphics::gta4_native {

// Surface retirement is already limited by completed GPU submissions and by
// the number of resources destroyed per frame. A full scan of every live
// surface just to find a handful of pending lifetimes still caused unbounded
// CPU housekeeping spikes on large scenes. Limit lookups as well as deletes.
//
// The cursor is recorder-owned; the container may append new surfaces and
// erase the current element, but does not reorder existing live elements.
// Shutdown/no-frame forced drains still scan everything that was live when
// the pass started, and still respect GPU-completion checks at the call site.
inline constexpr size_t kNativeSurfaceRetirementScanItemsPerFrame = 512;

class NativeSurfaceRetirementScan {
 public:
  constexpr NativeSurfaceRetirementScan(size_t live_count, size_t cursor, bool force_drain)
      : scan_limit_(force_drain ? live_count
                                : std::min(live_count, kNativeSurfaceRetirementScanItemsPerFrame)),
        cursor_(live_count ? cursor % live_count : 0) {}

  constexpr bool HasNext(size_t live_count) const {
    return visited_ < scan_limit_ && live_count != 0;
  }

  constexpr size_t index() const { return cursor_; }
  constexpr size_t visited() const { return visited_; }
  constexpr size_t limit() const { return scan_limit_; }

  // If a vector element was erased, the following element just shifted into
  // the same slot, so do not advance the cursor. If it wasn't erased,
  // advance one slot with wrap-around to ensure eventual progress even when
  // the pending set contains nothing near the beginning of the vector.
  constexpr void Next(size_t new_live_count, bool erased) {
    ++visited_;
    if (!new_live_count) {
      cursor_ = 0;
    } else if (erased) {
      if (cursor_ >= new_live_count) cursor_ = 0;
    } else {
      cursor_ = (cursor_ + 1) % new_live_count;
    }
  }

 private:
  size_t scan_limit_ = 0;
  size_t visited_ = 0;
  size_t cursor_ = 0;
};

}  // namespace rex::graphics::gta4_native

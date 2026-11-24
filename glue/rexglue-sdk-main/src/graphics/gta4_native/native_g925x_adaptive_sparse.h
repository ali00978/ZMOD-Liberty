#pragma once

#include <cstdint>

namespace rex::graphics::gta4_native {

// Recorder-owned G925 X command scheduling experiment.
// The existing contiguous index-draw coalescer always stays enabled.
// Sparse repacking is *only an optional acceleration*: suppressing it
// cannot drop, reorder or change any guest draw, vertex or sampled pixel.
//
// After eight consecutive full recording passes where no sparse batch was
// successfully emitted, stop spending CPU on the elaborate candidate gates
// for 48 recording passes, increasing to 96 after another empty probe window.
// The bounded backoff avoids repeated empty searches in long scenes; a real
// merge immediately resets both the miss streak and the next cooldown. This is cheap, deterministic,
// requires no wall clock/driver queries and supports host-only unit tests.
// The policy deliberately tracks passes, not submitted_frame numbers: a title
// can produce multiple internal frame submissions and wrap its counter.
class NativeG925XAdaptiveSparse {
 public:
  static constexpr uint32_t kMissesBeforeCooldown = 8;
  static constexpr uint32_t kCooldownPasses = 48;
  static constexpr uint32_t kMaxCooldownPasses = 96;

  constexpr bool BeginPass() {
    if (skip_remaining_) {
      --skip_remaining_;
      return false;
    }
    return true;
  }

  constexpr void EndPass(bool was_probed, bool batch_emitted) {
    if (!was_probed) return;
    if (batch_emitted) {
      consecutive_misses_ = 0;
      next_cooldown_ = kCooldownPasses;
      return;
    }
    if (++consecutive_misses_ >= kMissesBeforeCooldown) {
      consecutive_misses_ = 0;
      skip_remaining_ = next_cooldown_;
      next_cooldown_ = kMaxCooldownPasses;
    }
  }

  constexpr void Reset() {
    consecutive_misses_ = 0;
    skip_remaining_ = 0;
    next_cooldown_ = kCooldownPasses;
  }

  constexpr uint32_t SkipRemaining() const { return skip_remaining_; }

 private:
  uint32_t consecutive_misses_ = 0;
  uint32_t skip_remaining_ = 0;
  uint32_t next_cooldown_ = kCooldownPasses;
};

}  // namespace rex::graphics::gta4_native

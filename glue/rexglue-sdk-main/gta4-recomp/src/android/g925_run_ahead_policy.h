#pragma once

#include <algorithm>
#include <cstdint>

namespace gta4::android {

// The original title waits when pending >= 2. Lowering only the value it
// observes permits up to four additional guest frames to overlap the native
// renderer. Neither the real submission count nor GPU fence lifetime changes.
// Clamp defensively even when a stale native.toml/config requests >4.
constexpr uint32_t G925AdjustedPendingFrames(uint32_t actual_pending,
                                            uint32_t requested_extra) noexcept {
  const uint32_t extra = std::min(requested_extra, 4u);
  return actual_pending > extra ? actual_pending - extra : 0u;
}

}  // namespace gta4::android

#pragma once
#include <algorithm>
#include <cstdint>

namespace gta4::android {

// Target CPU work interval given to Android ADPF, not an FPS cap.
// The old launcher serialized "Maximum" as 20. Upgrade that one legacy
// value to the new aggressive 16ms profile, preserving balanced/battery
// explicit choices unchanged. The OS still chooses actual CPU frequencies.
constexpr uint32_t G925EffectiveHintTargetMs(uint32_t requested_ms) {
  const uint32_t value = std::clamp(requested_ms, uint32_t(10), uint32_t(50));
  return value == 20 ? 16 : value;
}

}  // namespace gta4::android

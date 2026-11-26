#pragma once
#include <cstdint>
namespace rex::ui::vulkan {
constexpr bool G925MobileFramegenCadence(uint32_t cap, uint64_t ns) noexcept {
  if (cap != 30 && cap != 40 && cap != 60) return false;
  // Don't steal GPU time from real 60 FPS. On a struggling 60-cap run,
  // permit interpolation only below ~35 real FPS; never synthesize after stalls.
  return ns >= (cap == 60 ? 28'000'000ull : 20'000'000ull) && ns <= 55'000'000ull;
}
}

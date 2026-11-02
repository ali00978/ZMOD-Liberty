#pragma once
#include <algorithm>
#include <cstdint>

namespace gta4::android {
// Experimental: the title may report at most four fewer pending frames.
// Do not modify the render worker queue or GPU resource lifetime.
inline constexpr uint32_t kG925MaxRunAhead = 4;

constexpr uint32_t G925EffectivePending(uint32_t pending, uint32_t extra) noexcept {
  const uint32_t bounded = std::min(extra, kG925MaxRunAhead);
  return pending > bounded ? pending - bounded : 0u;
}
static_assert(G925EffectivePending(0, 4) == 0);
static_assert(G925EffectivePending(2, 4) == 0);
static_assert(G925EffectivePending(4, 4) == 0);
static_assert(G925EffectivePending(5, 4) == 1);
static_assert(G925EffectivePending(6, 4) == 2);
static_assert(G925EffectivePending(10, UINT32_MAX) == 6);
}  // namespace gta4::android

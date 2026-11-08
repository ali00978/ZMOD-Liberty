// G925 v2.23: exact zero-fragment scissor elision and observation guards.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include "graphics/gta4_native/native_zero_coverage_draw.h"
#include "graphics/gta4_native/native_performance_samples.h"

using namespace rex::graphics::gta4_native;

static bool IndependentReference(const std::array<int32_t, 4>& scissor,
                                 uint32_t w, uint32_t h) {
  if (!w || !h || w > uint32_t(INT32_MAX) || h > uint32_t(INT32_MAX)) return false;
  // Intersection against the guest logical framebuffer, independent from
  // NativeRasterScissorHasNoPixels' coordinate clamping formulation.
  const int64_t left = std::max<int64_t>(0, scissor[0]);
  const int64_t top = std::max<int64_t>(0, scissor[1]);
  const int64_t right = std::min<int64_t>(w, scissor[2]);
  const int64_t bottom = std::min<int64_t>(h, scissor[3]);
  return left >= right || top >= bottom;
}

int main() {
  constexpr std::array<int32_t, 4> visible{0, 0, 1920, 1080};
  constexpr std::array<int32_t, 4> zero_width{200, 10, 200, 800};
  constexpr std::array<int32_t, 4> zero_height{1, 300, 500, 300};
  constexpr std::array<int32_t, 4> left_outside{-200, 10, 0, 200};
  constexpr std::array<int32_t, 4> right_outside{1920, 10, 1930, 200};
  constexpr std::array<int32_t, 4> above_outside{10, -100, 300, -1};
  constexpr std::array<int32_t, 4> below_outside{10, 1080, 300, 1085};
  constexpr std::array<int32_t, 4> clipped_visible{-1000, -1000, 1, 1};

  static_assert(!NativeRasterScissorHasNoPixels(visible, 1920, 1080));
  static_assert(!NativeRasterScissorHasNoPixels(clipped_visible, 1920, 1080));
  static_assert(NativeRasterScissorHasNoPixels(zero_width, 1920, 1080));
  static_assert(NativeRasterScissorHasNoPixels(zero_height, 1920, 1080));
  static_assert(NativeRasterScissorHasNoPixels(left_outside, 1920, 1080));
  static_assert(NativeRasterScissorHasNoPixels(right_outside, 1920, 1080));
  static_assert(NativeRasterScissorHasNoPixels(above_outside, 1920, 1080));
  static_assert(NativeRasterScissorHasNoPixels(below_outside, 1920, 1080));
  static_assert(!NativeRasterScissorHasNoPixels(zero_width, 0, 1080));
  static_assert(!NativeRasterScissorHasNoPixels(zero_width, UINT32_MAX, 1080));

  static_assert(CanElideNativeZeroCoverageDraw(zero_width, 1920, 1080, false, false, false));
  static_assert(!CanElideNativeZeroCoverageDraw(visible, 1920, 1080, false, false, false));
  static_assert(!CanElideNativeZeroCoverageDraw(zero_width, 1920, 1080, true, false, false));
  static_assert(!CanElideNativeZeroCoverageDraw(zero_width, 1920, 1080, false, true, false));
  static_assert(!CanElideNativeZeroCoverageDraw(zero_width, 1920, 1080, false, false, true));
  assert(std::string_view(performance::CounterName(
      performance::Counter::kZeroCoverageDrawSkips)) == "zero-coverage-draw-skips");

  std::mt19937 rng(92523);
  for (uint32_t i = 0; i < 300000; ++i) {
    const uint32_t width = (rng() % 5000) + 1;
    const uint32_t height = (rng() % 5000) + 1;
    std::array<int32_t, 4> scissor{
      int32_t(rng() % 9000) - 2000, int32_t(rng() % 9000) - 2000,
      int32_t(rng() % 9000) - 2000, int32_t(rng() % 9000) - 2000,
    };
    const bool actual = NativeRasterScissorHasNoPixels(scissor, width, height);
    const bool expected = IndependentReference(scissor, width, height);
    assert(actual == expected);
    const bool query = (rng() & 1) != 0;
    const bool diagnostic = (rng() & 1) != 0;
    const bool temporal = (rng() & 1) != 0;
    const bool allowed = CanElideNativeZeroCoverageDraw(
        scissor, width, height, query, diagnostic, temporal);
    assert(allowed == (expected && !query && !diagnostic && !temporal));
  }
  std::cout << "PASS: 300000 exact empty-scissor comparisons, query/diagnostic/temporal "
               "guards, and native performance counter schema\n";
}

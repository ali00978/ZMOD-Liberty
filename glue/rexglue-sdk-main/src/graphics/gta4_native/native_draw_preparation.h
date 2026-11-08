#pragma once

#include <bit>
#include "native_exact_memo.h"
#include "native_pipeline_policy.h"

namespace rex::graphics::gta4_native {

struct NativeRasterAreaKey {
  std::array<uint32_t, 6> viewport_bits{};
  std::array<int32_t, 4> scissor{};
  uint32_t width = 0, height = 0, logical_width = 0, logical_height = 0;
  std::array<uint32_t, 2> maximum_dimensions{};
  std::array<uint32_t, 2> bounds_bits{};
  bool suppress_coverage = false;
  bool operator==(const NativeRasterAreaKey&) const = default;
};

struct NativeRasterArea {
  NativeViewportSelection viewport;
  VkRect2D scissor{};
  float scale_x = 1.0f, scale_y = 1.0f;
};

// These are the same integer rounding rules used by the native resolve paths.
inline int32_t NativeRasterCoordinateFloor(int32_t value, uint32_t logical, uint32_t physical) {
  if (value <= 0 || !logical || logical == physical) return value;
  return int32_t((uint64_t(uint32_t(value)) * physical) / logical);
}
inline int32_t NativeRasterCoordinateCeil(int32_t value, uint32_t logical, uint32_t physical) {
  if (value <= 0 || !logical || logical == physical) return value;
  return int32_t((uint64_t(uint32_t(value)) * physical + logical - 1) / logical);
}

inline NativeRasterArea ComputeNativeRasterArea(const NativeRasterAreaKey& key) {
  struct Limits {
    std::array<uint32_t, 2> maxViewportDimensions;
    std::array<float, 2> viewportBoundsRange;
  } limits{key.maximum_dimensions, std::bit_cast<std::array<float, 2>>(key.bounds_bits)};
  NativeRasterArea result;
  const float scale_x = key.logical_width ? float(key.width) / float(key.logical_width) : 1.0f;
  const float scale_y = key.logical_height ? float(key.height) / float(key.logical_height) : 1.0f;
  result.scale_x = scale_x;
  result.scale_y = scale_y;
  result.viewport = SelectNativeViewport(std::bit_cast<std::array<float, 6>>(key.viewport_bits),
                                         scale_x, scale_y, key.width, key.height, limits);
  // The guest XDK already intersected its scissor with its viewport. Preserve
  // that authoritative rectangle, including floor/ceil at scaled boundaries.
  const int32_t logical_left = std::clamp(key.scissor[0], int32_t(0), int32_t(key.logical_width));
  const int32_t logical_top = std::clamp(key.scissor[1], int32_t(0), int32_t(key.logical_height));
  const int32_t logical_right = std::clamp(key.scissor[2], logical_left, int32_t(key.logical_width));
  const int32_t logical_bottom = std::clamp(key.scissor[3], logical_top, int32_t(key.logical_height));
  const int32_t left = NativeRasterCoordinateFloor(logical_left, key.logical_width, key.width);
  const int32_t top = NativeRasterCoordinateFloor(logical_top, key.logical_height, key.height);
  const int32_t right = NativeRasterCoordinateCeil(logical_right, key.logical_width, key.width);
  const int32_t bottom = NativeRasterCoordinateCeil(logical_bottom, key.logical_height, key.height);
  result.scissor.offset = {left, top};
  result.scissor.extent = {uint32_t(right - left), uint32_t(bottom - top)};
  if (result.viewport.empty || key.suppress_coverage) result.scissor = {};
  return result;
}

// No GPU resources or command-buffer state. Device limits, dimensions, debug
// coverage and every guest input are in the key, with bitwise float identity.
using NativeRasterAreaMemo = NativeExactMemo<NativeRasterAreaKey, NativeRasterArea>;

}  // namespace rex::graphics::gta4_native

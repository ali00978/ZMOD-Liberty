#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace rex::graphics::gta4_native {

// Identical clamping to RecordClear, before scaling into a physical render area.
// Unknown/invalid dimensions take the original conservative Vulkan path.
constexpr bool NativeClearRectHasNoPixels(int32_t left, int32_t top, int32_t right,
                                           int32_t bottom, uint32_t logical_width,
                                           uint32_t logical_height) {
  if (!logical_width || !logical_height ||
      logical_width > uint32_t(std::numeric_limits<int32_t>::max()) ||
      logical_height > uint32_t(std::numeric_limits<int32_t>::max()))
    return false;
  const int32_t width = int32_t(logical_width), height = int32_t(logical_height);
  const int32_t l = std::clamp(left, int32_t(0), width);
  const int32_t t = std::clamp(top, int32_t(0), height);
  const int32_t r = std::clamp(right, l, width);
  const int32_t b = std::clamp(bottom, t, height);
  return r <= l || b <= t;
}

constexpr bool NativeClearRectCoversTarget(int32_t left, int32_t top, int32_t right,
                                            int32_t bottom, uint32_t logical_width,
                                            uint32_t logical_height) {
  if (!logical_width || !logical_height ||
      logical_width > uint32_t(std::numeric_limits<int32_t>::max()) ||
      logical_height > uint32_t(std::numeric_limits<int32_t>::max()))
    return false;
  return left <= 0 && top <= 0 && right >= int32_t(logical_width) &&
         bottom >= int32_t(logical_height);
}

// Only color attachments actually selected for the current render pass are
// eligible. Inactive guest color targets remain on their original path.
constexpr uint32_t NativeFullColorClearMask(
    uint32_t flags, uint32_t attachment_mask, int32_t left, int32_t top,
    int32_t right, int32_t bottom, uint32_t logical_width,
    uint32_t logical_height) {
  return NativeClearRectCoversTarget(left, top, right, bottom, logical_width, logical_height)
             ? (flags & attachment_mask & 0xFu) : 0u;
}
}  // namespace rex::graphics::gta4_native

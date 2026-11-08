#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace rex::graphics::gta4_native {

// This is exactly the logical-scissor clamping used by ComputeNativeRasterArea
// before scaling to the physical render target. It does NOT assume that the
// guest's scissor-enable bit is authoritative: the native renderer always
// consumes the guest/XDK-intersected scissor in its raster-area selection.
// Empty rectangles produce no fragments, regardless of viewport scale or GPU.
// Unusual oversized logical dimensions conservatively keep the draw.
constexpr bool NativeRasterScissorHasNoPixels(const std::array<int32_t, 4>& scissor,
                                              uint32_t logical_width,
                                              uint32_t logical_height) {
  if (!logical_width || !logical_height ||
      logical_width > uint32_t(std::numeric_limits<int32_t>::max()) ||
      logical_height > uint32_t(std::numeric_limits<int32_t>::max()))
    return false;
  const int32_t width = int32_t(logical_width);
  const int32_t height = int32_t(logical_height);
  const int32_t left = std::clamp(scissor[0], int32_t(0), width);
  const int32_t top = std::clamp(scissor[1], int32_t(0), height);
  const int32_t right = std::clamp(scissor[2], left, width);
  const int32_t bottom = std::clamp(scissor[3], top, height);
  return right <= left || bottom <= top;
}

// Only elide a draw if it has zero fragment coverage AND none of its GPU
// observations can be read back. Keep draws around occlusion/fire/translucency
// queries, native diagnostic probes and the temporal/modern path untouched.
// Scope creation, attachment load-op clears and the guest command timeline
// remain in the renderer: only the no-pixel GPU draw itself is skipped.
constexpr bool CanElideNativeZeroCoverageDraw(const std::array<int32_t, 4>& scissor,
                                               uint32_t logical_width,
                                               uint32_t logical_height,
                                               bool query_active,
                                               bool diagnostic_active,
                                               bool temporal_or_special_work) {
  return !query_active && !diagnostic_active && !temporal_or_special_work &&
         NativeRasterScissorHasNoPixels(scissor, logical_width, logical_height);
}

}  // namespace rex::graphics::gta4_native

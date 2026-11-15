#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace rex::graphics::gta4_native {

// Xbox 360 RectangleList draws three guest corners. The existing native
// renderer reconstructs a fourth vertex and emits a separate 4-vertex
// triangle strip for each rectangle. A 32-bit index buffer can put those
// *same* strips in one vkCmdDrawIndexed, separated by Vulkan's standard
// primitive-restart sentinel. No triangles, order, winding or vertex values
// are changed. The pipeline must use VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
// with primitiveRestartEnable=VK_TRUE.
inline constexpr uint32_t kNativeRectangleRestartIndex = UINT32_MAX;

constexpr uint32_t NativeRectangleStripIndexCount(uint32_t rectangles) {
  if (!rectangles) return 0;
  const uint64_t total = uint64_t(rectangles) * 5 - 1;
  return total <= UINT32_MAX ? uint32_t(total) : 0;
}

// Reject invalid output *before* writing a single index. No caller may use
// a failed partial upload as an indexed draw.
inline bool WriteNativeRectangleStripIndices(std::span<uint32_t> indices,
                                               uint32_t rectangles) {
  const uint32_t required = NativeRectangleStripIndexCount(rectangles);
  if (!required || indices.size() < required) return false;
  size_t at = 0;
  for (uint32_t rect = 0; rect < rectangles; ++rect) {
    if (rect) indices[at++] = kNativeRectangleRestartIndex;
    const uint32_t first = rect * 4;
    indices[at++] = first;
    indices[at++] = first + 1;
    indices[at++] = first + 2;
    indices[at++] = first + 3;
  }
  return at == required;
}

}  // namespace rex::graphics::gta4_native

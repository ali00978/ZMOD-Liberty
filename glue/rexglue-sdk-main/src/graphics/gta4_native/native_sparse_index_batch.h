#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace rex::graphics::gta4_native {

// Repack precisely ordered, otherwise nonadjacent guest index ranges as ONE
// triangle-list draw. The caller must also verify exact immutable pipeline,
// vertex/index buffer, shader constants, texture, raster, target and query
// identity. The original source is the already host-endian index buffer.
struct NativeSparseIndexRange {
  uint32_t first = 0;
  uint32_t count = 0;
};

inline constexpr uint32_t kNativeSparseIndexBatchMaxIndices = 65536;
inline constexpr size_t kNativeSparseIndexBatchMaxDraws = 64;

constexpr uint32_t NativeSparseTriangleIndexCount(
    std::span<const NativeSparseIndexRange> ranges, uint64_t source_elements) {
  if (ranges.size() < 2 || ranges.size() > kNativeSparseIndexBatchMaxDraws)
    return 0;
  uint32_t total = 0;
  for (const auto& range : ranges) {
    if (range.count < 3 || range.count % 3 != 0 ||
        uint64_t(range.first) + range.count > source_elements ||
        range.count > kNativeSparseIndexBatchMaxIndices - total)
      return 0;
    total += range.count;
  }
  return total;
}

// 3 tiny opaque identical draws can save two GPU commands with a bounded
// <=1024-index upload. The caller still verifies all existing state/query gates.
inline constexpr uint32_t kNativeSmallSparseBatchMaxIndices = 1024;
constexpr bool ShouldBatchNativeSparseIndexDraws(size_t draw_count, size_t adjacent_count,
                                                 uint32_t indices, bool has_gap) {
  return has_gap &&
      ((draw_count >= 4 && draw_count >= adjacent_count + 2) ||
       (draw_count == 3 && adjacent_count == 1 &&
        indices <= kNativeSmallSparseBatchMaxIndices));
}
constexpr bool NativeSparseTriangleBatchHasGap(
    std::span<const NativeSparseIndexRange> ranges) {
  for (size_t i = 1; i < ranges.size(); ++i) {
    if (uint64_t(ranges[i - 1].first) + ranges[i - 1].count != ranges[i].first)
      return true;
  }
  return false;
}

// Two-stage validation: malformed batches cannot partially overwrite an upload.
// The uploaded bytes have the identical host-endian 16/32-bit index values as
// each guest draw, in exactly the original command/triangle order. This does
// not collapse duplicate triangles or change the vertex base.
inline bool WriteNativeSparseTriangleIndices(
    std::span<const uint8_t> source,
    std::span<const NativeSparseIndexRange> ranges, bool index32,
    std::span<uint8_t> destination) {
  const size_t element = index32 ? sizeof(uint32_t) : sizeof(uint16_t);
  const uint32_t count = NativeSparseTriangleIndexCount(ranges, source.size() / element);
  if (!count || destination.size() < size_t(count) * element)
    return false;
  size_t output_offset = 0;
  for (const auto& range : ranges) {
    const size_t bytes = size_t(range.count) * element;
    std::memcpy(destination.data() + output_offset,
                source.data() + size_t(range.first) * element, bytes);
    output_offset += bytes;
  }
  return output_offset == size_t(count) * element;
}

}  // namespace rex::graphics::gta4_native

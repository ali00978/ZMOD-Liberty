#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace rex::graphics::gta4_native {

// Repack precisely ordered, otherwise nonadjacent guest index ranges as ONE
// triangle-list draw. The caller must also verify exact immutable pipeline,
// vertex/index buffer, shader constants, texture, raster, target and query
// identity. The original source is the already host-endian index buffer.
struct NativeSparseIndexRange {
  uint32_t first = 0;
  uint32_t count = 0;
  // Original vkCmdDrawIndexed vertexOffset. Consecutive draws with different
  // offsets can only be merged after every index is rebased losslessly.
  int32_t base_vertex = 0;
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

// Large CPU/GPU win opportunity: reduce *real Vulkan draw submissions*.
// Lossless pair/triple batching previously allowed only 384 / 1024 indices,
// rejecting medium-size meshes even with identical immutable render state.
// Admit 4x / 3x larger candidates while bounding a merged upload to 6 / 12 KiB
// for 32-bit indices (half for 16-bit). The already established larger-run
// policy and its 65536-index hard cap are unchanged. Profile sparse-indexed-
// draws-merged and GPU time before attributing a device-side FPS gain.
// These are maximum *total* guest indices, not per-draw budgets.
// Caller still checks blending, stencil, queries, shader vertex IDs, render
// targets, binding generations, and pipeline/constant identity.
inline constexpr uint32_t kNativeSmallSparseBatchMaxIndices = 3072;
inline constexpr uint32_t kNativePairedSparseBatchMaxIndices = 1536;
constexpr bool ShouldBatchNativeSparseIndexDraws(size_t draw_count, size_t adjacent_count,
                                                 uint32_t indices, bool has_gap) {
  return has_gap &&
      ((draw_count >= 4 && draw_count >= adjacent_count + 2) ||
       (draw_count == 3 && adjacent_count == 1 &&
        indices <= kNativeSmallSparseBatchMaxIndices) ||
       (draw_count == 2 && adjacent_count == 1 &&
        indices <= kNativePairedSparseBatchMaxIndices));
}
constexpr bool NativeSparseTriangleBatchHasGap(
    std::span<const NativeSparseIndexRange> ranges) {
  for (size_t i = 1; i < ranges.size(); ++i) {
    if (uint64_t(ranges[i - 1].first) + ranges[i - 1].count != ranges[i].first)
      return true;
  }
  return false;
}

// A physical index gap is not necessary when consecutive guest draws used
// different vertex offsets: their index words must first be converted.  The
// original copy-only path remains byte-identical for common-offset draws.
constexpr bool NativeSparseTriangleBatchNeedsRebase(
    std::span<const NativeSparseIndexRange> ranges) {
  if (ranges.empty()) return false;
  for (size_t i = 1; i < ranges.size(); ++i)
    if (ranges[i].base_vertex != ranges.front().base_vertex) return true;
  return false;
}

// Conservative, exact vertex re-basing for an opaque triangle-list run:
//
// Original draw n: vertex = guest_index + original_base[n].
// Batched draw:   vertex = uint32_index + minimum_original_base.
//
// To ensure equal vertex fetches with a single Vulkan indexed draw, reject
// negative/out-of-range effective vertices, integer overflow, malformed ranges
// and unsupported destination sizes *before writing any output*.  Output is
// always UINT32 indices; it never aliases the guest's index buffer.
//
// The caller MUST separately verify identical immutable shader/pipeline,
// descriptors, raster state, vertex streams, texture sources, constants,
// render target, no blending/stencil/query side effects, and that the shader
// does not use SV_VertexID. This helper does not replace those safety gates.
inline bool WriteNativeRebasedSparseTriangleIndices(
    std::span<const uint8_t> source,
    std::span<const NativeSparseIndexRange> ranges,
    bool source_is_32bit,
    std::span<uint8_t> destination,
    int32_t* output_base_vertex) {
  const size_t input_stride = source_is_32bit ? sizeof(uint32_t) : sizeof(uint16_t);
  const uint32_t total = NativeSparseTriangleIndexCount(ranges, source.size() / input_stride);
  if (!output_base_vertex || !total || !NativeSparseTriangleBatchNeedsRebase(ranges) ||
      destination.size() < size_t(total) * sizeof(uint32_t))
    return false;
  int32_t common_base = ranges.front().base_vertex;
  for (const auto& range : ranges)
    if (range.base_vertex < common_base) common_base = range.base_vertex;

  // First pass prevalidates all effective vertex IDs and the output index
  // width. No allocations, no writes, and no changes to the original buffer.
  const auto read_index = [&](size_t index) {
    const uint8_t* bytes = source.data() + index * input_stride;
    if (source_is_32bit) {
      uint32_t value;
      std::memcpy(&value, bytes, sizeof(value));
      return value;
    }
    uint16_t value;
    std::memcpy(&value, bytes, sizeof(value));
    return uint32_t(value);
  };
  for (const auto& range : ranges) {
    for (uint32_t i = 0; i < range.count; ++i) {
      const int64_t effective = int64_t(read_index(size_t(range.first) + i)) +
                                int64_t(range.base_vertex);
      const int64_t rebased = effective - int64_t(common_base);
      if (effective < 0 ||
          effective > std::numeric_limits<int32_t>::max() ||
          rebased < 0 || rebased > std::numeric_limits<uint32_t>::max())
        return false;
    }
  }

  size_t written = 0;
  for (const auto& range : ranges) {
    for (uint32_t i = 0; i < range.count; ++i) {
      const int64_t effective = int64_t(read_index(size_t(range.first) + i)) +
                                int64_t(range.base_vertex);
      const uint32_t index = uint32_t(effective - int64_t(common_base));
      std::memcpy(destination.data() + written * sizeof(index), &index, sizeof(index));
      ++written;
    }
  }
  *output_base_vertex = common_base;
  return written == total;
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
  // Exact consecutive guest ranges form one byte-contiguous source span.
  // Copy each maximal adjacent run once instead of issuing a memcpy per draw.
  // Out-of-order, overlapping, repeated or genuinely sparse ranges still
  // preserve their original input ordering and exact source bytes.
  size_t output_offset = 0;
  size_t run_first = ranges.front().first;
  size_t run_count = ranges.front().count;
  const auto flush_run = [&] {
    const size_t bytes = run_count * element;
    std::memcpy(destination.data() + output_offset,
                source.data() + run_first * element, bytes);
    output_offset += bytes;
  };
  for (size_t i = 1; i < ranges.size(); ++i) {
    const auto& range = ranges[i];
    if (uint64_t(run_first) + run_count == range.first) {
      run_count += range.count;
    } else {
      flush_run();
      run_first = range.first;
      run_count = range.count;
    }
  }
  flush_run();
  return output_offset == size_t(count) * element;
}

}  // namespace rex::graphics::gta4_native

#pragma once

#include <cstdint>

namespace rex::graphics::gta4_native {

// A run of separately drawn triangle lists can be represented by a single
// vertex draw ONLY when no triangle is split across command boundaries and
// the vertex ranges are directly adjacent in the same immutable buffers.
// All pipeline/resource/constant identity checks are performed by the caller.
// The extra end-bound is conservative: never wrap a 32-bit vertex index.
constexpr bool CanExtendNativeSequentialTriangleList(uint32_t first_vertex,
                                                      uint32_t merged_vertex_count,
                                                      uint32_t next_first_vertex,
                                                      uint32_t next_vertex_count) {
  if (!merged_vertex_count || !next_vertex_count ||
      merged_vertex_count % 3 != 0 || next_vertex_count % 3 != 0) return false;
  const uint64_t next_start = uint64_t(first_vertex) + merged_vertex_count;
  const uint64_t end = next_start + next_vertex_count;
  return next_start == next_first_vertex && end <= UINT32_MAX &&
         uint64_t(merged_vertex_count) + next_vertex_count <= UINT32_MAX;
}

}  // namespace rex::graphics::gta4_native

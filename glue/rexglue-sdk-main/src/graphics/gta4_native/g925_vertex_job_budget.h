#pragma once
#include <cstddef>

namespace rex::graphics::gta4_native {

// A streaming scene can enqueue hundreds of meshes faster than one phone CPU
// can reorder them. Preserve original draw ordering rather than allocating an
// unbounded backlog of copies: the renderer already draws a rejected range in
// original index order. Pure policy for C++20 regression tests.
constexpr size_t kG925MaxVertexOptimizationJobs = 512;
constexpr size_t kG925MaxVertexOptimizationBytes = 32u * 1024u * 1024u;

constexpr bool G925CanQueueVertexOptimization(size_t queued_jobs,
                                                size_t queued_bytes,
                                                size_t incoming_bytes) noexcept {
  return incoming_bytes != 0 &&
         queued_jobs < kG925MaxVertexOptimizationJobs &&
         incoming_bytes <= kG925MaxVertexOptimizationBytes &&
         queued_bytes <= kG925MaxVertexOptimizationBytes - incoming_bytes;
}

static_assert(G925CanQueueVertexOptimization(0, 0, 6));
static_assert(G925CanQueueVertexOptimization(511, 0, 6));
static_assert(!G925CanQueueVertexOptimization(512, 0, 6));
static_assert(!G925CanQueueVertexOptimization(0, 0, 0));
static_assert(!G925CanQueueVertexOptimization(0, 0, kG925MaxVertexOptimizationBytes + 1));
static_assert(G925CanQueueVertexOptimization(3, kG925MaxVertexOptimizationBytes - 6, 6));
static_assert(!G925CanQueueVertexOptimization(3, kG925MaxVertexOptimizationBytes - 5, 6));

}  // namespace rex::graphics::gta4_native

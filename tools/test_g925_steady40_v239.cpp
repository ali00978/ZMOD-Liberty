#include <cassert>
#include <cstddef>
#include <cstdint>
#include <random>

#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/g925_vertex_job_budget.h"

using namespace rex::graphics::gta4_native;

int main() {
  std::mt19937_64 rng{0x92540u};
  for (int i = 0; i < 100000; ++i) {
    const size_t queued_jobs = static_cast<size_t>(rng() % 550u);
    const size_t queued_bytes = static_cast<size_t>(rng() % (64u * 1024u * 1024u));
    const size_t incoming_bytes = static_cast<size_t>(rng() % (64u * 1024u * 1024u));
    const bool expected =
        queued_jobs < kG925MaxVertexOptimizationJobs &&
        incoming_bytes != 0 &&
        incoming_bytes <= kG925MaxVertexOptimizationBytes &&
        queued_bytes <= kG925MaxVertexOptimizationBytes - incoming_bytes;
    assert(G925CanQueueVertexOptimization(queued_jobs, queued_bytes, incoming_bytes) == expected);
  }
  // Check overflow-safe arithmetic for untrusted or malformed job sizes.
  assert(!G925CanQueueVertexOptimization(0, SIZE_MAX, 1));
  assert(!G925CanQueueVertexOptimization(0, 1, SIZE_MAX));
  assert(G925CanQueueVertexOptimization(0, 0, kG925MaxVertexOptimizationBytes));
  assert(!G925CanQueueVertexOptimization(0, 1, kG925MaxVertexOptimizationBytes));
}

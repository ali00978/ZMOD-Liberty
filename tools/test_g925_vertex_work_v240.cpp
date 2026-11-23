#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_vertex_cache.h"

using namespace rex::graphics::gta4_native;

template <typename Index>
void VerifyReorder(uint32_t triangles, std::mt19937& random) {
  std::vector<Index> input(triangles * 3u);
  for (auto& value : input)
    value = static_cast<Index>(random() % 256u);
  std::vector<Index> output(input.size(), std::numeric_limits<Index>::max());
  assert(OptimizeTriangleListForVertexCache(input.data(), triangles, output.data()));
  auto asTriangles = [](const std::vector<Index>& indices) {
    std::vector<std::array<Index, 3>> result;
    for (size_t i = 0; i < indices.size(); i += 3)
      result.push_back({indices[i], indices[i + 1], indices[i + 2]});
    std::sort(result.begin(), result.end());
    return result;
  };
  // Every triangle keeps its three indices in the same winding order. A
  // different triangle ordering is acceptable for the guarded opaque draws.
  assert(asTriangles(input) == asTriangles(output));
}

int main() {
  static_assert(!G925VertexRangeWorthOptimizing(0));
  static_assert(!G925VertexRangeWorthOptimizing(287));
  static_assert(G925VertexRangeWorthOptimizing(288));
  static_assert(!G925VertexRangeWorthOptimizing(289));
  static_assert(G925VertexRangeWorthOptimizing(300));

  std::array<uint32_t, 3> huge{0u, UINT32_MAX, 1u};
  std::array<uint32_t, 3> untouched{123u, 123u, 123u};
  assert(!OptimizeTriangleListForVertexCache(huge.data(), 1u, untouched.data()));
  assert(untouched == (std::array<uint32_t, 3>{123u, 123u, 123u}));
  assert(!OptimizeTriangleListForVertexCache(
      huge.data(), UINT32_MAX / 3u + 1u, untouched.data()));
  assert(!OptimizeTriangleListForVertexCache(
      static_cast<const uint32_t*>(nullptr), 1u, untouched.data()));

  std::mt19937 random(0x925240u);
  for (int i = 0; i < 80; ++i) {
    const uint32_t triangles = 1u + random() % 192u;
    VerifyReorder<uint16_t>(triangles, random);
    VerifyReorder<uint32_t>(triangles, random);
  }
}

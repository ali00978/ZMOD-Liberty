// G925 Native v2.44: indexed draw fusion across differing vertex offsets.
//
// This is a behavioral oracle: the resulting effective vertex IDs for every
// triangle must match the original sequence exactly, not merely the number
// of indices or bytes. Wrong geometry or changed winding is a hard failure.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <span>
#include <vector>

#include "graphics/gta4_native/native_sparse_index_batch.h"

using namespace rex::graphics::gta4_native;

template <typename Index>
static void FuzzRebasedVertices(uint32_t seed) {
  std::mt19937 rng(seed);
  constexpr size_t kSourceSize = 8192;
  std::vector<Index> guest(kSourceSize);
  for (auto& index : guest)
    index = Index(rng() % 1024);

  const auto source = std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(guest.data()),
      guest.size() * sizeof(Index));

  for (uint32_t trial = 0; trial < 7000; ++trial) {
    const uint32_t num_draws = 2 + rng() % 10;
    std::vector<NativeSparseIndexRange> ranges;
    ranges.reserve(num_draws);
    uint32_t total = 0;
    for (uint32_t d = 0; d < num_draws; ++d) {
      // Testing positive/negative offsets, duplicate/overlapping geometry,
      // contiguous source spans and out-of-order source spans.
      const uint32_t count = 3 * (1 + rng() % 25);
      const uint32_t first = rng() % (kSourceSize - count + 1);
      const int32_t base = int32_t(rng() % 1200) - 100;
      ranges.push_back({first, count, base});
      total += count;
    }
    if (ranges[1].base_vertex == ranges[0].base_vertex)
      ranges[1].base_vertex += 17;
    assert(NativeSparseTriangleBatchNeedsRebase(ranges));
    assert(NativeSparseTriangleIndexCount(ranges, guest.size()) == total);

    std::vector<uint32_t> written(total + 3, 0xDEADBEEFu);
    int32_t common_base = 1986;
    const bool valid = WriteNativeRebasedSparseTriangleIndices(
        source, ranges, sizeof(Index) == 4,
        {reinterpret_cast<uint8_t*>(written.data()), total * sizeof(uint32_t)},
        &common_base);

    bool expected_valid = true;
    for (const auto& range : ranges)
      for (uint32_t k = 0; k < range.count; ++k) {
        const int64_t expected = int64_t(guest[range.first + k]) + range.base_vertex;
        if (expected < 0 || expected > std::numeric_limits<int32_t>::max())
          expected_valid = false;
      }
    assert(valid == expected_valid);
    assert(written[total] == 0xDEADBEEFu);
    assert(written[total+1] == 0xDEADBEEFu);
    assert(written[total+2] == 0xDEADBEEFu);
    if (!valid) {
      assert(common_base == 1986);
      for (uint32_t k = 0; k < total; ++k) assert(written[k] == 0xDEADBEEFu);
      continue;
    }
    assert(common_base == std::min_element(ranges.begin(), ranges.end(),
        [](const auto& a, const auto& b) {
          return a.base_vertex < b.base_vertex;
        })->base_vertex);
    size_t index = 0;
    for (const auto& range : ranges) {
      for (uint32_t k = 0; k < range.count; ++k) {
        const int64_t original = int64_t(guest[range.first + k]) +
                                 range.base_vertex;
        const int64_t merged = int64_t(written[index++]) + common_base;
        assert(original == merged);
      }
    }
    assert(index == total);
  }
}

static void InvalidRejectsWithoutWrites() {
  const std::array<uint32_t, 12> guest = {
      0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14
  };
  const auto source = std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(guest.data()),
      guest.size() * sizeof(uint32_t));
  std::array<uint32_t, 20> output{};
  auto reject = [&](std::span<const NativeSparseIndexRange> ranges) {
    output.fill(0xFAFBFCFDu);
    int32_t common = 456;
    assert(!WriteNativeRebasedSparseTriangleIndices(
        source, ranges, true,
        {reinterpret_cast<uint8_t*>(output.data()), output.size() * 4},
        &common));
    assert(common == 456);
    for (uint32_t value : output) assert(value == 0xFAFBFCFDu);
  };

  const std::array<NativeSparseIndexRange, 2> same_base{{
      {0, 3, 0}, {3, 3, 0}
  }};
  reject(same_base);
  const std::array<NativeSparseIndexRange, 2> negative_effective{{
      {0, 3, -1}, {3, 3, 1}
  }};
  reject(negative_effective);
  const std::array<NativeSparseIndexRange, 2> out_of_bounds{{
      {0, 3, 0}, {12, 3, 1}
  }};
  reject(out_of_bounds);
  const std::array<NativeSparseIndexRange, 2> invalid_triangle_count{{
      {0, 4, 0}, {4, 3, 1}
  }};
  reject(invalid_triangle_count);
  const std::array<NativeSparseIndexRange, 2> too_large_vertex{{
      {0, 3, 0}, {3, 3, INT32_MAX}
  }};
  reject(too_large_vertex);
  const std::array<NativeSparseIndexRange, 2> good{{
      {0, 3, 0}, {3, 3, 2}
  }};
  output.fill(0xFAFBFCFDu);
  int32_t common = 456;
  assert(!WriteNativeRebasedSparseTriangleIndices(
      source, good, true,
      {reinterpret_cast<uint8_t*>(output.data()), 5 * 4}, &common));
  assert(common == 456);
  for (uint32_t value : output) assert(value == 0xFAFBFCFDu);
  assert(!WriteNativeRebasedSparseTriangleIndices(
      source, good, true,
      {reinterpret_cast<uint8_t*>(output.data()), output.size()*4}, nullptr));

  // Entire original triangle order remains valid when source ranges touch,
  // but their original base vertex differs (a new batch opportunity).
  int32_t base = 0;
  assert(!NativeSparseTriangleBatchHasGap(good));
  assert(NativeSparseTriangleBatchNeedsRebase(good));
  assert(WriteNativeRebasedSparseTriangleIndices(
      source, good, true, {reinterpret_cast<uint8_t*>(output.data()), 6*4}, &base));
  assert(base == 0);
  for (size_t i = 0; i < 3; ++i) assert(output[i] == guest[i]);
  for (size_t i = 3; i < 6; ++i) assert(output[i] == guest[i] + 2);
}

int main() {
  FuzzRebasedVertices<uint16_t>(0x9251624u);
  FuzzRebasedVertices<uint32_t>(0x9253224u);
  InvalidRejectsWithoutWrites();
}

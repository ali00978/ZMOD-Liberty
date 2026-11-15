// G925 v2.34: mixed contiguous/sparse exact-byte upload regression.
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <span>
#include <vector>
#include "graphics/gta4_native/native_sparse_index_batch.h"

using namespace rex::graphics::gta4_native;

template <typename Word>
static void CheckMixedRuns(uint32_t seed) {
  std::mt19937 random(seed);
  constexpr uint32_t kSourceWords = 16384;
  std::vector<uint8_t> source(size_t(kSourceWords) * sizeof(Word) + 2, 0xA5);
  for (size_t i = 1; i + 1 < source.size(); ++i)
    source[i] = static_cast<uint8_t>(random());

  for (unsigned trial = 0; trial < 3200; ++trial) {
    std::vector<NativeSparseIndexRange> ranges;
    const uint32_t length = 2 + random() % 63;
    uint32_t previous_first = 0, previous_end = 0, total = 0;
    for (uint32_t n = 0; n < length; ++n) {
      const uint32_t count = 3 * (1 + random() % 16);
      uint32_t first = random() % (kSourceWords - count + 1);
      const uint32_t kind = random() % 5;
      if (n && kind <= 2 && previous_end + count <= kSourceWords)
        first = previous_end;  // contiguous run of several guest draws
      else if (n && kind == 3 && previous_first + count <= kSourceWords)
        first = previous_first; // repeat/overlap: must not be coalesced
      ranges.push_back({first, count});
      previous_first = first;
      previous_end = first + count;
      total += count;
    }
    assert(total <= kNativeSparseIndexBatchMaxIndices);
    assert(NativeSparseTriangleIndexCount(ranges, kSourceWords) == total);
    std::vector<uint8_t> actual(size_t(total) * sizeof(Word) + 2, 0xC7);
    assert(WriteNativeSparseTriangleIndices(
        {source.data() + 1, size_t(kSourceWords) * sizeof(Word)}, ranges,
        sizeof(Word) == 4, {actual.data() + 1, size_t(total) * sizeof(Word)}));
    size_t offset = 1;
    for (const auto& range : ranges) {
      const size_t bytes = size_t(range.count) * sizeof(Word);
      assert(std::memcmp(actual.data() + offset,
                         source.data() + 1 + size_t(range.first) * sizeof(Word),
                         bytes) == 0);
      offset += bytes;
    }
    assert(offset == 1 + size_t(total) * sizeof(Word));
    assert(actual.front() == 0xC7 && actual.back() == 0xC7);
    assert(source.front() == 0xA5 && source.back() == 0xA5);
  }
}

static void CheckInvalidNeverWrites() {
  const std::array<uint8_t, 64> source{};
  std::array<uint8_t, 64> output{};
  const auto reject = [&](auto ranges, size_t size) {
    output.fill(0xDA);
    assert(!WriteNativeSparseTriangleIndices(source, ranges, false,
                                            std::span<uint8_t>(output).first(size)));
    for (auto value : output) assert(value == 0xDA);
  };
  const std::array<NativeSparseIndexRange, 2> non_triangles{{{0, 4}, {4, 3}}};
  reject(std::span(non_triangles), output.size());
  const std::array<NativeSparseIndexRange, 2> overflow{{{UINT32_MAX, 3}, {0, 3}}};
  reject(std::span(overflow), output.size());
  const std::array<NativeSparseIndexRange, 2> short_output{{{0, 3}, {3, 6}}};
  reject(std::span(short_output), 17);
  const std::array<NativeSparseIndexRange, 2> too_far{{{0, 3}, {30, 6}}};
  reject(std::span(too_far), output.size());
}

int main() {
  CheckMixedRuns<uint16_t>(162925);
  CheckMixedRuns<uint32_t>(322925);
  CheckInvalidNeverWrites();
}

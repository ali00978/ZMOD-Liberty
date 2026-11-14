// G925 2.26 -- host-only validation of lossless, nonadjacent indexed batches.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <span>
#include <vector>
#include "graphics/gta4_native/native_sparse_index_batch.h"

using namespace rex::graphics::gta4_native;

template <typename Word>
static void ExactBatch(uint32_t seed, bool repeated) {
  std::mt19937 rng(seed);
  constexpr uint32_t source_count = 32768;
  std::vector<uint8_t> source(source_count * sizeof(Word) + 2, 0xA5);
  for (uint32_t i = 0; i < source_count; ++i) {
    const Word raw = static_cast<Word>(rng());
    std::memcpy(source.data() + 1 + size_t(i) * sizeof(raw), &raw, sizeof(raw));
  }
  std::vector<NativeSparseIndexRange> ranges;
  uint32_t previous_end = 0;
  for (uint32_t n = 0; n < 20; ++n) {
    const uint32_t count = 3 * (1 + rng() % 120);
    uint32_t first = rng() % (source_count - count);
    if (repeated && n % 4 == 0 && n &&
        uint64_t(ranges[n - 1].first) + count <= source_count)
      first = ranges[n - 1].first;
    else if (!repeated && n % 4 == 0 && n && previous_end + count <= source_count)
      first = previous_end;
    ranges.push_back({first, count});
    previous_end = first + count;
  }
  const auto s = std::span<const NativeSparseIndexRange>(ranges);
  const uint32_t count = NativeSparseTriangleIndexCount(s, source_count);
  assert(count > 0 && count <= kNativeSparseIndexBatchMaxIndices);
  std::vector<uint8_t> out(size_t(count) * sizeof(Word) + 2, 0xC7);
  assert(WriteNativeSparseTriangleIndices(
      std::span<const uint8_t>(source.data() + 1, source_count * sizeof(Word)),
      s, sizeof(Word) == 4,
      std::span<uint8_t>(out.data() + 1, size_t(count) * sizeof(Word))));
  assert(source[0] == 0xA5 && source.back() == 0xA5);
  assert(out[0] == 0xC7 && out.back() == 0xC7);
  size_t at = 0;
  for (const auto& range : ranges) {
    const size_t bytes = size_t(range.count) * sizeof(Word);
    assert(std::memcmp(out.data() + 1 + at,
                       source.data() + 1 + size_t(range.first) * sizeof(Word),
                       bytes) == 0);
    at += bytes;
  }
  assert(at == size_t(count) * sizeof(Word));
  // Every original triangle index triplet is still in original draw order.
  // This is the mathematical equivalence required for one triangle-list draw.
  for (size_t tri = 0; tri < count; tri += 3) {
    for (size_t k = 0; k < 3; ++k) {
      Word value{};
      std::memcpy(&value, out.data() + 1 + (tri + k) * sizeof(Word), sizeof(Word));
      (void)value;
    }
  }
}

static void InvalidNeverWrites() {
  const std::array<uint8_t, 128> source{};
  std::array<uint8_t, 128> output{};
  const auto reject = [&](auto ranges, size_t destination_bytes) {
    output.fill(0xCE);
    assert(!WriteNativeSparseTriangleIndices(
        source, ranges, false, std::span<uint8_t>(output).first(destination_bytes)));
    for (uint8_t value : output) assert(value == 0xCE);
  };
  const std::array<NativeSparseIndexRange, 2> past_source{{{0, 3}, {63, 6}}};
  reject(std::span(past_source), output.size());
  const std::array<NativeSparseIndexRange, 2> empty{{{0, 3}, {3, 0}}};
  reject(std::span(empty), output.size());
  const std::array<NativeSparseIndexRange, 2> partial{{{0, 4}, {4, 3}}};
  reject(std::span(partial), output.size());
  const std::array<NativeSparseIndexRange, 2> overflow{{{UINT32_MAX, 3}, {0, 3}}};
  reject(std::span(overflow), output.size());
  const std::array<NativeSparseIndexRange, 2> small{{{0, 3}, {3, 6}}};
  reject(std::span(small), 17);
  assert(NativeSparseTriangleIndexCount(std::span(small), 64) == 9);
  assert(!NativeSparseTriangleBatchHasGap(std::span(small)));
  const std::array<NativeSparseIndexRange, 2> skipped{{{0, 3}, {9, 6}}};
  assert(NativeSparseTriangleBatchHasGap(std::span(skipped)));
  const std::array<NativeSparseIndexRange, 2> large{{{0, 65535}, {0, 3}}};
  assert(NativeSparseTriangleIndexCount(std::span(large), UINT32_MAX) == 0);
  std::array<NativeSparseIndexRange, 65> too_many{};
  too_many.fill({0, 3});
  assert(NativeSparseTriangleIndexCount(std::span(too_many), 64) == 0);
}

int main() {
  InvalidNeverWrites();
  for (uint32_t seed = 0; seed < 512; ++seed) {
    ExactBatch<uint16_t>(seed, false);
    ExactBatch<uint16_t>(seed, true);
    ExactBatch<uint32_t>(seed, false);
    ExactBatch<uint32_t>(seed, true);
  }
  std::cout << "PASS: G925 v2.26 exact sparse 16/32-bit triangle index sequences, bounds, and no-partial-write fallbacks\n";
}

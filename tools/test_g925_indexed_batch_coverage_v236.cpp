// G925 2.36: expanded medium-mesh draw batching remains byte-exact.
// Host executable: no GPU or Android dependency.
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>
#include <span>
#include <vector>
#include "graphics/gta4_native/native_sparse_index_batch.h"

using namespace rex::graphics::gta4_native;
static_assert(kNativePairedSparseBatchMaxIndices == 1536);
static_assert(kNativeSmallSparseBatchMaxIndices == 3072);
static_assert(ShouldBatchNativeSparseIndexDraws(2, 1, 1536, true));
static_assert(!ShouldBatchNativeSparseIndexDraws(2, 1, 1537, true));
static_assert(ShouldBatchNativeSparseIndexDraws(3, 1, 3072, true));
static_assert(!ShouldBatchNativeSparseIndexDraws(3, 1, 3073, true));
static_assert(!ShouldBatchNativeSparseIndexDraws(2, 1, 1536, false));
static_assert(!ShouldBatchNativeSparseIndexDraws(3, 1, 3072, false));
static_assert(!ShouldBatchNativeSparseIndexDraws(3, 2, 1800, true));
static_assert(!ShouldBatchNativeSparseIndexDraws(4, 3, 2000, true));
static_assert(ShouldBatchNativeSparseIndexDraws(4, 2, 2000, true));

template <typename Index>
static void Replay(uint32_t seed, size_t draw_count) {
  std::mt19937 random(seed);
  std::vector<Index> source(16384);
  for (auto& value : source) value = Index(random());
  uint64_t eligible_new = 0, eligible_old = 0;
  for (unsigned trial = 0; trial < 9000; ++trial) {
    std::vector<NativeSparseIndexRange> ranges;
    uint32_t total = 0;
    for (size_t d = 0; d < draw_count; ++d) {
      // Medium meshes; many fall outside legacy 384/1024-index limits.
      const uint32_t count = 3u * (40u + random() % 230u);
      const uint32_t first = uint32_t(128 + d * 4000 + random() % 500);
      ranges.push_back({first, count});
      total += count;
    }
    assert(NativeSparseTriangleBatchHasGap(ranges));
    assert(NativeSparseTriangleIndexCount(ranges, source.size()) == total);
    const bool fast = ShouldBatchNativeSparseIndexDraws(draw_count, 1, total, true);
    const uint32_t old_limit = draw_count == 2 ? 384 : 1024;
    eligible_old += (total <= old_limit);
    eligible_new += fast;
    if (!fast) continue;
    std::vector<Index> destination(total + 2, Index(0xCECE));
    const auto input = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(source.data()), source.size() * sizeof(Index));
    const auto output = std::span<uint8_t>(
        reinterpret_cast<uint8_t*>(destination.data()), total * sizeof(Index));
    assert(WriteNativeSparseTriangleIndices(
        input, ranges, sizeof(Index) == sizeof(uint32_t), output));
    assert(destination[total] == Index(0xCECE));
    assert(destination[total + 1] == Index(0xCECE));
    size_t cursor = 0;
    for (const auto& range : ranges) {
      const auto* expected = source.data() + range.first;
      assert(std::memcmp(destination.data() + cursor, expected,
                         size_t(range.count) * sizeof(Index)) == 0);
      cursor += range.count;
    }
    assert(cursor == total);
  }
  // More candidate submissions become one draw, but this is an *eligibility*
  // replay, not proof of improved GPU FPS on a physical device.
  assert(eligible_new > eligible_old);
  assert(eligible_new > 1000);
}

static void InvalidInputNeverWrites() {
  const std::array<uint8_t, 64> source{};
  std::array<uint8_t, 64> out{};
  out.fill(0xA5);
  const std::array<NativeSparseIndexRange, 2> bad{{{0, 4}, {4, 3}}};
  assert(!WriteNativeSparseTriangleIndices(source, bad, false, out));
  for (uint8_t b : out) assert(b == 0xA5);
  const std::array<NativeSparseIndexRange, 2> too_far{{{0, 3}, {30, 6}}};
  assert(!WriteNativeSparseTriangleIndices(source, too_far, false, out));
  for (uint8_t b : out) assert(b == 0xA5);
}

int main() {
  Replay<uint16_t>(23616, 2);
  Replay<uint32_t>(23632, 2);
  Replay<uint16_t>(23617, 3);
  Replay<uint32_t>(23633, 3);
  InvalidInputNeverWrites();
}

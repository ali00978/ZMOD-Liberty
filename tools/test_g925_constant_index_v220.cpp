// G925 v2.20 exact single-probe constant reservations and bulk restart upload.
// Host-only C++20 tests: no Vulkan driver or Android device required.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>
#include "graphics/gta4_native/frame_constant_arena.h"
#include "graphics/gta4_native/native_index_conversion.h"

using namespace rex::graphics::gta4_native;

struct CountingHash {
  static inline size_t calls = 0;
  size_t operator()(uint32_t key) const noexcept { ++calls; return key; }
};
struct CollidingHash {
  size_t operator()(uint32_t) const noexcept { return 7; }
};

static void TestOnePassReservationFactory() {
  // An invalid first reservation must not create an unused hash table.
  FrameGenerationMap<uint32_t, uint32_t, CountingHash> empty_cache;
  CountingHash::calls = 0;
  auto no_capacity = empty_cache.FindOrInsertWith(7, []() -> std::optional<uint32_t> {
    return std::nullopt;
  });
  assert(!no_capacity && empty_cache.size() == 0);
  assert(empty_cache.bucket_count() == 0 && CountingHash::calls == 0);
  FrameGenerationMap<uint32_t, uint32_t, CountingHash> cache;
  CountingHash::calls = 0;
  size_t factory_calls = 0;
  for (uint32_t i = 0; i < 12; ++i) {
    const auto result = cache.FindOrInsertWith(i + 1, [&]() -> std::optional<uint32_t> {
      ++factory_calls;
      return i + 100;
    });
    assert(result && result.inserted && *result.value == i + 100);
    assert(CountingHash::calls == i + 1);
  }
  assert(factory_calls == 12);
  auto repeated = cache.FindOrInsertWith(2, [&]() -> std::optional<uint32_t> {
    ++factory_calls;
    return 999;
  });
  assert(repeated && !repeated.inserted && *repeated.value == 101);
  assert(factory_calls == 12 && CountingHash::calls == 13);
  const size_t buckets_before_reject = cache.bucket_count();
  const size_t count_before_reject = cache.size();
  auto reject = cache.FindOrInsertWith(900, [&]() -> std::optional<uint32_t> {
    ++factory_calls;
    return std::nullopt;
  });
  assert(!reject && cache.size() == count_before_reject);
  assert(cache.bucket_count() == buckets_before_reject);
  assert(CountingHash::calls == 14 && factory_calls == 13);
  // Once the 32 buckets are at half load, a genuine insertion grows the
  // table; a duplicate must NOT grow, and a rejected insertion is a no-op.
  for (uint32_t i = 13; i <= 16; ++i)
    assert(cache.FindOrInsertWith(i, [i]() { return std::optional<uint32_t>{1000 + i}; }));
  const auto old_count = cache.bucket_count();
  assert(cache.FindOrInsertWith(13, [] { return std::optional<uint32_t>{9999}; }));
  assert(cache.bucket_count() == old_count);
  assert(cache.FindOrInsertWith(17, [] { return std::optional<uint32_t>{1017}; }));
  assert(cache.bucket_count() > old_count);
  assert(*cache.Find(2) == 101);
}

static void TestCollisionsAndGenerations() {
  FrameGenerationMap<uint32_t, uint32_t, CollidingHash> cache;
  for (uint32_t generation = 1; generation <= 100; ++generation) {
    std::unordered_map<uint32_t, uint32_t> reference;
    for (uint32_t i = 0; i < 96; ++i) {
      const uint32_t key = (i * 29) % 96;
      auto result = cache.FindOrInsertWith(key, [i, generation]() {
        return std::optional<uint32_t>{generation * 1000 + i};
      });
      assert(result && result.inserted);
      reference[key] = generation * 1000 + i;
    }
    for (auto [key, value] : reference) {
      auto result = cache.FindOrInsertWith(key, [] {
        assert(false && "factory executed on an existing key");
        return std::optional<uint32_t>{};
      });
      assert(result && !result.inserted && *result.value == value);
    }
    assert(cache.size() == reference.size());
    assert(cache.ResetGeneration());
    assert(cache.size() == 0);
    for (auto [key, value] : reference) {
      (void)value;
      assert(!cache.Find(key));
    }
  }
}

static void TestArenaFencesAndFullReuse() {
  FrameConstantArenaIndex arena;
  assert(arena.SetByteCapacity(160));
  const FrameConstantIdentity a{FrameConstantKind::kVertex, 1};
  const FrameConstantIdentity b{FrameConstantKind::kPixel, 2};
  const FrameConstantIdentity c{FrameConstantKind::kShared, 3};
  auto first = arena.FindOrReserve(a, 64, 32);
  auto second = arena.FindOrReserve(b, 96, 32);
  assert(first && !first->reused && first->offset == 0);
  assert(second && !second->reused && second->offset == 64);
  assert(arena.bytes_used() == 160 && arena.reservation_count() == 2);
  auto same = arena.FindOrReserve(a, 64, 32);
  assert(same && same->reused && same->offset == 0);
  assert(!arena.FindOrReserve(a, 65, 32)); // reject wrong size without mutation
  assert(!arena.FindOrReserve(b, 96, 128)); // reject misaligned old reservation
  assert(!arena.FindOrReserve(c, 4, 4)); // no capacity
  assert(arena.bytes_used() == 160 && arena.reservation_count() == 2);
  assert(arena.MarkSubmitted(91));
  assert(!arena.ResetAfterCompletion(90) && !arena.ResetUnsubmitted());
  assert(arena.ResetAfterCompletion(91));
  assert(!arena.Find(a) && arena.bytes_used() == 0);
  auto next = arena.FindOrReserve(c, 8, 8);
  assert(next && !next->reused && next->offset == 0);
  assert(arena.ResetUnsubmitted());
}

static void TestByteExactRestartConversion(bool wide, bool dense, uint32_t seed) {
  std::mt19937 rng(seed);
  const size_t width = wide ? 4 : 2;
  const uint32_t count = 2048;
  const uint32_t first = 3;
  std::vector<uint32_t> words(count + first + 1);
  const uint32_t sentinel = wide ? 0x00FFFFFFu : 0x1234u;
  const uint32_t mask = wide ? 0x00FFFFFFu : 0u;
  for (size_t i = 0; i < words.size(); ++i) {
    uint32_t value = rng();
    if (!wide) value &= UINT16_MAX;
    const bool match = dense ? (i % 3 != 0) : (i % 509 == 0);
    if (match) value = wide ? ((value & 0xFF000000u) | sentinel) : sentinel;
    words[i] = value;
  }
  std::vector<uint8_t> src(words.size() * width + 2, 0xE5);
  for (size_t i = 0; i < words.size(); ++i) {
    if (wide) std::memcpy(src.data() + 1 + i * width, &words[i], width);
    else {
      uint16_t small = uint16_t(words[i]);
      std::memcpy(src.data() + 1 + i * width, &small, width);
    }
  }
  std::vector<uint8_t> dst(count * width + 2, 0xD7);
  const NativeConvertedIndexKey key{1, first, count, sentinel, mask,
                                    NativeIndexConversion::kRestart, wide};
  const auto input = std::span<const uint8_t>(src.data() + 1, words.size() * width);
  const auto output = std::span<uint8_t>(dst.data() + 1, count * width);
  assert(WriteNativeConvertedIndices(input, key, output));
  assert(dst.front() == 0xD7 && dst.back() == 0xD7);
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t value = words[i + first];
    const uint32_t expected = ((wide ? (value & mask) : value) == sentinel)
        ? (wide ? UINT32_MAX : UINT16_MAX) : value;
    uint32_t actual = 0;
    if (wide) std::memcpy(&actual, output.data() + size_t(i) * width, width);
    else {
      uint16_t small;
      std::memcpy(&small, output.data() + size_t(i) * width, width);
      actual = small;
    }
    assert(actual == expected);
  }
  std::fill(dst.begin(), dst.end(), 0xD7);
  assert(!WriteNativeConvertedIndices(input, key,
                                      std::span<uint8_t>(dst.data() + 1, count * width - 1)));
  assert(std::all_of(dst.begin(), dst.end(), [](uint8_t b) { return b == 0xD7; }));
}

int main() {
  TestOnePassReservationFactory();
  TestCollisionsAndGenerations();
  TestArenaFencesAndFullReuse();
  for (uint32_t seed = 1; seed <= 50; ++seed)
    for (bool wide : {false, true})
      for (bool dense : {false, true})
        TestByteExactRestartConversion(wide, dense, seed * 71093);
  std::cout << "PASS: one-hash lazy reservations, 100 collision generations, "
               "GPU fences and 200 byte-exact 2048-index restart replays\n";
}

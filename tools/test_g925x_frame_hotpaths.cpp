// Production helpers: independent byte oracle and descriptor lifetime checks.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_constant_upload.h"
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_descriptor_tuple_cache.h"
using namespace rex::graphics::gta4_native;
static void UploadReplay() {
  std::mt19937 rng(0x92506);
  NativeConstantUploadTracker tracker;
  std::vector<uint8_t> actual(8192, 0xCD), expected = actual;
  size_t prefix = 0, frame_written = 0;
  for (unsigned trial = 0; trial < 20000; ++trial) {
    if (trial % 37 == 0) { tracker.BeginFrame(); frame_written = 0; }
    if (trial % 199 == 0) { tracker.ResetStorage(); prefix = frame_written = 0; }
    size_t bytes = (1 + rng() % 32) * 16;
    size_t offset = (rng() % ((actual.size() - bytes) / 16 + 1)) * 16;
    bool guest = rng() & 1, compare = rng() & 1;
    std::vector<uint8_t> src(bytes + 1);
    for (size_t i = 0; i < bytes; ++i) {
      const size_t mapped = guest ? (i ^ 3u) : i;
      src[1 + i] = (trial % 3 == 0 || rng() % 17 != 0)
          ? expected[offset + mapped] : static_cast<uint8_t>(rng());
    }
    size_t count = 0;
    for (size_t i = 0; i < bytes; i += 16) {
      std::array<uint8_t, 16> reg{};
      for (size_t j = 0; j < 16; ++j)
        reg[j] = src[1 + i + (guest ? (j ^ 3u) : j)];
      auto dst = expected.begin() + offset + i;
      if (!compare || offset + i >= prefix || !std::equal(reg.begin(), reg.end(), dst))
        count += 16;
      std::copy(reg.begin(), reg.end(), dst);
    }
    prefix = std::max(prefix, offset + bytes);
    frame_written += count;
    auto result = tracker.Write(actual, offset, {src.data() + 1, bytes}, guest, compare);
    assert(result && *result == count && actual == expected);
    assert(tracker.written_bytes() == frame_written && tracker.initialized_bytes() == prefix);
  }
}
struct Hash {
  static inline size_t calls = 0;
  size_t operator()(uint64_t k) const noexcept { ++calls; return k; }
};
struct Deps {
  std::array<uint64_t, 1> operator()(uint64_t k) const { return {k}; }
};
static void DescriptorReplay() {
  NativeDescriptorTupleCache<uint64_t, uint64_t, Hash, Deps> cache(16);
  assert(cache.BeginAfterCommandPoolReset(0));
  for (uint64_t i = 1; i <= 4; ++i) assert(cache.Insert(i, i * 100));
  std::unordered_set<uint64_t, Hash> small{1, 2, 8, 9};
  Hash::calls = 0;
  cache.Prepare(small);
  assert(Hash::calls == 0 && cache.size() == 4);
  for (uint64_t i = 5; i <= 16; ++i) assert(cache.Insert(i, i * 100));
  std::unordered_set<uint64_t, Hash> mixed{1, 2, 17, 18, 19};
  cache.Prepare(mixed);
  assert(cache.size() == 13 && cache.reusable_count() == 3);
  assert(*cache.Find(1) == 100 && *cache.Find(2) == 200);
  assert(cache.Insert(17, 1700));
  cache.MarkSubmitted(9);
  cache.InvalidateDependency(17);
  assert(!cache.Find(17) && cache.retired_count() == 1);
  assert(!cache.BeginAfterCommandPoolReset(8));
  assert(cache.BeginAfterCommandPoolReset(9));
  assert(cache.retired_count() == 0 && cache.reusable_count() == 4);
}
int main() {
  UploadReplay();
  DescriptorReplay();
  std::cout << "PASS: 20000 exact shader uploads and descriptor hash-free capacity/lifetime replay\n";
}

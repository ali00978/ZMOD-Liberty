// G925 2.22: high-frequency recorder lookup and raster memo correctness.
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include "graphics/gta4_native/native_draw_lookup_cache.h"
#include "graphics/gta4_native/native_exact_memo.h"

using namespace rex::graphics::gta4_native;

struct CountingKey {
  uint32_t id = 0;
  static inline uint64_t comparisons = 0;
  bool operator==(const CountingKey& other) const {
    ++comparisons;
    return id == other.id;
  }
};

static void TestRasterGetPromotesConsumedEntry() {
  NativeExactMemo<CountingKey, uint64_t> cache;
  uint64_t calculated = 0;
  const auto get = [&](uint32_t key) -> const uint64_t& {
    return cache.Get({key}, [&]() { ++calculated; return uint64_t(key) * 10; });
  };
  assert(get(1) == 10 && get(2) == 20 && calculated == 2);
  // The old implementation would leave B as "most recently used" after
  // consuming A, and incorrectly evict A on the next C miss.
  assert(get(1) == 10 && calculated == 2);
  CountingKey::comparisons = 0;
  assert(get(3) == 30 && calculated == 3);
  assert(CountingKey::comparisons == 2); // each valid key compared just once
  assert(get(1) == 10 && calculated == 3); // A survived because it was consumed
  assert(cache.Find({2})); // Four-way cache also retains B after introducing C.
  assert(get(2) == 20 && calculated == 3);
  assert(get(1) == 10 && calculated == 3);

  // A rejected compute must not evict an existing valid raster area.
  try {
    (void)cache.Get({77}, []() -> uint64_t {
      throw std::runtime_error("failed raster computation");
    });
    assert(false && "expected exception");
  } catch (const std::runtime_error&) {
    assert(cache.Find({1}) && cache.Find({2}));
    assert(!cache.Find({77}));
  }
  // Find stays a non-promoting peek: old Store callers keep their eviction
  // behavior and snapshots are independent.
  cache.Reset();
  cache.Store({1}, 11);
  cache.Store({2}, 22);
  assert(cache.Find({1}) && *cache.Find({1}) == 11);
  cache.Store({3}, 33);
  cache.Store({4}, 44);
  assert(cache.Find({1}) && cache.Find({2}) && cache.Find({3}) && cache.Find({4}));
  cache.Store({5}, 55);
  assert(!cache.Find({1}) && cache.Find({2}) && cache.Find({3}) &&
         cache.Find({4}) && cache.Find({5}));
  auto copy = cache;
  cache.Reset();
  assert(!cache.Find({2}) && !cache.Find({3}) && !cache.Find({5}));
  assert(copy.Find({2}) && copy.Find({3}) && copy.Find({5}));
}

struct Image { uint32_t id = 0; };

static NativeSurfaceLookupKey Surface(uint32_t handle, bool depth, uint32_t variant) {
  NativeSurfaceLookupKey result;
  result.descriptor = {handle, 11, 12, 13, 14, variant, 1920, 1080, 16};
  result.samples = 4;
  result.depth = depth;
  return result;
}

static void TestShortRejectPreservesExactSurfaceIdentity() {
  NativeSurfaceLookupCache<Image, 2> cache; // intentional one-set collision
  Image color{11}, otherColor{12}, depth{13};
  auto a = Surface(1337, false, 19);
  auto b = Surface(1337, false, 20); // same handle, depth and samples!
  cache.Store(a, 1, &color);
  cache.Store(b, 1, &otherColor);
  for (int i = 0; i < 50000; ++i) {
    assert(cache.Find(a, 1) == &color);
    assert(cache.Find(b, 1) == &otherColor);
  }
  for (size_t field = 0; field < a.descriptor.size(); ++field) {
    auto different = a;
    ++different.descriptor[field];
    // Field 5 turns a into b when i==5? a.format=19, b.format=20.
    if (different == b) continue;
    assert(cache.Find(different, 1) == nullptr);
  }
  auto otherRole = a;
  otherRole.depth = true;
  assert(cache.Find(otherRole, 1) == nullptr);
  otherRole = a;
  otherRole.samples = 8;
  assert(cache.Find(otherRole, 1) == nullptr);
  assert(cache.Find(a, 2) == nullptr);
  cache.Invalidate();
  assert(!cache.Find(a, 1) && !cache.Find(b, 1));

  // Typical alternating color/depth hits have different handle and role.
  NativeSurfaceLookupCache<Image> frame;
  a = Surface(100, false, 9);
  b = Surface(200, true, 9);
  frame.Store(a, 7, &color);
  frame.Store(b, 7, &depth);
  for (int i = 0; i < 100000; ++i) {
    assert(frame.Find(a, 7) == &color);
    assert(frame.Find(b, 7) == &depth);
  }
  frame.Invalidate();
  assert(!frame.Find(a, 7) && !frame.Find(b, 7));
}

int main() {
  TestRasterGetPromotesConsumedEntry();
  TestShortRejectPreservesExactSurfaceIdentity();
  std::cout << "PASS: true raster-Get MRU, one-pass miss, exception safety, "
               "150k exact surface alternations, revisions, epochs and key collisions\n";
}

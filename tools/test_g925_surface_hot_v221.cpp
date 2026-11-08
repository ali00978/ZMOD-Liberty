// G925 2.21: high-frequency color/depth surface lookup correctness replay.
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include "graphics/gta4_native/native_draw_lookup_cache.h"

using namespace rex::graphics::gta4_native;
struct Image { uint32_t id; };

static NativeSurfaceLookupKey Key(uint32_t handle, uint32_t fmt = 0) {
  NativeSurfaceLookupKey key;
  key.descriptor = {handle, 11, 13, 17, 19, fmt, 1280, 720, 2};
  key.samples = 4;
  return key;
}

static void TestAlternatingHotLookup() {
  NativeSurfaceLookupCache<Image> cache;
  Image color{1}, depth{2}, colorReplacement{3};
  auto a = Key(100, 7), b = Key(200, 8);
  b.depth = true;
  assert(!cache.Find(a, 1) && !cache.Find(b, 1));
  cache.Store(a, 1, &color);
  cache.Store(b, 1, &depth);
  for (size_t n = 0; n < 100000; ++n) {
    assert(cache.Find(a, 1) == &color);
    assert(cache.Find(b, 1) == &depth);
  }
  assert(!cache.Find(a, 2) && !cache.Find(b, 2));
  auto changed = a;
  changed.descriptor[8] ^= 1;
  assert(!cache.Find(changed, 1));
  changed = a; ++changed.descriptor[5];
  assert(!cache.Find(changed, 1));
  changed = b; changed.depth = false;
  assert(!cache.Find(changed, 1));

  // Updating a surface under a new published registry revision must never
  // expose the old hot pointer. An identical key must return its new image.
  cache.Store(a, 2, &colorReplacement);
  assert(cache.Find(a, 2) == &colorReplacement);
  assert(cache.Find(a, 1) == nullptr);
  assert(cache.Find(b, 2) == nullptr);
  cache.Store(b, 2, &depth);
  for (size_t n = 0; n < 100000; ++n) {
    assert(cache.Find(a, 2) == &colorReplacement);
    assert(cache.Find(b, 2) == &depth);
  }
  cache.Invalidate();
  assert(!cache.Find(a, 2) && !cache.Find(b, 2));
  cache.Store(a, 3, &color);
  assert(cache.Find(a, 3) == &color);
  assert(!cache.Find(a, 2));
}

static void TestFourSurfaceHotSet() {
  NativeSurfaceLookupCache<Image, 256> cache;
  Image images[5]{{1}, {2}, {3}, {4}, {5}};
  std::array<NativeSurfaceLookupKey, 5> keys;
  for (size_t i = 0; i < keys.size(); ++i) {
    keys[i] = Key(800 + uint32_t(i * 59), 4 + uint32_t(i));
    keys[i].depth = i % 2 == 1;
  }
  for (size_t i = 0; i < 4; ++i) cache.Store(keys[i], 42, &images[i]);
  // Four active surface types must be retained exactly; in particular no
  // stale image may be read after a new registry revision or an epoch reset.
  for (size_t repeat = 0; repeat < 10000; ++repeat)
    for (size_t i = 0; i < 4; ++i)
      assert(cache.Find(keys[i], 42) == &images[i]);
  cache.Store(keys[4], 42, &images[4]);
  assert(cache.Find(keys[4], 42) == &images[4]);
  for (size_t i = 1; i < 4; ++i)
    assert(cache.Find(keys[i], 42) == &images[i]);
  assert(cache.Find(keys[0], 43) == nullptr);
  cache.Invalidate();
  for (const auto& key : keys) assert(!cache.Find(key, 42));
}

static void TestHotCacheEvictionAndCollision() {
  // A one-way cache is intentionally incompatible with multiple identities.
  // The two-entry hot cache must NOT expose a victim evicted from its single
  // backing entry, even if the image pointer has not yet been destroyed.
  NativeSurfaceLookupCache<Image, 1> one;
  Image x{10}, y{20};
  const auto a = Key(11), b = Key(22);
  one.Store(a, 5, &x);
  assert(one.Find(a, 5) == &x);
  one.Store(b, 5, &y);
  assert(one.Find(a, 5) == nullptr);
  assert(one.Find(b, 5) == &y);

  NativeSurfaceLookupCache<Image, 2> two;
  Image imgs[8]{{0},{1},{2},{3},{4},{5},{6},{7}};
  std::array<NativeSurfaceLookupKey, 8> keys;
  for (size_t i = 0; i < keys.size(); ++i) keys[i] = Key(100 + uint32_t(i));
  for (size_t i = 0; i < keys.size(); ++i) {
    two.Store(keys[i], 9, &imgs[i]);
    assert(two.Find(keys[i], 9) == &imgs[i]);
    for (size_t j = 0; j < i; ++j)
      if (auto* cached = two.Find(keys[j], 9))
        assert(cached == &imgs[j]);
  }
  two.Invalidate();
  for (const auto& key : keys) assert(!two.Find(key, 9));
}

static void TestChurnAndEpochSafety() {
  NativeSurfaceLookupCache<Image> cache;
  Image imgs[1000]{};
  std::mt19937 rng(92521);
  uint64_t revision = 1;
  for (size_t i = 0; i < 1000; ++i) {
    imgs[i].id = uint32_t(i);
    auto key = Key(uint32_t(rng() % 64), uint32_t(rng() % 32));
    key.descriptor[3] = uint32_t(i);
    cache.Store(key, revision, &imgs[i]);
    assert(cache.Find(key, revision) == &imgs[i]);
    assert(cache.Find(key, revision + 1) == nullptr);
    if (i % 13 == 0) {
      ++revision;
      cache.Invalidate();
      assert(!cache.Find(key, revision));
    }
  }
  cache.Invalidate();
  for (size_t i = 0; i < 2000; ++i) {
    auto key = Key(uint32_t(i % 47), uint32_t(i % 7));
    cache.Store(key, uint64_t(i + 1000), &imgs[i % 1000]);
    assert(cache.Find(key, uint64_t(i + 1000)) == &imgs[i % 1000]);
    cache.Invalidate();
    assert(!cache.Find(key, uint64_t(i + 1000)));
  }
}

int main() {
  TestAlternatingHotLookup();
  TestFourSurfaceHotSet();
  TestHotCacheEvictionAndCollision();
  TestChurnAndEpochSafety();
  std::cout << "PASS: 200000 alternating color/depth hot hits, exact revision, "
               "eviction, collision and 2000 lifetime resets\n";
}

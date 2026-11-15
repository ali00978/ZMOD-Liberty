// G925 2.35: role-aware hot-surface lookup must preserve full identity and
// the previously proven Vulkan object lifetime / revision guarantees.
#include <array>
#include <cassert>
#include <cstdint>
#include <random>
#include "graphics/gta4_native/native_draw_lookup_cache.h"

using namespace rex::graphics::gta4_native;
struct Image { uint32_t id; };

static NativeSurfaceLookupKey Key(uint32_t handle, bool depth, uint32_t variant) {
  NativeSurfaceLookupKey key;
  key.descriptor = {handle, 11, 13, 17, 19, variant, 1920, 1080, 2};
  key.samples = 4;
  key.depth = depth;
  return key;
}

static void AlternateRolesAndVariants() {
  NativeSurfaceLookupCache<Image, 256> cache;
  std::array<Image, 5> images{{{1}, {2}, {3}, {4}, {5}}};
  const auto a = Key(101, false, 7);
  const auto b = Key(202, true, 8);
  const auto a2 = Key(101, false, 9); // same handle/role, distinct format
  const auto b2 = Key(202, true, 10);
  cache.Store(a, 1, &images[0]);
  cache.Store(b, 1, &images[1]);
  cache.Store(a2, 1, &images[2]);
  cache.Store(b2, 1, &images[3]);
  for (int i = 0; i < 200000; ++i) {
    assert(cache.Find(a, 1) == &images[0]);
    assert(cache.Find(b, 1) == &images[1]);
    assert(cache.Find(a2, 1) == &images[2]);
    assert(cache.Find(b2, 1) == &images[3]);
  }
  // A same-role hit that differs only in a late descriptor word must not
  // return the last-slot shortcut and must never leak a different image.
  for (size_t word = 0; word < a.descriptor.size(); ++word) {
    auto changed = a;
    changed.descriptor[word] ^= 1u << (word % 12);
    assert(cache.Find(changed, 1) == nullptr);
  }
  auto wrong_samples = a; wrong_samples.samples = 8;
  auto wrong_role = a; wrong_role.depth = true;
  assert(cache.Find(wrong_samples, 1) == nullptr);
  assert(cache.Find(wrong_role, 1) == nullptr);
  assert(cache.Find(a, 2) == nullptr);

  // A new published registry revision must not expose the old generation.
  cache.Store(a, 2, &images[4]);
  assert(cache.Find(a, 2) == &images[4]);
  assert(cache.Find(a, 1) == nullptr);
  assert(cache.Find(a2, 2) == nullptr);
  cache.Invalidate();
  assert(cache.Find(a, 2) == nullptr);
  assert(cache.Find(b, 1) == nullptr);
}

static void EvictionCannotResurrectRoleHint() {
  NativeSurfaceLookupCache<Image, 1> cache;
  Image color{11}, depth{12}, new_color{13};
  const auto a = Key(10, false, 1);
  const auto b = Key(20, true, 2);
  const auto c = Key(30, false, 3);
  cache.Store(a, 1, &color);
  assert(cache.Find(a, 1) == &color);
  cache.Store(b, 1, &depth);
  assert(cache.Find(a, 1) == nullptr);
  assert(cache.Find(b, 1) == &depth);
  cache.Store(c, 1, &new_color);
  assert(cache.Find(b, 1) == nullptr);
  assert(cache.Find(c, 1) == &new_color);
  cache.Invalidate();
  for (auto key : {a, b, c}) assert(cache.Find(key, 1) == nullptr);
}

static void RandomChurnNeverGivesWrongImage() {
  NativeSurfaceLookupCache<Image, 64> cache;
  std::array<Image, 128> images{};
  std::array<NativeSurfaceLookupKey, 64> keys{};
  std::array<Image*, 64> latest{};
  for (size_t i = 0; i < keys.size(); ++i) {
    keys[i] = Key(1000 + uint32_t(i), i % 2 != 0, uint32_t(i));
    latest[i] = nullptr;
  }
  std::mt19937 rng(235925);
  for (size_t i = 0; i < 120000; ++i) {
    if (i % 1997 == 0) {
      cache.Invalidate();
      latest.fill(nullptr);
    }
    const size_t which = rng() % keys.size();
    if ((rng() & 7u) == 0) {
      Image* image = &images[rng() % images.size()];
      cache.Store(keys[which], 55, image);
      latest[which] = image;
    } else {
      Image* found = cache.Find(keys[which], 55);
      assert(!found || found == latest[which]);
    }
  }
}

int main() {
  AlternateRolesAndVariants();
  EvictionCannotResurrectRoleHint();
  RandomChurnNeverGivesWrongImage();
}

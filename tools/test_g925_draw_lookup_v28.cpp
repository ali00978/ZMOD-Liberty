#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string_view>
#include "graphics/gta4_native/native_performance_samples.h"
#include "graphics/gta4_native/native_draw_lookup_cache.h"
using namespace rex::graphics::gta4_native;

struct Descriptor {
  uint32_t handle, flags, base, address, packed_dimensions, format, width, height, sample_type;
};
struct Image { uint64_t generation; };
int main() {
  using namespace rex::graphics::gta4_native::performance;
  assert(std::string_view(CounterName(Counter::kSurfaceLookupReuses)) == "surface-lookup-reuses");
  assert(std::string_view(CounterName(Counter::kVertexLayoutReuses)) == "vertex-layout-reuses");
  for (size_t i = 0; i < kCounterCount; ++i) {
    assert(std::string_view(CounterName(Counter(i))) != "unknown");
    assert(std::string_view(CounterName(Counter(i))).size());
  }
  const Descriptor descriptor{0x1000, 1, 2, 3, 4, 5, 1280, 720, 2};
  const auto key = MakeNativeSurfaceLookupKey(descriptor, false, 4);
  assert((key.descriptor == std::array<uint32_t, 9>{0x1000, 1, 2, 3, 4, 5, 1280, 720, 2}));
  NativeSurfaceLookupCache<Image> surfaces;
  Image image{1};
  assert(!surfaces.Find(key, 1));
  surfaces.Store(key, 1, &image);
  assert(surfaces.Find(key, 1) == &image);
  for (size_t i = 0; i < key.descriptor.size(); ++i) {
    auto changed = key;
    ++changed.descriptor[i];
    assert(!surfaces.Find(changed, 1));
  }
  auto changed = key;
  changed.depth = true;
  assert(!surfaces.Find(changed, 1));
  changed = key; changed.samples = 1;
  assert(!surfaces.Find(changed, 1));
  assert(!surfaces.Find(key, 2));
  surfaces.Invalidate();
  assert(!surfaces.Find(key, 1));
  surfaces.Store(key, 1, nullptr);
  assert(!surfaces.Find(key, 1));
  // Force all keys into one slot. Collisions must miss, never alias images.
  NativeSurfaceLookupCache<Image, 1> collisions;
  collisions.Store(key, 1, &image);
  collisions.Store(changed, 1, &image);
  assert(!collisions.Find(key, 1));
  assert(collisions.Find(changed, 1) == &image);

  // Two exact keys with the same original surface handle and same cache-set
  // hash must coexist; the old direct-mapped cache thrashed on every A/B draw.
  NativeSurfaceLookupCache<Image, 2> two_way;
  Image alternate_image{2}, third_image{3};
  auto alternate = key;
  alternate.descriptor[5] ^= 7;  // Format changes, handle and hash unchanged.
  auto third = key;
  third.descriptor[7] += 5;  // Geometry changes, handle and hash unchanged.
  two_way.Store(key, 1, &image);
  two_way.Store(alternate, 1, &alternate_image);
  for (size_t i = 0; i < 500; ++i) {
    assert(two_way.Find(key, 1) == &image);
    assert(two_way.Find(alternate, 1) == &alternate_image);
  }
  two_way.Store(third, 1, &third_image);
  assert(two_way.Find(third, 1) == &third_image);
  assert((two_way.Find(key, 1) != nullptr) !=
         (two_way.Find(alternate, 1) != nullptr));
  two_way.Store(key, 2, &image);  // New registry revision invalidates old keys.
  assert(two_way.Find(key, 2) == &image);
  assert(!two_way.Find(alternate, 2));
  two_way.Invalidate();  // Epoch invalidation must make every pointer unobservable.
  assert(!two_way.Find(key, 2) && !two_way.Find(third, 1));
  for (size_t epoch = 0; epoch < 10000; ++epoch) {
    two_way.Store(alternate, epoch + 3, &alternate_image);
    assert(two_way.Find(alternate, epoch + 3) == &alternate_image);
    two_way.Invalidate();
    assert(!two_way.Find(alternate, epoch + 3));
  }

  // Simulate recording over creation, release, descriptor/registry changes and
  // reused handles; compare hits against an independently maintained lifetime.
  std::mt19937 random(92528);
  std::array<Image, 20001> lifetimes{};
  Image* live = nullptr;
  uint64_t revision = 1;
  auto current_key = key;
  size_t allocations = 0, hits = 0;
  for (size_t step = 0; step < 20000; ++step) {
    switch (random() % 13) {
      case 0:  // Registry replaces logical/physical metadata.
        ++revision;
        live = nullptr;
        break;
      case 1:  // Release/destroy, or reflection re-registration.
        surfaces.Invalidate();
        live = nullptr;
        break;
      case 2:
        current_key.descriptor[3]++;
        live = nullptr;
        break;
      default: break;
    }
    if (auto* cached = surfaces.Find(current_key, revision)) {
      assert(live && cached == live);
      ++hits;
    } else {
      if (!live) {
        live = &lifetimes[allocations++];
        live->generation = allocations;
      }
      surfaces.Store(current_key, revision, live);
    }
    assert(surfaces.Find(current_key, revision) == live);
  }
  assert(hits > 10000);

  int shader_a, shader_b, declaration_a, declaration_b;
  NativeVertexStreamKey layout{&shader_a, &declaration_a, 11, 22};
  NativeVertexStreamMemo memo;
  assert(!memo.Find(layout));
  memo.Store(layout, {(1u << 0) | (1u << 16), true});
  assert(memo.Find(layout)->mask == 65537);
  auto copy = memo;  // Pipeline snapshots are copied before resource changes.
  auto other = layout;
  other.shader = &shader_b; assert(!copy.Find(other));
  other = layout; other.declaration = &declaration_b; assert(!copy.Find(other));
  other = layout; ++other.input_signature; assert(!copy.Find(other));
  other = layout; ++other.declaration_hash; assert(!copy.Find(other));
  memo.Store(other, {0, false});
  assert(memo.Find(other) && !memo.Find(other)->valid);
  memo.Store(other, {0, true});  // An empty valid input layout is not failure.
  assert(memo.Find(other)->valid && !memo.Find(other)->mask);
  // Two exact layouts should coexist; immutable pipeline snapshots can
  // alternate A/B declarations without recalculating their stream masks.
  for (unsigned i = 0; i < 10000; ++i) {
    assert(memo.Find(layout) && memo.Find(layout)->mask == 65537);
    assert(memo.Find(other) && memo.Find(other)->valid && memo.Find(other)->mask == 0);
  }
  memo.Store(layout, {37, true});  // Existing earlier entry is promoted.
  assert(memo.Find(layout) && memo.Find(layout)->mask == 37);
  assert(memo.Find(other) && memo.Find(other)->mask == 0);
  auto third_layout = other;
  ++third_layout.declaration_hash;
  memo.Store(third_layout, {99, true});
  assert(memo.Find(third_layout) && memo.Find(third_layout)->mask == 99);
  assert(memo.Find(layout) && memo.Find(layout)->mask == 37);
  assert(memo.Find(other) && memo.Find(other)->mask == 0);
  auto fourth_layout = other;
  fourth_layout.input_signature += 7;
  memo.Store(fourth_layout, {101, true});
  // The four alternating exact layouts must all be retained, with no
  // declaration/stream aliasing and without redoing discovery.
  for (unsigned i = 0; i < 20000; ++i) {
    assert(memo.Find(layout) && memo.Find(layout)->mask == 37);
    assert(memo.Find(other) && memo.Find(other)->mask == 0);
    assert(memo.Find(third_layout) && memo.Find(third_layout)->mask == 99);
    assert(memo.Find(fourth_layout) && memo.Find(fourth_layout)->mask == 101);
  }
  auto fifth_layout = other;
  fifth_layout.input_signature += 9;
  memo.Store(fifth_layout, {103, true});
  assert(!memo.Find(layout));  // Least recently used fourth-way entry.
  assert(memo.Find(other) && memo.Find(third_layout) && memo.Find(fourth_layout));
  assert(memo.Find(fifth_layout) && memo.Find(fifth_layout)->mask == 103);
  auto snapshot_copy = memo;
  assert(snapshot_copy.Find(other) && snapshot_copy.Find(third_layout) &&
         snapshot_copy.Find(fourth_layout) && snapshot_copy.Find(fifth_layout));
  // Sparse stream iteration must visit exactly the original stream set.
  for (uint32_t mask = 0; mask < (1u << 17); ++mask) {
    uint32_t visited = 0, previous = 0;
    for (uint32_t remaining = mask; remaining; remaining &= remaining - 1) {
      uint32_t stream = std::countr_zero(remaining);
      assert(stream < 17 && (!visited || stream > previous));
      previous = stream;
      visited |= 1u << stream;
    }
    assert(visited == mask);
  }
  std::cout << "PASS: every surface key field, collision/revision/release invalidation, "
               "20,000 lifetime transitions, copied vertex memos and all 131,072 stream masks\n";
}

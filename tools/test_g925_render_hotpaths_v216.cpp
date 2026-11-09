// Native CPU recording fast paths. Executed on the Windows host by APK CI.
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "graphics/gta4_native/native_draw_lookup_cache.h"
#include "graphics/gta4_native/native_pipeline_lookup_memo.h"
#include "graphics/gta4_native/native_resolve_descriptor_cache.h"

using namespace rex::graphics::gta4_native;

struct FixedState {
  uint32_t id = 0;
  static inline size_t comparisons = 0;
  bool operator==(const FixedState& other) const {
    ++comparisons;
    return id == other.id;
  }
};
using PipelineMemo = NativePipelineLookupMemo<FixedState, 4, uint64_t>;

static void TestPipelineFastPath() {
  PipelineMemo memo;
  PipelineMemo::Context ctx{};
  ctx.lifetime = 13;
  ctx.pipeline_layout = 21;
  const int owner = 1, alternate_owner = 2;
  for (uint32_t i = 1; i <= 4; ++i) {
    memo.Store(&owner, {i}, ctx, 1000 + i, 9000 + i);
  }
  FixedState::comparisons = 0;
  for (size_t n = 0; n < 20000; ++n) {
    assert(memo.Find(&owner, {4}, ctx) == 1004);
  }
  // Last stored/matched exact identity is checked before any other entry.
  assert(FixedState::comparisons == 20000);
  assert(memo.Find(&owner, {1}, ctx) == 1001);
  FixedState::comparisons = 0;
  for (size_t n = 0; n < 10000; ++n) {
    assert(memo.Find(&owner, {1}, ctx) == 1001);
  }
  assert(FixedState::comparisons == 10000);
  for (size_t n = 0; n < 10000; ++n) {
    assert(memo.FindSigned(&owner, 9002, ctx) == 1002);
  }
  auto snapshot = memo;
  assert(snapshot.Find(&alternate_owner, {2}, ctx) == 0);
  assert(memo.Find(&owner, {99}, ctx) == 0);
  assert(memo.FindSigned(&owner, 0, ctx) == 0);
  auto different = ctx;
  ++different.pipeline_layout;
  assert(memo.Find(&owner, {2}, different) == 0);
  ++different.lifetime;
  assert(memo.FindSigned(&owner, 9002, different) == 0);
  memo.Store(&owner, {5}, ctx, 1005, 9005);
  assert(memo.Find(&owner, {5}, ctx) == 1005);
  assert(memo.Find(&owner, {1}, ctx) == 0); // four-entry ring eviction
  assert(memo.Find(&owner, {4}, ctx) == 1004);
  memo.Store(&owner, {5}, ctx, 1015, 9015); // duplicate key replaces result
  assert(memo.Find(&owner, {5}, ctx) == 1015);
  assert(memo.FindSigned(&owner, 9015, ctx) == 1015);
  assert(snapshot.Find(&owner, {1}, ctx) == 1001); // independent copy
  NativePipelineLookupLifetime lifetime(5);
  lifetime.Invalidate();
  assert(lifetime.epoch() == 6);
}

struct SurfaceImage { uint64_t serial = 0; };
static void TestSurfaceDistribution() {
  NativeSurfaceLookupCache<SurfaceImage> surfaces;
  NativeSurfaceLookupKey base{};
  base.descriptor = {0x1234, 1, 2, 3, 4, 5, 1280, 720, 2};
  base.samples = 4;
  constexpr size_t count = 24;
  std::array<SurfaceImage, count> images{};
  std::array<NativeSurfaceLookupKey, count> keys{};
  // 24 different formats on the same handle share ONE set in the old
  // handle-only hash. Variant-aware set indexing should retain them all.
  for (size_t i = 0; i < count; ++i) {
    keys[i] = base;
    keys[i].descriptor[5] = uint32_t(100 + i);
    images[i].serial = i + 1;
    surfaces.Store(keys[i], 7, &images[i]);
  }
  for (size_t repeat = 0; repeat < 1000; ++repeat)
    for (size_t i = 0; i < count; ++i)
      assert(surfaces.Find(keys[i], 7) == &images[i]);
  assert(surfaces.Find(keys[0], 8) == nullptr);
  auto changed = keys[0];
  changed.descriptor[3]++;
  assert(!surfaces.Find(changed, 7));
  surfaces.Invalidate();
  for (auto& key : keys) assert(!surfaces.Find(key, 7));
  surfaces.Store(keys[3], 8, &images[3]);
  assert(surfaces.Find(keys[3], 8) == &images[3]);
}

using ResolveCache = NativeResolveDescriptorCache<uint64_t>;
static ResolveCache::Key ResolveKey(uint64_t id) {
  return {id + 1, 8, 9, uint32_t(id % 4)};
}
static void TestResolveHotDescriptorLifetime() {
  ResolveCache cache;
  const auto key = ResolveKey(1);
  assert(!cache.Find(key));
  cache.BeginScope(81, 91);
  cache.Insert(key, 42);
  const auto initial = cache.statistics().hits;
  for (size_t i = 0; i < 20000; ++i) {
    assert(cache.Find(key) == 42);
  }
  assert(cache.statistics().hits - initial == 20000);
  cache.Insert(key, 99);  // the exact existing pool-owned descriptor wins
  assert(cache.Find(key) == 42);
  // Fill and roll the bounded map while the hot memo follows exact keys.
  for (uint64_t i = 2; i < 2000; ++i) {
    cache.Insert(ResolveKey(i), i + 50);
    assert(cache.Find(ResolveKey(i)) == i + 50);
    assert(cache.size() <= ResolveCache::kCapacity);
  }
  cache.BeginScope(81, 92);
  assert(!cache.Find(ResolveKey(1999)));  // old pool handles are invalid
  cache.Insert(ResolveKey(1999), 2025);
  assert(cache.Find(ResolveKey(1999)) == 2025);
  cache.BeginScope(82, 92);
  assert(!cache.Find(ResolveKey(1999)));  // command-buffer change
  cache.Reset();
  assert(!cache.Find(ResolveKey(1999)));
}

int main() {
  TestPipelineFastPath();
  TestSurfaceDistribution();
  TestResolveHotDescriptorLifetime();
  std::cout << "PASS: 30,000 exact pipeline fast hits, 24 surface variants x 1000, "
               "bounded resolve descriptor MRU and pool/epoch resets\n";
}

// G925 Native 2.38: executable exact GPU-state lookup and geometry replay.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#include "graphics/gta4_native/native_resolve_descriptor_cache.h"
#include "graphics/gta4_native/native_vertex_cache.h"

using rex::graphics::gta4_native::NativeResolveDescriptorCache;
using rex::graphics::gta4_native::OptimizeTriangleListForVertexCache;

using Cache = NativeResolveDescriptorCache<uint64_t>;
static Cache::Key Key(uint64_t id) { return {id + 1, id + 33, id + 99, uint32_t(id % 4)}; }

static void CheckFourWayExactHit() {
  Cache cache;
  cache.BeginScope(110, 210);
  for (uint64_t id = 1; id <= 4; ++id) cache.Insert(Key(id), 1000 + id);
  const auto before = cache.statistics();
  for (size_t cycle = 0; cycle < 10000; ++cycle) {
    for (uint64_t id = 1; id <= 4; ++id) assert(cache.Find(Key(id)) == 1000 + id);
  }
  const auto& after = cache.statistics();
  assert(after.hits - before.hits == 40000);
  assert(after.hot_hits - before.hot_hits == 40000); // no map traversal
  // Inserting an exact duplicate does not replace a published descriptor.
  cache.Insert(Key(2), 99999);
  assert(cache.Find(Key(2)) == 1002);
  // Different exact lifetimes, layouts and image layouts may never alias.
  auto mismatch = Key(2);
  mismatch.surface_lifetime++;
  assert(!cache.Find(mismatch));
  mismatch = Key(2);
  mismatch.layout++;
  assert(!cache.Find(mismatch));
  mismatch = Key(2);
  mismatch.image_layout++;
  assert(!cache.Find(mismatch));
  // A command-buffer/pool transition invalidates every borrowed shortcut.
  cache.BeginScope(110, 211);
  for (uint64_t id = 1; id <= 4; ++id) assert(!cache.Find(Key(id)));
  cache.Insert(Key(2), 5002);
  assert(cache.Find(Key(2)) == 5002);
  cache.BeginScope(111, 211);
  assert(!cache.Find(Key(2)));
}

static void CheckNoHotStaleEntriesOnMapEviction() {
  Cache cache;
  cache.BeginScope(600, 601);
  for (uint64_t id = 1; id <= 256; ++id) cache.Insert(Key(id), 9000 + id);
  assert(cache.size() == 256);
  // Force 256 -> 257; the arbitrary map victim MUST also be evicted from MRU.
  cache.Insert(Key(257), 9257);
  assert(cache.size() == 256);
  size_t misses = 0;
  for (uint64_t id = 1; id <= 257; ++id) {
    const uint64_t found = cache.Find(Key(id));
    if (!found) ++misses;
    else assert(found == (id == 257 ? 9257 : 9000 + id));
  }
  assert(misses == 1);
}

static void CheckVertexTrianglePreservation() {
  // Background vertex-cache reordering must preserve winding and all
  // triangles; the overflow guard only affects work that would be skipped.
  const std::array<uint16_t, 24> indices{
       0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7,
       0, 3, 4, 2, 6, 5, 1, 2, 7, 4, 7, 3
  };
  std::array<uint16_t, indices.size()> optimized{};
  assert(OptimizeTriangleListForVertexCache(indices.data(), 8, optimized.data()));
  auto triangles = [](const auto& values) {
    std::vector<std::array<uint16_t, 3>> result;
    for (size_t i = 0; i < values.size(); i += 3)
      result.push_back({values[i], values[i + 1], values[i + 2]});
    std::sort(result.begin(), result.end());
    return result;
  };
  assert(triangles(indices) == triangles(optimized));
}

int main() {
  CheckFourWayExactHit();
  CheckNoHotStaleEntriesOnMapEviction();
  CheckVertexTrianglePreservation();
}

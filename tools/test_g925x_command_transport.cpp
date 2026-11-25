#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_texture_protection.h"
using rex::graphics::gta4_native::NativeTextureProtectionIndex;
int main() {
  NativeTextureProtectionIndex index;
  std::unordered_map<uint64_t, uint64_t> expected;
  std::mt19937_64 rng(92507);
  for (unsigned frame = 0; frame < 240; ++frame) {
    for (unsigned i = 0; i < 14771; ++i) {
      uint64_t key = i % 11 ? 1 + rng() % 64 : 1 + rng() % 16384;
      if (expected.contains(key) && (rng() & 1)) {
        assert(index.Release(key));
        if (--expected[key] == 0) expected.erase(key);
      } else {
        assert(index.Retain(key));
        ++expected[key];
      }
      assert(index.Contains(key) == expected.contains(key));
      assert(index.size() == expected.size() && index.valid());
    }
    std::unordered_set<uint64_t> live;
    index.AppendTo(live);
    assert(live.size() == expected.size());
    for (const auto& item : expected) assert(live.contains(item.first));
    if (frame % 13 == 0) { index.Reset(); expected.clear(); assert(index.size() == 0); }
  }
  index.Reset();
  assert(index.Retain(0) && index.Release(0));
  assert(!index.Release(9) && !index.valid());
  index.Reset();
  assert(index.valid() && index.Retain(9) && index.Release(9) && !index.Contains(9));
  std::cout << "PASS: 3545040 exact queue-reference operations, collisions, rehash, reset and fallback\n";
}

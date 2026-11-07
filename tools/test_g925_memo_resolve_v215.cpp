// Host-only exact-memo and descriptor-pool cache regressions (no Vulkan GPU).
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "graphics/gta4_native/native_exact_memo.h"
#include "graphics/gta4_native/native_draw_lookup_cache.h"
#include "graphics/gta4_native/native_resolve_descriptor_cache.h"

using namespace rex::graphics::gta4_native;

static void TestExactMemo() {
  NativeExactMemo<uint64_t, uint64_t> memo;
  unsigned computed = 0;
  auto calculate = [&](uint64_t key) { ++computed; return key * 19; };
  for (unsigned n = 0; n < 10000; ++n) {
    for (uint64_t key : {uint64_t{1}, uint64_t{2}}) {
      assert(memo.Get(key, [&] { return calculate(key); }) == key * 19);
    }
  }
  assert(computed == 2); // old one-entry memo performed 20,000 calculations
  memo.Store(1, 21);
  assert(memo.Find(1) && *memo.Find(1) == 21);
  assert(memo.Find(2) && *memo.Find(2) == 38);
  memo.Store(3, 57);
  assert(memo.Find(3) && memo.Find(1));
  assert(!memo.Find(2)); // latest store replaces only the older entry
  auto copy = memo;
  assert(copy.Find(1) && copy.Find(3));
  memo.Reset();
  assert(!memo.Find(1) && !memo.Find(2) && !memo.Find(3));
  assert(copy.Find(1) && copy.Find(3)); // copies remain independent
}

static void TestVertexMemo() {
  int shader1, shader2, decl1, decl2;
  NativeVertexStreamMemo memo;
  NativeVertexStreamKey a{&shader1, &decl1, 11, 23};
  NativeVertexStreamKey b{&shader2, &decl2, 13, 29};
  assert(!memo.Find(a));
  memo.Store(a, {7, true});
  memo.Store(b, {15, true});
  for (unsigned i = 0; i < 10000; ++i) {
    assert(memo.Find(a) && memo.Find(a)->mask == 7);
    assert(memo.Find(b) && memo.Find(b)->mask == 15);
  }
  memo.Store(a, {21, true}); // promote older exact selection
  assert(memo.Find(a) && memo.Find(a)->mask == 21);
  assert(memo.Find(b) && memo.Find(b)->mask == 15);
  auto c = b; ++c.input_signature;
  assert(!memo.Find(c));
  memo.Store(c, {31, true});
  assert(memo.Find(c) && memo.Find(a));
  assert(!memo.Find(b)); // signature prevents wrong-layout reuse
  auto snapshot = memo;
  assert(snapshot.Find(a) && snapshot.Find(c));
  c.declaration_hash ^= 17;
  assert(!snapshot.Find(c));
}

using Resolve = NativeResolveDescriptorCache<uint64_t>;
static Resolve::Key Key(uint64_t id) {
  return Resolve::Key{id + 1, 555, 777, uint32_t(id % 4)};
}

static void TestDescriptorLifetimeAndCapacity() {
  Resolve cache;
  const auto first = Key(0);
  cache.Insert(first, 11); // invalid before BeginScope
  assert(cache.size() == 0);
  cache.BeginScope(100, 200);
  cache.Insert(first, 11);
  assert(cache.Find(first) == 11);
  cache.Insert(first, 99);
  assert(cache.size() == 1 && cache.Find(first) == 11);
  cache.Insert(Key(10), 0); // reject invalid handles
  assert(!cache.Find(Key(10)));
  for (uint64_t i = 1; i < Resolve::kCapacity; ++i) {
    cache.Insert(Key(i), i + 1000);
  }
  assert(cache.size() == Resolve::kCapacity);
  cache.Insert(first, 99); // duplicate at capacity cannot evict any entry
  assert(cache.size() == Resolve::kCapacity);
  for (uint64_t i = 0; i < Resolve::kCapacity; ++i) {
    assert(cache.Find(Key(i)) == (i ? i + 1000 : 11));
  }

  cache.Insert(Key(Resolve::kCapacity), 987654);
  assert(cache.size() == Resolve::kCapacity);
  assert(cache.Find(Key(Resolve::kCapacity)) == 987654);
  unsigned survivors = 0;
  for (uint64_t i = 0; i < Resolve::kCapacity; ++i) {
    if (cache.Find(Key(i))) ++survivors;
  }
  assert(survivors == Resolve::kCapacity - 1); // only one victim, not 256

  for (uint64_t i = Resolve::kCapacity + 1; i < 10000; ++i) {
    cache.Insert(Key(i), i + 1000);
    assert(cache.size() <= Resolve::kCapacity);
  }
  cache.BeginScope(100, 201); // new descriptor pool invalidates all handles
  assert(cache.size() == 0 && cache.Find(Key(9999)) == 0);
  cache.Insert(first, 77);
  assert(cache.Find(first) == 77);
  cache.BeginScope(101, 201); // same pool, different command buffer
  assert(cache.size() == 0 && cache.Find(first) == 0);
  cache.Insert(first, 88);
  cache.Reset();
  assert(cache.size() == 0 && cache.Find(first) == 0);
}

int main() {
  TestExactMemo();
  TestVertexMemo();
  TestDescriptorLifetimeAndCapacity();
  std::cout << "PASS: A/B exact memos, snapshot copies, bounded descriptor retention, "
               "pool/command-buffer invalidation and 10,000 insertions\n";
}

// Self-contained CPU cache regressions; does not need an Android device.
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include "graphics/gta4_native/frame_constant_arena.h"
#include "graphics/gta4_native/native_draw_lookup_cache.h"

using namespace rex::graphics::gta4_native;

static void Check(bool value) {
  if (!value) throw std::runtime_error("G925 native cache correctness regression");
}

struct CountingHash {
  static inline uint64_t calls = 0;
  size_t operator()(uint32_t key) const noexcept {
    ++calls;
    return key;
  }
};

struct CollidingHash {
  size_t operator()(uint64_t) const noexcept { return 1; }
};

static void TestSingleProbe() {
  FrameGenerationMap<uint32_t, uint32_t, CountingHash> map;
  CountingHash::calls = 0;
  for (uint32_t i = 1; i <= 12; ++i) {
    auto result = map.Insert(i, 100 + i);
    Check(result && result.inserted && *result.value == 100 + i);
    // No resizing below 50% occupancy: one hash, one insertion walk.
    Check(CountingHash::calls == i);
  }
  auto duplicate = map.Insert(3, 999);
  Check(duplicate && !duplicate.inserted && *duplicate.value == 103);
  Check(CountingHash::calls == 13);
  Check(map.size() == 12 && map.bucket_count() == 32);
}

static void TestCollisionAndGeneration() {
  FrameGenerationMap<uint64_t, uint64_t, CollidingHash> map;
  for (uint64_t epoch = 1; epoch <= 120; ++epoch) {
    std::unordered_map<uint64_t, uint64_t> reference;
    for (uint64_t step = 0; step < 960; ++step) {
      const uint64_t key = (step * 31 + epoch * 3) % 96;
      const uint64_t value = (epoch << 32) | step;
      const auto existing = reference.find(key);
      const bool should_insert = existing == reference.end();
      const auto result = map.Insert(key, value);
      Check(result && result.inserted == should_insert);
      if (should_insert) {
        reference.emplace(key, value);
        Check(*result.value == value);
      } else {
        Check(*result.value == existing->second);
      }
      if ((step % 79) == 0) {
        for (const auto& [check_key, check_value] : reference) {
          const uint64_t* found = map.Find(check_key);
          Check(found && *found == check_value);
        }
      }
    }
    Check(map.size() == reference.size());
    for (const auto& [key, value] : reference) {
      const auto* found = map.Find(key);
      Check(found && *found == value);
    }
    Check(map.CanResetGeneration() && map.ResetGeneration());
    Check(map.size() == 0);
    for (const auto& [key, value] : reference) {
      (void)value;
      Check(map.Find(key) == nullptr);
    }
  }
}

static void TestSubmissionFenceLifetime() {
  FrameConstantArenaIndex arena;
  Check(arena.SetByteCapacity(4096));
  FrameConstantIdentity vertex{FrameConstantKind::kVertex, 17};
  FrameConstantIdentity pixel{FrameConstantKind::kPixel, 17};
  auto a = arena.FindOrReserve(vertex, 64, 32);
  Check(a && !a->reused && a->offset == 0);
  auto b = arena.FindOrReserve(pixel, 40, 64);
  Check(b && !b->reused && b->offset == 64);
  auto same = arena.FindOrReserve(vertex, 64, 32);
  Check(same && same->reused && same->offset == a->offset);
  Check(!arena.FindOrReserve(vertex, 80, 32));
  Check(arena.MarkSubmitted(77));
  Check(!arena.ResetUnsubmitted());
  Check(!arena.ResetAfterCompletion(76));
  Check(arena.ResetAfterCompletion(77));
  Check(arena.bytes_used() == 0 && arena.reservation_count() == 0);
  Check(!arena.Find(vertex));
  auto next = arena.FindOrReserve(pixel, 80, 16);
  Check(next && !next->reused && next->offset == 0);
  Check(arena.ResetUnsubmitted());
}

int main() {
  TestSingleProbe();
  TestCollisionAndGeneration();
  TestSubmissionFenceLifetime();
  std::cout << "PASS: one-hash normal inserts, duplicate identity, "
               "120 collision-heavy generations, GPU fence guarded slots\n";
}

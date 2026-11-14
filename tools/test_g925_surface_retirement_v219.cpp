// Vulkan-surface retirement scheduling replay: no Android driver required.
// Use the exact production scan cursor and completed-submission policy.
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <unordered_set>
#include <vector>
#include "graphics/gta4_native/native_surface_retirement_scan.h"
#include "graphics/gta4_native/native_frame_scheduling.h"

using namespace rex::graphics::gta4_native;
struct Surface { uint64_t identity = 0, last_gpu_submission = 0; };
struct Model {
  std::vector<Surface> live;
  std::unordered_set<uint64_t> pending;
  size_t cursor = 0;
  uint64_t completed = 1;
  struct Result { size_t visited, retired; };

  Result Tick(bool force = false) {
    if (pending.empty()) return {};
    NativeSurfaceRetirementScan scan(live.size(), cursor, force);
    size_t retired = 0;
    while (scan.HasNext(live.size()) &&
           retired < NativeSurfaceRetirementLimit(force)) {
      const Surface& image = live[scan.index()];
      if (!pending.contains(image.identity) ||
          ShouldKeepNativeSurfacePending(image.last_gpu_submission, completed,
                                         retired, force)) {
        scan.Next(live.size(), false);
        continue;
      }
      const uint64_t id = image.identity;
      live.erase(live.begin() + scan.index());
      pending.erase(id);
      scan.Next(live.size(), true);
      ++retired;
    }
    cursor = scan.index();
    return {scan.visited(), retired};
  }
};

static void TestBoundedSearchAndFairness() {
  static_assert(kNativeSurfaceRetirementScanItemsPerFrame == 512);
  static_assert(NativeSurfaceRetirementScan(30000, 0, false).limit() == 512);
  static_assert(NativeSurfaceRetirementScan(30000, 0, true).limit() == 30000);
  static_assert(NativeSurfaceRetirementScan(0, 19, false).index() == 0);
  static_assert(NativeSurfaceRetirementScan(11, 100, false).index() == 1);
  Model model;
  model.live.reserve(30000);
  for (uint64_t i = 1; i <= 30000; ++i) model.live.push_back({i, 1});
  model.pending.insert(29999);
  model.pending.insert(30000);
  bool found = false;
  for (size_t frame = 0; frame < 65; ++frame) {
    const auto result = model.Tick();
    assert(result.visited <= kNativeSurfaceRetirementScanItemsPerFrame);
    assert(result.retired <= kNativeSurfaceRetirementsPerFrame);
    if (model.pending.empty()) { found = true; break; }
  }
  assert(found);
  assert(model.live.size() == 29998);
  assert(model.cursor < model.live.size());
}

static void TestFencesBurstAndVectorErase() {
  Model model;
  for (uint64_t i = 0; i < 300; ++i) {
    model.live.push_back({i+1, i % 10 == 0 ? 7u : 5u});
    model.pending.insert(i+1);
  }
  model.completed = 5;
  size_t retired = 0;
  for (size_t frame = 0; frame < 120; ++frame) {
    auto tick = model.Tick();
    assert(tick.visited <= kNativeSurfaceRetirementScanItemsPerFrame);
    assert(tick.retired <= kNativeSurfaceRetirementsPerFrame);
    retired += tick.retired;
    if (model.pending.size() == 30) break;
  }
  assert(retired == 270 && model.pending.size() == 30);
  for (const auto& s : model.live) assert(s.last_gpu_submission == 7);
  // The still-in-flight thirty cannot be destroyed, even during a forced
  // drain with enough scan allowance to examine the whole live vector.
  assert(model.Tick(true).retired == 0);
  model.completed = 7;
  for (size_t frame = 0; frame < 10; ++frame) retired += model.Tick().retired;
  assert(retired == 300 && model.pending.empty() && model.live.empty());
  assert(model.cursor == 0);
}

static void TestForcedIdleDrainAndAppend() {
  Model model;
  for (uint64_t i = 1; i <= 1000; ++i) {
    model.live.push_back({i, 2});
    model.pending.insert(i);
  }
  model.completed = 2;
  auto done = model.Tick(true);
  assert(done.retired == 1000 && model.live.empty());
  assert(model.cursor == 0 && model.pending.empty());
  // A fresh lifecycle appending into an empty vector must not retain an old
  // invalid index; both ordinary and force drains remain bounded/safe.
  model.live.push_back({1001, 2});
  model.pending.insert(1001);
  done = model.Tick();
  assert(done.retired == 1 && model.live.empty());
}

int main() {
  TestBoundedSearchAndFairness();
  TestFencesBurstAndVectorErase();
  TestForcedIdleDrainAndAppend();
  std::cout << "PASS: bounded 512-item surface search; 30k-scene fairness, "
               "GPU fence protection, 300-release burst and full idle drain\n";
}

// Frame-spike prevention policies: host-only deterministic C++20 replay.
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include "graphics/gta4_native/native_frame_scheduling.h"

using namespace rex::graphics::gta4_native;

struct Surface {
  uint64_t last_gpu_submission = 0;
  bool pending = true;
  bool destroyed = false;
};

static size_t Retire(std::vector<Surface>& surfaces, uint64_t completed, bool force_drain) {
  size_t retired = 0;
  for (auto& surface : surfaces) {
    if (!surface.pending || surface.destroyed) continue;
    if (ShouldKeepNativeSurfacePending(surface.last_gpu_submission, completed,
                                       retired, force_drain)) continue;
    surface.destroyed = true;
    surface.pending = false;
    ++retired;
  }
  return retired;
}

static void TestBoundedSurfaceRetirement() {
  static_assert(kNativeSurfaceRetirementsPerFrame == 6);
  assert(NativeSurfaceRetirementLimit(false) == 6);
  assert(NativeSurfaceRetirementLimit(true) > 100000);
  assert(ShouldKeepNativeSurfacePending(33, 32, 0, true));
  assert(!ShouldKeepNativeSurfacePending(32, 32, 0, true));
  assert(ShouldKeepNativeSurfacePending(32, 32, 6, false));
  assert(!ShouldKeepNativeSurfacePending(32, 32, 5, false));

  // 300 completed releases would stall one frame if destroyed together.
  // The renderer must be able to drain them across bounded present frames.
  std::vector<Surface> surfaces(300);
  for (size_t i = 0; i < surfaces.size(); ++i)
    surfaces[i].last_gpu_submission = i % 10 == 0 ? 120 : 100;
  size_t total = 0;
  for (uint32_t frame = 0; frame < 45; ++frame) {
    const size_t n = Retire(surfaces, 100, false);
    assert(n <= kNativeSurfaceRetirementsPerFrame);
    total += n;
    if (frame == 0) assert(n == 6);
  }
  assert(total == 270);
  for (size_t i = 0; i < surfaces.size(); ++i) {
    if (i % 10 == 0) assert(!surfaces[i].destroyed);
    else assert(surfaces[i].destroyed);
  }
  // GPU completes the remaining submission. The in-flight fence is still
  // checked, and completion is spread across frames.
  for (uint32_t frame = 0; frame < 10; ++frame)
    total += Retire(surfaces, 120, false);
  assert(total == 300);
  for (const auto& surface : surfaces) assert(surface.destroyed);

  // If the title is paused with no more frames, explicit no-frame release
  // drains every completed resource; never leaks pending objects forever.
  std::vector<Surface> paused(1000);
  for (auto& surface : paused) surface.last_gpu_submission = 4;
  assert(Retire(paused, 4, true) == 1000);
  std::vector<Surface> mixed(1000);
  for (size_t i = 0; i < 1000; ++i) mixed[i].last_gpu_submission = i < 500 ? 3 : 5;
  assert(Retire(mixed, 4, true) == 500);
  for (size_t i = 500; i < 1000; ++i) assert(!mixed[i].destroyed);
  assert(Retire(mixed, 5, true) == 500);
}

static void TestAgedBufferCleanupBudget() {
  static_assert(GetNativeBufferReclamationWorkLimit(100, 100).max_resources == 8);
  static_assert(GetNativeBufferReclamationWorkLimit(101, 100).max_resources == 64);
  const auto idle = GetNativeBufferReclamationWorkLimit(128ull << 20, 256ull << 20);
  const auto pressured = GetNativeBufferReclamationWorkLimit(257ull << 20, 256ull << 20);
  assert(idle.max_resources == 8 && idle.max_bytes == 2ull * 1024 * 1024);
  assert(pressured.max_resources == 64 && pressured.max_bytes == 16ull * 1024 * 1024);
  // Reclamation must stay bounded in both modes; under pressure the old
  // aggressive work limits remain available.
  assert(idle.max_resources < pressured.max_resources);
  assert(idle.max_bytes < pressured.max_bytes);
}

int main() {
  TestBoundedSurfaceRetirement();
  TestAgedBufferCleanupBudget();
  std::cout << "PASS: 300 completed surfaces retired in <=6/frame batches, "
               "in-flight fences, full idle drain, pressure-aware aged-buffer budgets\n";
}

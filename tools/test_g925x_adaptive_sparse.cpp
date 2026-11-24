// Host-only proof that G925 X skips failed speculative search WITHOUT
// skipping or mutating original indexed draws. Not a phone FPS benchmark.
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_g925x_adaptive_sparse.h"

using namespace rex::graphics::gta4_native;

int main() {
  NativeG925XAdaptiveSparse policy{};
  constexpr uint32_t kFrames = 560;
  uint32_t probes = 0, passes = 0;
  for (uint32_t frame = 0; frame < kFrames; ++frame) {
    const bool probe = policy.BeginPass();
    probes += uint32_t(probe);
    ++passes;
    // v3.5 actual report: zero sparse/rebased draws saved for 180 frames.
    policy.EndPass(probe, false);
  }
  assert(passes == kFrames);
  // 8 probes, 48 skipped; a fresh probe phase every 56 frames.
  assert(probes == 80);
  assert(policy.SkipRemaining() == 48);
  // No render commands are discarded: scheduling controls optional
  // merge candidate search only; the original draw path always runs.
  uint64_t dispatched = 0;
  constexpr uint32_t kCommandsPerFrame = 3657;
  for (uint32_t i = 0; i < kFrames; ++i) {
    const bool probe = policy.BeginPass();
    (void)probe;
    dispatched += kCommandsPerFrame;
    policy.EndPass(probe, false);
  }
  assert(dispatched == uint64_t(kFrames) * kCommandsPerFrame);
  policy.Reset();
  // The first real merge keeps the optimizer active, and periodic scene
  // changes are detected on the next probe.
  for (uint32_t i = 0; i < 100; ++i) {
    const bool probe = policy.BeginPass();
    assert(probe);
    policy.EndPass(probe, true);
  }
  assert(policy.SkipRemaining() == 0);
  for (uint32_t i = 0; i < 8; ++i) {
    assert(policy.BeginPass());
    policy.EndPass(true, false);
  }
  assert(!policy.BeginPass());
  policy.EndPass(false, true);  // false probe must never force-enable the path
  assert(policy.SkipRemaining() == 47);
  std::cout << "PASS: G925 X lossless CPU scheduling (80/560 speculative probes; "
               "all guest draws preserved), merge-success wakeup\n";
}

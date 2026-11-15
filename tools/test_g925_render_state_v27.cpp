// Standalone host tests: no phone, Vulkan driver, Catch2 or Android SDK required.
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "graphics/gta4_native/native_draw_state_cache.h"
#include "android/g925_hint_thread_policy.h"

using Cache = rex::graphics::gta4_native::NativeDrawStateCache<2>;
static void check(bool value) {
  if (!value) throw std::runtime_error("render-state/ADPF regression");
}
struct State {
  std::array<uint32_t, 6> viewport{1, 2, 1280, 720, 0, 0x3f800000};
  std::array<int64_t, 4> scissor{0, 0, 1280, 720};
  std::array<uint32_t, 3> bias{};
  std::array<uint32_t, 6> stencil{1, 255, 255, 1, 255, 255};
  std::array<uint32_t, 4> blend{};
  bool operator==(const State&) const = default;
};
static unsigned emit(Cache& cache, const State& requested, State& driver) {
  unsigned commands = 0;
  if (cache.UpdateViewport(requested.viewport)) { driver.viewport = requested.viewport; ++commands; }
  if (cache.UpdateScissor(requested.scissor)) { driver.scissor = requested.scissor; ++commands; }
  if (cache.UpdateDepthBias(requested.bias)) { driver.bias = requested.bias; ++commands; }
  if (cache.UpdateStencil(requested.stencil)) { driver.stencil = requested.stencil; commands += 6; }
  if (cache.UpdateBlendConstants(requested.blend)) { driver.blend = requested.blend; ++commands; }
  return commands;
}
static void state_replay() {
  Cache cache;
  State desired, driver;
  check(cache.UpdatePipeline(1, true));
  check(emit(cache, desired, driver) == 10);
  check(cache.UpdatePipeline(2, true));
  check(emit(cache, desired, driver) == 0);
  desired.viewport[0] = 0x80000000u; // signed zero is NOT positive zero
  desired.blend[1] = 0x7fc00001u; // preserve exact NaN payload
  check(emit(cache, desired, driver) == 2);
  check(driver == desired);
  // A static/external pipeline invalidates dynamic values, even if equal.
  check(cache.UpdatePipeline(99));
  driver = {};
  check(cache.UpdatePipeline(3, true));
  check(emit(cache, desired, driver) == 10);
  check(driver == desired);
  cache.Reset(); // same VkCommandBuffer handle can be re-recorded
  check(emit(cache, desired, driver) == 10);

  uint32_t seed = 0x92527;
  for (unsigned i = 0; i < 20000; ++i) {
    seed = seed * 1664525u + 1013904223u;
    if (i % 31 == 0) { cache.Reset(); driver = {}; }
    if (i % 47 == 0) { cache.UpdatePipeline(0x10000 + i); driver = {}; }
    cache.UpdatePipeline(1 + i % 17, true);
    if (i % 3 == 0) desired.viewport[i % 6] = seed;
    if (i % 5 == 0) desired.scissor[i % 4] = seed;
    if (i % 7 == 0) desired.bias[i % 3] = seed;
    if (i % 11 == 0) desired.stencil[i % 6] = seed;
    if (i % 13 == 0) desired.blend[i % 4] = seed;
    emit(cache, desired, driver);
    check(driver == desired); // independently model the state used by the draw
  }
}
static void index_bindings() {
  Cache cache;
  check(cache.UpdateIndexBuffer(1, 2, 64, 0));
  check(!cache.UpdateIndexBuffer(1, 2, 64, 0));
  cache.UpdatePipeline(7, true);
  check(!cache.UpdateIndexBuffer(1, 2, 64, 0));
  check(cache.UpdateIndexBuffer(1, 2, 64, 1)); // uint16 -> uint32
  check(cache.UpdateIndexBuffer(1, 2, 128, 1));
  check(cache.UpdateIndexBuffer(1, 3, 128, 1));
  check(cache.UpdateIndexBuffer(2, 3, 128, 1)); // another command buffer
  check(cache.UpdateIndexBuffer(2, 3, (uint64_t{1} << 33) + 128, 1));
  cache.Reset();
  check(cache.UpdateIndexBuffer(2, 3, (uint64_t{1} << 33) + 128, 1));
  check(cache.UpdateVertexBuffer(2, 1, 3, 64));
  check(!cache.UpdateVertexBuffer(2, 1, 3, 64));
}
static void stencil_fields() {
  Cache cache;
  std::array<uint32_t, 6> driver{}, desired{1, 255, 255, 1, 255, 255};
  unsigned calls = 0;
  auto emit = [&](size_t field, bool front, bool back, uint32_t value) {
    check(front || back);
    if (front) driver[field] = value;
    if (back) driver[field + 3] = value;
    ++calls;
  };
  cache.EmitStencilChanges(desired, emit); check(calls == 3 && driver == desired);
  cache.EmitStencilChanges(desired, emit); check(calls == 3);
  desired[0] = 9;
  cache.EmitStencilChanges(desired, emit); check(calls == 4 && driver == desired);
  desired[3] = 9;
  cache.EmitStencilChanges(desired, emit); check(calls == 5 && driver == desired);
  uint32_t seed = 27;
  for (unsigned i = 0; i < 20000; ++i) {
    seed = seed * 1664525u + 1013904223u;
    if (i % 31 == 0) { cache.Reset(); driver = {}; }
    cache.UpdatePipeline(i + 1, true);
    desired[i % 6] = seed & 255;
    cache.EmitStencilChanges(desired, emit);
    check(driver == desired);
  }
}
static void hint_threads() {
  using gta4::android::G925StableHintThreads;
  auto alive = [](int id) { return id > 0; };
  G925StableHintThreads<int> policy(3);
  check(policy.Select({1,2,3}, alive) == std::vector<int>({1,2,3}));
  check(policy.Select({1,2,4}, alive) == std::vector<int>({1,2,3}));
  check(policy.Select({1,2,4}, alive, {4}) == std::vector<int>({1,2,4}));
  check(policy.Select({1,2,5}, alive, {4}) == std::vector<int>({1,2,4}));
  auto dead_two = [](int id) { return id > 0 && id != 2; };
  check(policy.Select({1,4,5}, dead_two, {4}) == std::vector<int>({1,4,5}));
  check(policy.Select({1,4,6}, alive, {4}) == std::vector<int>({1,4,5}));
  check(policy.Select({1,4,6}, alive, {4}) == std::vector<int>({1,4,5}));
  check(policy.Select({1,4,6}, alive, {4}) == std::vector<int>({1,4,6}));
  check(policy.Select({}, [](int){return false;}, {4}).empty());
  check(policy.Select({-1,9,9,8}, alive, {-1,7,7,8,9,10}) == std::vector<int>({7,8,9}));
  policy.Reset(); check(policy.chosen().empty());
}
static void equivalent_workload() {
  Cache before, after;
  State desired, old_driver, new_driver;
  unsigned old_commands = 0, new_commands = 0;
  for (unsigned draw = 0; draw < 1000; ++draw) {
    before.UpdatePipeline(draw + 1);
    after.UpdatePipeline(draw + 1, true);
    old_commands += emit(before, desired, old_driver);
    new_commands += emit(after, desired, new_driver);
    check(old_driver == new_driver);
  }
  check(old_commands == 10000 && new_commands == 10);
  std::cout << "Synthetic 1000-pipeline state replay: " << old_commands << " -> "
            << new_commands << " dynamic commands, identical state (NOT an FPS benchmark)\n";
}
int main() {
  state_replay(); index_bindings(); stencil_fields(); hint_threads(); equivalent_workload();
  std::cout << "PASS: state replay, stencil field replay, static invalidation, index identity, ADPF priority and hysteresis\n";
}

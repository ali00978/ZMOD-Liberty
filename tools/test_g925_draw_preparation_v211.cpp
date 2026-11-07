// Host replay of production policies. Requires Vulkan headers, never a driver.
#include <bit>
#include <iostream>
#include <map>
#include <stdexcept>
#include "graphics/gta4_native/native_draw_preparation.h"
#include "graphics/gta4_native/native_draw_state_cache.h"

using namespace rex::graphics::gta4_native;
static void check(bool ok) { if (!ok) throw std::runtime_error("draw preparation regression"); }
static uint32_t seed = 0x925211;
static uint32_t next() { return seed = seed * 1664525u + 1013904223u; }
static auto bits(const VkViewport& v) {
  return std::array<uint32_t, 6>{std::bit_cast<uint32_t>(v.x), std::bit_cast<uint32_t>(v.y),
      std::bit_cast<uint32_t>(v.width), std::bit_cast<uint32_t>(v.height),
      std::bit_cast<uint32_t>(v.minDepth), std::bit_cast<uint32_t>(v.maxDepth)};
}
static auto rect(const VkRect2D& s) {
  return std::array<int64_t, 4>{s.offset.x, s.offset.y, s.extent.width, s.extent.height};
}
// Pre-2.11 draw calculation, kept independent of the new raster helper.
static NativeRasterArea legacy(const NativeRasterAreaKey& k) {
  NativeRasterArea r;
  r.scale_x = k.logical_width ? float(k.width) / float(k.logical_width) : 1.0f;
  r.scale_y = k.logical_height ? float(k.height) / float(k.logical_height) : 1.0f;
  struct Limits { uint32_t maxViewportDimensions[2]; float viewportBoundsRange[2]; } limits{
      {k.maximum_dimensions[0], k.maximum_dimensions[1]},
      {std::bit_cast<float>(k.bounds_bits[0]), std::bit_cast<float>(k.bounds_bits[1])}};
  r.viewport = SelectNativeViewport(std::bit_cast<std::array<float, 6>>(k.viewport_bits),
      r.scale_x, r.scale_y, k.width, k.height, limits);
  const auto floor = [](int32_t v, uint32_t l, uint32_t p) {
    if (v <= 0 || !l || l == p) return v;
    return int32_t((uint64_t(uint32_t(v)) * p) / l);
  };
  const auto ceil = [](int32_t v, uint32_t l, uint32_t p) {
    if (v <= 0 || !l || l == p) return v;
    const uint64_t numerator = uint64_t(uint32_t(v)) * p + l - 1;
    return int32_t(numerator / l);
  };
  const int32_t l = std::clamp(k.scissor[0], int32_t(0), int32_t(k.logical_width));
  const int32_t t = std::clamp(k.scissor[1], int32_t(0), int32_t(k.logical_height));
  const int32_t rr = std::clamp(k.scissor[2], l, int32_t(k.logical_width));
  const int32_t bb = std::clamp(k.scissor[3], t, int32_t(k.logical_height));
  const int32_t x = floor(l, k.logical_width, k.width), y = floor(t, k.logical_height, k.height);
  r.scissor.offset = {x, y};
  r.scissor.extent = {uint32_t(ceil(rr, k.logical_width, k.width) - x),
                      uint32_t(ceil(bb, k.logical_height, k.height) - y)};
  if (r.viewport.empty || k.suppress_coverage) r.scissor = {};
  return r;
}
static void same(const NativeRasterArea& a, const NativeRasterArea& b) {
  check(bits(a.viewport.viewport) == bits(b.viewport.viewport));
  check(a.viewport.empty == b.viewport.empty && rect(a.scissor) == rect(b.scissor));
  check(std::bit_cast<uint32_t>(a.scale_x) == std::bit_cast<uint32_t>(b.scale_x));
  check(std::bit_cast<uint32_t>(a.scale_y) == std::bit_cast<uint32_t>(b.scale_y));
}
static NativeRasterAreaKey base_key() {
  return {std::bit_cast<std::array<uint32_t, 6>>(std::array<float, 6>{0, 0, 1280, 720, 0, 1}),
      {0, 0, 1280, 720}, 1920, 1080, 1280, 720, {16384, 16384},
      {std::bit_cast<uint32_t>(-32768.0f), std::bit_cast<uint32_t>(32767.0f)}, false};
}
static void raster_replay() {
  NativeRasterAreaMemo memo;
  const auto base = base_key();
  memo.Store(base, legacy(base));
  // Every dependency must invalidate, including float payload and device limits.
  for (unsigned field = 0; field < 19; ++field) {
    auto k = base;
    if (field < 6) k.viewport_bits[field] ^= 1;
    else if (field < 10) ++k.scissor[field - 6];
    else if (field == 10) ++k.width;
    else if (field == 11) ++k.height;
    else if (field == 12) ++k.logical_width;
    else if (field == 13) ++k.logical_height;
    else if (field < 16) ++k.maximum_dimensions[field - 14];
    else if (field < 18) k.bounds_bits[field - 16] ^= 1;
    else k.suppress_coverage = true;
    check(!memo.Find(k));
  }
  // The prior one-entry memo recomputed the full viewport/scissor calculation
  // on every A/B alternation. Two exact entries must preserve both results.
  auto second = base;
  second.scissor[0] += 16;
  const auto second_value = legacy(second);
  memo.Store(second, second_value);
  for (unsigned i = 0; i < 10000; ++i) {
    check(memo.Find(base) && memo.Find(second));
    same(*memo.Find(base), legacy(base));
    same(*memo.Find(second), second_value);
  }
  auto third = base;
  third.scissor[1] += 8;
  memo.Store(third, legacy(third));
  check(!memo.Find(base) && memo.Find(second) && memo.Find(third));
  memo.Reset();
  check(!memo.Find(base) && !memo.Find(second) && !memo.Find(third));
  unsigned computations = 0;
  for (unsigned draw = 0; draw < 20000; ++draw) {
    auto k = base;
    k.width = next() % 4097; k.height = next() % 2161;
    k.logical_width = next() % 2049; k.logical_height = next() % 1081;
    for (auto& v : k.scissor) v = int32_t(next() % 4000) - 500;
    auto vp = std::array<float, 6>{float(int32_t(next()%500)-250), float(int32_t(next()%500)-250),
        float(next()%2000), float(next()%1200), float(next()%20)/10, float(next()%20)/10};
    k.viewport_bits = std::bit_cast<std::array<uint32_t, 6>>(vp);
    if (draw % 11 == 0) k.viewport_bits[draw % 6] = next(); // NaN/Inf/negative/denormal too
    if (draw % 17 == 0) k.viewport_bits[0] = 0x80000000;
    if (draw % 19 == 0) k.viewport_bits[2] = 0x7fc00001;
    if (draw % 23 == 0) k.viewport_bits[3] = 0x7f800000;
    k.maximum_dimensions = {next()%16385, next()%16385};
    k.bounds_bits = {std::bit_cast<uint32_t>(-float(next()%32769)),
                     std::bit_cast<uint32_t>(float(next()%32768))};
    k.suppress_coverage = draw % 7 == 0;
    const auto expected = legacy(k);
    for (unsigned repeat = 0; repeat < 10; ++repeat) {
      same(memo.Get(k, [&] { ++computations; return ComputeNativeRasterArea(k); }), expected);
    }
  }
  check(computations == 20000);
  memo.Reset(); check(!memo.Find(base));
  std::cout << "PASS: 200000 raster replays, 20000 calculations, identical legacy state\n";
}
static void blend_replay() {
  constexpr std::array<VkFormat, 5> formats{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R16G16_SFLOAT,
      VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED};
  for (unsigned i = 0; i < 20000; ++i) {
    std::array<uint32_t, 4> controls{}, values{};
    std::array<VkFormat, 4> targets{};
    for (unsigned j = 0; j < 4; ++j) { controls[j]=next(); values[j]=next(); targets[j]=formats[next()%5]; }
    const auto constants = std::bit_cast<std::array<float, 4>>(values);
    const uint32_t mask = next();
    for (bool supported : {false, true}) {
      const auto expected = SelectNativeBlendConstants(controls, NativeBlendWriteMask(mask, targets), constants, supported);
      const auto actual = SelectNativeDrawBlendConstants(controls, mask, targets, constants, supported);
      check(expected.representable == actual.representable);
      check(std::bit_cast<std::array<uint32_t,4>>(expected.constants) ==
            std::bit_cast<std::array<uint32_t,4>>(actual.constants));
    }
  }
  std::cout << "PASS: 40000 blend replays, native and portability paths\n";
}
static void push_replay() {
  NativeDrawStateCache<2> cache;
  std::array<uint64_t, 3> driver{}, desired{1,2,3};
  uint64_t driver_layout = 0, layout = 42;
  unsigned emitted = 0;
  for (unsigned i = 0; i < 1000; ++i) {
    cache.UpdatePipeline(i + 1, true);
    if (cache.UpdatePushConstants(layout, desired)) { driver = desired; driver_layout = layout; ++emitted; }
    check(driver == desired && driver_layout == layout);
  }
  check(emitted == 1);
  for (unsigned i = 0; i < 20000; ++i) {
    if (i % 13 == 0) { cache.Reset(); driver = {}; driver_layout = 0; }
    if (i % 17 == 0) { cache.UpdatePipeline(0xffff); driver = {}; driver_layout = 0; }
    if (i % 7 == 0) layout = next();
    if (i % 5 == 0) desired[i % 3] = next();
    cache.UpdatePipeline(i + 1, true);
    if (cache.UpdatePushConstants(layout, desired)) { driver = desired; driver_layout = layout; }
    check(driver == desired && driver_layout == layout);
  }
  std::cout << "PASS: 21000 push-constant draws; 1000 equal-address pipeline changes emit once\n";
}
struct Allocation {
  uint64_t buffer, offset, address, mapping, host;
  bool operator==(const Allocation&) const = default;
};
static void allocation_replay() {
  std::array<NativeExactMemo<uint64_t, Allocation>, 3> slots;
  for (unsigned frame = 0; frame < 1000; ++frame) {
    auto& memo = slots[frame % 3];
    memo.Reset(); check(!memo.Find(1));
    std::map<uint64_t, Allocation> arena;
    for (unsigned i = 0; i < 100; ++i) {
      const uint64_t identity = 1 + (i / 10) % 3;
      const Allocation allocation{frame+1, identity*256, 0x100000+frame*4096+identity*256,
                                  frame*8192+identity*256, frame*8192+identity*256};
      arena.insert_or_assign(identity, allocation);
      const auto* found = memo.Find(identity);
      const auto actual = found ? *found : memo.Store(identity, arena.at(identity));
      check(actual == arena.at(identity));
    }
    memo.Reset(); // backing storage replacement, even if IDs/handles are reused
    check(!memo.Find(1) && !memo.Find(2) && !memo.Find(3));
  }
  std::cout << "PASS: 100000 allocation lookups, 1000 slot/identity resets\n";
}
int main() { raster_replay(); blend_replay(); push_replay(); allocation_replay(); }

// Host-only G925 2.24 rectangle list Vulkan draw-elision proof.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <vector>
#include "graphics/gta4_native/native_rectangle_batch.h"

using namespace rex::graphics::gta4_native;

// A Vulkan indexed triangle strip creates a new primitive sequence whenever
// it encounters the all-ones primitive restart sentinel. Odd triangles flip
// winding, just like the original one-draw-per-rectangle non-indexed strip.
static std::vector<std::array<uint32_t, 3>> TrianglesFromRestartStrip(
    std::span<const uint32_t> indices) {
  std::vector<std::array<uint32_t, 3>> triangles;
  std::array<uint32_t, 2> previous{};
  uint32_t length = 0;
  for (uint32_t index : indices) {
    if (index == kNativeRectangleRestartIndex) {
      length = 0;
      continue;
    }
    if (length >= 2) {
      if ((length & 1) == 0) {
        triangles.push_back({previous[0], previous[1], index});
      } else {
        triangles.push_back({previous[1], previous[0], index});
      }
    }
    if (length == 0) {
      previous[0] = index;
    } else if (length == 1) {
      previous[1] = index;
    } else {
      previous[0] = previous[1];
      previous[1] = index;
    }
    ++length;
  }
  return triangles;
}

static std::vector<std::array<uint32_t, 3>> OriginalSeparateStrips(uint32_t rectangles) {
  std::vector<std::array<uint32_t, 3>> reference;
  for (uint32_t i = 0; i < rectangles; ++i) {
    const uint32_t first = i * 4;
    reference.push_back({first, first + 1, first + 2});
    reference.push_back({first + 2, first + 1, first + 3});
  }
  return reference;
}

int main() {
  assert(!NativeRectangleStripIndexCount(0));
  assert(NativeRectangleStripIndexCount(1) == 4);
  assert(NativeRectangleStripIndexCount(2) == 9);
  assert(NativeRectangleStripIndexCount(64) == 319);
  const uint32_t max_ok = uint32_t((uint64_t(UINT32_MAX) + 1) / 5);
  assert(NativeRectangleStripIndexCount(max_ok) != 0);
  assert(!NativeRectangleStripIndexCount(max_ok + 1));
  assert(!NativeRectangleStripIndexCount(UINT32_MAX));
  std::array<uint32_t, 8> guard{};
  guard.fill(0x12345678u);
  assert(!WriteNativeRectangleStripIndices(guard, 2)); // 9-word request must reject
  for (uint32_t word : guard) assert(word == 0x12345678u);
  assert(!WriteNativeRectangleStripIndices(guard, 0));
  for (uint32_t word : guard) assert(word == 0x12345678u);

  for (uint32_t count = 1; count <= 1024; ++count) {
    std::vector<uint32_t> uploaded(NativeRectangleStripIndexCount(count) + 2, 0xA5A5A5A5u);
    auto exact = std::span<uint32_t>(uploaded.data() + 1,
                                     uploaded.size() - 2);
    assert(WriteNativeRectangleStripIndices(exact, count));
    assert(uploaded.front() == 0xA5A5A5A5u && uploaded.back() == 0xA5A5A5A5u);
    const auto emitted = TrianglesFromRestartStrip(exact);
    assert(emitted == OriginalSeparateStrips(count));
    assert(emitted.size() == count * 2);
    for (const auto& tri : emitted)
      for (uint32_t vertex : tri)
        assert(vertex < count * 4);
    assert(std::count(exact.begin(), exact.end(), kNativeRectangleRestartIndex) == count - 1);
  }
  std::cout << "PASS: 1024 rectangle scenes preserve exact triangle order and winding, "
               "valid restart separation, overflow, failed-write and bounds guards\n";
}

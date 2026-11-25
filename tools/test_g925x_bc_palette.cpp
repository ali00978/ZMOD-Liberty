#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_bc_palette_expand.h"
using rex::graphics::gta4_native::bc_detail::ExpandColorPalette;
int main() {
  std::mt19937 rng(0x92505);
  for (unsigned trial = 0; trial < 200000; ++trial) {
    uint32_t palette[4][4];
    for (auto& entry : palette)
      for (auto& channel : entry) channel = rng() & 255u;
    uint32_t selectors = rng();
    if (trial < 256) selectors = trial * 0x01010101u;
    std::array<uint8_t, 66> actual{}, expected{};
    actual.fill(0xAA); expected.fill(0xAA);
    for (unsigned i = 0; i < 16; ++i)
      for (unsigned c = 0; c < 4; ++c)
        expected[1 + i * 4 + c] = static_cast<uint8_t>(palette[(selectors >> (2 * i)) & 3u][c]);
    ExpandColorPalette(palette, selectors, actual.data() + 1);
    assert(actual == expected);
  }
#if defined(__aarch64__) && defined(__ARM_NEON)
  std::cout << "PASS: ARM64 NEON palette expansion, 200000 exact blocks\n";
#else
  std::cout << "PASS: scalar palette expansion, 200000 exact blocks\n";
#endif
}

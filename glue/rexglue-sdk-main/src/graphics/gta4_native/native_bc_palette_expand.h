#pragma once
#include <cstdint>
#include <cstring>
#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif
namespace rex::graphics::gta4_native::bc_detail {
// Exact RGBA palette expansion. Four pixels per NEON table lookup, no
// aligned-load requirement and no changes to interpolation or alpha values.
inline void ExpandColorPalette(const uint32_t (&p)[4][4], uint32_t selectors,
                               uint8_t* rgba) {
  uint8_t palette[16];
  for (unsigned i = 0; i < 4; ++i)
    for (unsigned c = 0; c < 4; ++c)
      palette[i * 4 + c] = static_cast<uint8_t>(p[i][c]);
#if defined(__aarch64__) && defined(__ARM_NEON)
  const uint8x16_t table = vld1q_u8(palette);
  // Lane layout: one selector repeated across each pixel's four channels.
  const uint8_t shifts_data[16] = {0,0,0,0,2,2,2,2,4,4,4,4,6,6,6,6};
  const uint8_t channels_data[16] = {0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3};
  const int8x16_t shifts = vnegq_s8(vreinterpretq_s8_u8(vld1q_u8(shifts_data)));
  const uint8x16_t channels = vld1q_u8(channels_data);
  for (unsigned y = 0; y < 4; ++y) {
    const uint8x16_t codes = vdupq_n_u8(static_cast<uint8_t>(selectors >> (y * 8)));
    const uint8x16_t offset = vandq_u8(vshlq_u8(codes, shifts), vdupq_n_u8(3));
    const uint8x16_t lookup = vaddq_u8(vshlq_n_u8(offset, 2), channels);
    vst1q_u8(rgba + y * 16, vqtbl1q_u8(table, lookup));
  }
#else
  for (unsigned i = 0; i < 16; ++i) {
    std::memcpy(rgba + i * 4, palette + ((selectors & 3u) * 4), 4);
    selectors >>= 2;
  }
#endif
}
}  // namespace rex::graphics::gta4_native::bc_detail

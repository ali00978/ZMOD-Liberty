// G925 2.25: byte-exact single-pass primitive-restart conversion.
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <span>
#include <vector>
#include "graphics/gta4_native/native_index_conversion.h"

using namespace rex::graphics::gta4_native;

static void VerifyRestart(bool wide, uint32_t sentinel, uint32_t mask,
                          uint32_t count, uint32_t first, uint32_t seed) {
  std::mt19937 rng(seed);
  const size_t element = wide ? 4 : 2;
  const size_t total = size_t(first) + count + 3;
  std::vector<uint8_t> source(total * element + 2, 0xAD);
  std::vector<uint8_t> dest(size_t(count) * element + 2, 0xC7);
  for (size_t i = 0; i < total; ++i) {
    uint32_t value = rng();
    if (!wide) value &= 0xFFFFu;
    if (i % 7 == 0) {
      value = wide ? ((value & ~mask) | sentinel) : sentinel;
    } else if (i % 11 == 0) {
      value = wide ? (value ^ mask) : (value ^ 0x4F31u);
    }
    if (wide) std::memcpy(source.data() + 1 + i * element, &value, element);
    else {
      const uint16_t narrow = uint16_t(value);
      std::memcpy(source.data() + 1 + i * element, &narrow, element);
    }
  }
  const NativeConvertedIndexKey key{seed + 1, first, count, sentinel, mask,
                                    NativeIndexConversion::kRestart, wide};
  assert(WriteNativeConvertedIndices(
      std::span<const uint8_t>(source.data() + 1, total * element), key,
      std::span<uint8_t>(dest.data() + 1, size_t(count) * element)));
  assert(dest.front() == 0xC7 && dest.back() == 0xC7);
  assert(source.front() == 0xAD && source.back() == 0xAD);
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t raw = 0, actual = 0;
    std::memcpy(&raw, source.data() + 1 + (size_t(first) + i) * element, element);
    std::memcpy(&actual, dest.data() + 1 + size_t(i) * element, element);
    const bool marker = wide ? (raw & mask) == sentinel : raw == sentinel;
    const uint32_t expected = marker ? (wide ? UINT32_MAX : UINT16_MAX) : raw;
    assert(actual == expected);
  }
}

static void VerifyInvalidRequestsNeverWrite() {
  const std::array<uint8_t, 16> source{};
  std::array<uint8_t, 16> output;
  const auto unchanged = [&] {
    for (auto value : output) assert(value == 0xD2);
  };
  NativeConvertedIndexKey key{1, 5, 5, 0x1234, 0xFFFF,
                               NativeIndexConversion::kRestart, false};
  output.fill(0xD2);
  assert(!WriteNativeConvertedIndices(source, key, output));  // range overrun
  unchanged();
  key.first = 0;
  assert(!WriteNativeConvertedIndices(source, key, std::span<uint8_t>(output).first(8)));
  unchanged();  // destination too small
  key.count = 0;
  assert(!WriteNativeConvertedIndices(source, key, output));
  unchanged();
  key.count = 4;
  key.restart_index = 0x10000;
  assert(!WriteNativeConvertedIndices(source, key, output));
  unchanged();
  key.restart_index = 0x1234;
  key.conversion = static_cast<NativeIndexConversion>(100);
  assert(!WriteNativeConvertedIndices(source, key, output));
  unchanged();
}

int main() {
  VerifyInvalidRequestsNeverWrite();
  for (uint32_t seed = 0; seed < 64; ++seed) {
    for (const uint32_t count : {1u, 2u, 3u, 4u, 9u, 31u, 128u, 1024u}) {
      const uint32_t first = seed % 7;
      VerifyRestart(false, 0x1234u, 0u, count, first, seed);
      VerifyRestart(false, UINT16_MAX, 0u, count, first, seed);
      VerifyRestart(true, 0x00FFFFFFu, 0x00FFFFFFu, count, first, seed);
      VerifyRestart(true, 0x1234u, 0xFFFFu, count, first, seed);
      VerifyRestart(true, UINT32_MAX, UINT32_MAX, count, first, seed);
    }
  }
  std::cout << "PASS: G925 v2.25 single-pass restart conversion exact under unaligned, masked, full-width, guard and 16/32-bit cases\n";
}

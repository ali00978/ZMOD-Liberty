#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include "graphics/gta4_native/native_index_conversion.h"
#include "graphics/gta4_native/frame_constant_arena.h"
using namespace rex::graphics::gta4_native;

static std::vector<uint32_t> Reference(const std::vector<uint32_t>& words,
                                      const NativeConvertedIndexKey& k) {
  std::vector<uint32_t> out;
  if (k.conversion == NativeIndexConversion::kQuads) {
    for (size_t q = k.first; q < size_t(k.first) + k.count; q += 4)
      for (size_t corner : {0u, 1u, 2u, 0u, 2u, 3u}) out.push_back(words[q + corner]);
  } else {
    for (size_t i = k.first; i < size_t(k.first) + k.count; ++i) {
      const auto value = words[i];
      out.push_back((k.index32 ? (value & k.index_mask) : value) == k.restart_index
                       ? (k.index32 ? UINT32_MAX : UINT16_MAX) : value);
    }
  }
  return out;
}

static void Verify(const NativeConvertedIndexKey& key, std::vector<uint32_t> words) {
  const size_t element = key.index32 ? 4 : 2;
  if (!key.index32) for (auto& word : words) word &= UINT16_MAX;
  auto reference = Reference(words, key);
  std::vector<uint8_t> source(words.size() * element + 2, 0xAB);
  for (size_t i = 0; i < words.size(); ++i) {
    if (key.index32) std::memcpy(source.data() + 1 + i * element, &words[i], 4);
    else { uint16_t small = uint16_t(words[i]); std::memcpy(source.data() + 1 + i * element, &small, 2); }
  }
  const auto original = source;
  std::vector<uint8_t> output(reference.size() * element + 2, 0xCD);
  assert(WriteNativeConvertedIndices({source.data() + 1, source.size() - 2}, key,
                                     {output.data() + 1, output.size() - 2}));
  assert(source == original && output.front() == 0xCD && output.back() == 0xCD);
  for (size_t i = 0; i < reference.size(); ++i) {
    uint32_t actual = 0;
    if (key.index32) std::memcpy(&actual, output.data() + 1 + i * element, 4);
    else { uint16_t small; std::memcpy(&small, output.data() + 1 + i * element, 2); actual = small; }
    assert(actual == reference[i]);
    // Base vertex is not baked into converted data or a cache key.
    for (int32_t base : {-19, 0, 170}) assert(int64_t(actual) + base == int64_t(reference[i]) + base);
  }
  // Short destinations must be rejected before any partial write.
  std::fill(output.begin(), output.end(), 0xCD);
  assert(!WriteNativeConvertedIndices({source.data() + 1, source.size() - 2}, key,
                                      {output.data() + 1, output.size() - 3}));
  assert(std::all_of(output.begin(), output.end(), [](uint8_t b) { return b == 0xCD; }));
}

struct Collide { size_t operator()(const NativeConvertedIndexKey&) const { return 1; } };
int main() {
  assert(NativeQuadIndexCount(4) == 6 && NativeQuadIndexCount(8) == 12);
  assert(!NativeQuadIndexCount(0) && !NativeQuadIndexCount(5));
  assert(!NativeQuadIndexCount(UINT32_MAX - 3));
  constexpr uint32_t largest = (UINT32_MAX / 6) * 4;
  assert(NativeQuadIndexCount(largest) == (largest / 4) * 6);
  assert(!NativeQuadIndexCount(largest + 4));
  NativeConvertedIndexKey key{1, 0, 4, 7, 0x00FFFFFF, NativeIndexConversion::kRestart, true};
  Verify(key, {7, 0xAA000007u, 0xAA000008u, UINT32_MAX});
  key.index32 = false; key.restart_index = UINT16_MAX;
  Verify(key, {1, UINT16_MAX, 7, 9});

  // All-ones restart sentinel is byte-for-byte identical to the original
  // GPU index representation. Exercise the native bulk-copy fast path with
  // unaligned inputs and destination guards (Verify checks both).
  key = {91, 1, 4, UINT32_MAX, UINT32_MAX, NativeIndexConversion::kRestart, true};
  Verify(key, {123, UINT32_MAX, 0, 0xAB000123u, 7, 9});
  key = {92, 1, 4, UINT16_MAX, 0, NativeIndexConversion::kRestart, false};
  Verify(key, {123, UINT16_MAX, 0, 53, 7, 9});
  // An all-ones *masked* 32-bit marker still needs conversion, even though
  // its low 24 bits look like the native restart marker.
  key = {93, 0, 4, 0x00FFFFFF, 0x00FFFFFF,
         NativeIndexConversion::kRestart, true};
  Verify(key, {0xAAFFFFFFu, 0x00FFFFFFu, 123, 0xAABBCCDDu});

  std::mt19937 random(92529);
  for (uint32_t replay = 0; replay < 20000; ++replay) {
    key.generation = replay + 1;
    key.index32 = bool(random() & 1);
    key.first = random() % 5;
    key.count = (random() % 32 + 1) * 4;
    key.conversion = (random() & 1) ? NativeIndexConversion::kQuads : NativeIndexConversion::kRestart;
    key.restart_index = random() & (key.index32 ? 0x00FFFFFF : UINT16_MAX);
    key.index_mask = 0x00FFFFFF;
    std::vector<uint32_t> words(key.first + key.count + 3);
    for (auto& word : words) word = random();
    words[key.first] = key.restart_index;
    if (key.index32) words[key.first + 1] = key.restart_index | 0xAA000000;
    Verify(key, words);
  }
  std::array<uint8_t, 32> bytes{};
  key = {1, UINT32_MAX, 4, 0, 0, NativeIndexConversion::kQuads, true};
  assert(!WriteNativeConvertedIndices(bytes, key, bytes));
  key.first = 0; key.count = 5;
  assert(!WriteNativeConvertedIndices(bytes, key, bytes));
  key.count = 0;
  assert(!WriteNativeConvertedIndices(bytes, key, bytes));
  key = {1, 0, 4, 65536, UINT32_MAX, NativeIndexConversion::kRestart, false};
  assert(!WriteNativeConvertedIndices(bytes, key, bytes));

  // Force hash collisions and vary every semantic identity independently.
  FrameGenerationMap<NativeConvertedIndexKey, uint32_t, Collide> uploads;
  key = {1, 0, 4, 7, 0x00FFFFFF, NativeIndexConversion::kRestart, true};
  assert(uploads.Insert(key, 11));
  std::array<NativeConvertedIndexKey, 7> variants; variants.fill(key);
  ++variants[0].generation; ++variants[1].first; ++variants[2].count;
  ++variants[3].restart_index; ++variants[4].index_mask;
  variants[5].conversion = NativeIndexConversion::kQuads; variants[6].index32 = false;
  for (uint32_t i = 0; i < variants.size(); ++i) {
    assert(!uploads.Find(variants[i]));
    assert(uploads.Insert(variants[i], 20 + i));
    assert(*uploads.Find(key) == 11);
  }
  for (uint32_t frame = 0; frame < 1000; ++frame) {
    assert(uploads.CanResetGeneration() && uploads.ResetGeneration());
    assert(!uploads.Find(key));
    assert(uploads.Insert(key, frame + 100));
    for (int pass = 0; pass < 8; ++pass) assert(*uploads.Find(key) == frame + 100);
  }
  std::cout << "PASS: 20,000 byte-exact quad/restart replays, unaligned inputs, guards, "
               "overflow rejection, every key field, collisions and 1,000 frame resets\n";
}

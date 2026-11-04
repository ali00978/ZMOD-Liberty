#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace rex::graphics::gta4_native {

enum class NativeIndexConversion : uint8_t { kQuads, kRestart };

// Generation identifies immutable source bytes. Base vertex is deliberately
// absent: Vulkan applies it at draw time, after interpreting restart markers.
struct NativeConvertedIndexKey {
  uint64_t generation = 0;
  uint32_t first = 0, count = 0, restart_index = 0, index_mask = 0;
  NativeIndexConversion conversion = NativeIndexConversion::kQuads;
  bool index32 = false;
  bool operator==(const NativeConvertedIndexKey&) const = default;
};

struct NativeConvertedIndexKeyHash {
  size_t operator()(const NativeConvertedIndexKey& key) const noexcept {
    uint64_t h = key.generation;
    const auto mix = [&h](uint64_t value) {
      h = (h ^ value) * 0x9E3779B97F4A7C15ull;
      h ^= h >> 29;
    };
    mix(uint64_t(key.first) << 32 | key.count);
    mix(uint64_t(key.restart_index) << 32 | key.index_mask);
    mix(uint32_t(key.conversion) | (uint32_t(key.index32) << 8));
    return size_t(h);
  }
};

constexpr uint32_t NativeQuadIndexCount(uint32_t count) {
  const uint64_t expanded = uint64_t(count / 4) * 6;
  return count % 4 == 0 && expanded <= UINT32_MAX ? uint32_t(expanded) : 0;
}

// Source and destination contain host-endian words and must not overlap.
// Validate the whole request before writing, including expansion overflow.
inline bool WriteNativeConvertedIndices(std::span<const uint8_t> source,
                                        const NativeConvertedIndexKey& key,
                                        std::span<uint8_t> destination) {
  const size_t element = key.index32 ? sizeof(uint32_t) : sizeof(uint16_t);
  uint32_t output_count = 0;
  switch (key.conversion) {
    case NativeIndexConversion::kQuads: output_count = NativeQuadIndexCount(key.count); break;
    case NativeIndexConversion::kRestart:
      if (!key.index32 && key.restart_index > UINT16_MAX) return false;
      output_count = key.count;
      break;
    default: return false;
  }
  if (!output_count || uint64_t(key.first) + key.count > source.size() / element ||
      uint64_t(output_count) * element > destination.size()) return false;

  // Primitive restart with the full-width all-ones sentinel is already in
  // Vulkan's native representation. Copy the exact source bytes instead of
  // reading and rewriting every index individually. The source/destination
  // bounds have been checked above and the non-overlap contract is unchanged.
  // For masked 32-bit restart markers, conversion remains mandatory because
  // values such as 0xAAFFFFFF must become 0xFFFFFFFF.
  if (key.conversion == NativeIndexConversion::kRestart &&
      ((!key.index32 && key.restart_index == UINT16_MAX) ||
       (key.index32 && key.index_mask == UINT32_MAX &&
        key.restart_index == UINT32_MAX))) {
    std::memcpy(destination.data(), source.data() + size_t(key.first) * element,
                size_t(key.count) * element);
    return true;
  }

  const auto read = [&](uint32_t i) {
    uint32_t value = 0;
    const uint8_t* p = source.data() + (size_t(key.first) + i) * element;
    if (key.index32) std::memcpy(&value, p, sizeof(value));
    else { uint16_t small; std::memcpy(&small, p, sizeof(small)); value = small; }
    return value;
  };
  size_t cursor = 0;
  const auto emit = [&](uint32_t value) {
    if (key.index32) std::memcpy(destination.data() + cursor, &value, sizeof(value));
    else { const uint16_t small = uint16_t(value); std::memcpy(destination.data() + cursor, &small, sizeof(small)); }
    cursor += element;
  };
  if (key.conversion == NativeIndexConversion::kQuads) {
    for (uint32_t quad = 0; quad < key.count / 4; ++quad) {
      const uint32_t first = quad * 4;
      const uint32_t a = read(first), b = read(first + 1), c = read(first + 2), d = read(first + 3);
      emit(a); emit(b); emit(c); emit(a); emit(c); emit(d);
    }
  } else {
    for (uint32_t i = 0; i < key.count; ++i) {
      const uint32_t value = read(i);
      const bool restart = (key.index32 ? value & key.index_mask : value) == key.restart_index;
      emit(restart ? (key.index32 ? UINT32_MAX : UINT16_MAX) : value);
    }
  }
  return true;
}

}  // namespace rex::graphics::gta4_native

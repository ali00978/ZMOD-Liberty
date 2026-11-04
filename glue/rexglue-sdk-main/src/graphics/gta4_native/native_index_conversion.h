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

  if (key.conversion == NativeIndexConversion::kQuads) {
    // ABCD -> ABC ACD. Preserve the original host-endian bytes directly:
    // three bounded memcpy calls per quad instead of four decoded reads and
    // six encoded writes. No alignment assumptions for either index width.
    const uint8_t* src = source.data() + size_t(key.first) * element;
    uint8_t* dst = destination.data();
    for (uint32_t quad = 0; quad < key.count / 4; ++quad) {
      std::memcpy(dst, src, 3 * element);
      std::memcpy(dst + 3 * element, src, element);
      std::memcpy(dst + 4 * element, src + 2 * element, 2 * element);
      src += 4 * element;
      dst += 6 * element;
    }
  } else {
    // A masked primitive-restart marker must become the native all-ones
    // sentinel; all other values retain their original bit patterns.
    const uint8_t* src = source.data() + size_t(key.first) * element;
    uint8_t* dst = destination.data();
    for (uint32_t i = 0; i < key.count; ++i) {
      uint32_t value = 0;
      if (key.index32) std::memcpy(&value, src, sizeof(value));
      else { uint16_t small; std::memcpy(&small, src, sizeof(small)); value = small; }
      if ((key.index32 ? (value & key.index_mask) : value) == key.restart_index) {
        if (key.index32) { const uint32_t restart = UINT32_MAX;
          std::memcpy(dst, &restart, sizeof(restart)); }
        else { const uint16_t restart = UINT16_MAX;
          std::memcpy(dst, &restart, sizeof(restart)); }
      } else {
        std::memcpy(dst, src, element);
      }
      src += element;
      dst += element;
    }
  }
  return true;
}

}  // namespace rex::graphics::gta4_native

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rex::graphics::gta4_native {

// Exact keys: hash collisions may cause a miss, never a different surface.
// Placement/content and Vulkan layout tracking remain outside this cache.
struct NativeSurfaceLookupKey {
  std::array<uint32_t, 9> descriptor{};
  uint32_t samples = 0;
  bool depth = false;
  bool operator==(const NativeSurfaceLookupKey&) const = default;
};

template <typename Descriptor>
NativeSurfaceLookupKey MakeNativeSurfaceLookupKey(const Descriptor& d, bool depth,
                                                 uint32_t samples) {
  return {{d.handle, d.flags, d.base, d.address, d.packed_dimensions, d.format,
           d.width, d.height, d.sample_type}, samples, depth};
}

// Recorder-owned. Registry writers publish their metadata revision under the
// existing mutex; the recorder may reuse only the same published revision.
// Reflection changes, pending release and destruction explicitly invalidate.
template <typename Image, size_t Capacity = 64>
class NativeSurfaceLookupCache {
 public:
  Image* Find(const NativeSurfaceLookupKey& key, uint64_t registry_revision) const {
    const size_t start = SetIndex(key) * kWays;
    for (size_t way = 0; way < kWays; ++way) {
      const auto& entry = entries_[start + way];
      if (entry.epoch == epoch_ && entry.image &&
          entry.registry_revision == registry_revision && entry.key == key) {
        return entry.image;
      }
    }
    return nullptr;
  }

  void Store(const NativeSurfaceLookupKey& key, uint64_t registry_revision, Image* image) {
    if (!image) return;
    const size_t set = SetIndex(key);
    const size_t start = set * kWays;
    // Prefer refreshing a matching key, then a vacant/expired way. Keeping
    // two exact keys for the same surface handle avoids direct-mapped thrash
    // when reflection, format and sample variants alternate on adjacent draws.
    for (size_t way = 0; way < kWays; ++way) {
      auto& entry = entries_[start + way];
      if (entry.epoch == epoch_ && entry.image && entry.key == key) {
        entry = {key, registry_revision, image, epoch_};
        return;
      }
    }
    for (size_t way = 0; way < kWays; ++way) {
      auto& entry = entries_[start + way];
      if (entry.epoch != epoch_ || !entry.image ||
          entry.registry_revision != registry_revision) {
        entry = {key, registry_revision, image, epoch_};
        return;
      }
    }
    const size_t victim = size_t(next_victim_[set]++ % kWays);
    entries_[start + victim] = {key, registry_revision, image, epoch_};
  }

  void Invalidate() {
    // The registry owns image lifetimes. Invalidating by generation avoids
    // touching every cache line whenever a surface is released/replaced.
    // Stale pointers are never dereferenced; wrap-around cannot revive them.
    if (++epoch_ == 0) {
      for (auto& entry : entries_) entry = {};
      next_victim_.fill(0);
      epoch_ = 1;
    }
  }

 private:
  static_assert(Capacity && !(Capacity & (Capacity - 1)));
  static constexpr size_t kWays = Capacity >= 2 ? 2 : 1;
  static constexpr size_t kSetCount = Capacity / kWays;
  static size_t SetIndex(const NativeSurfaceLookupKey& key) {
    uint32_t h = key.descriptor[0] * 0x9E3779B9u;
    h ^= h >> 16;
    return (h ^ (key.samples * 17u) ^ uint32_t(key.depth)) & (kSetCount - 1);
  }
  struct Entry {
    NativeSurfaceLookupKey key{};
    uint64_t registry_revision = 0;
    Image* image = nullptr;
    uint64_t epoch = 0;
  };
  std::array<Entry, Capacity> entries_{};
  std::array<uint8_t, kSetCount> next_victim_{};
  uint64_t epoch_ = 1;
};

struct NativeVertexStreamKey {
  const void* shader = nullptr;
  const void* declaration = nullptr;
  uint64_t input_signature = 0;
  uint64_t declaration_hash = 0;
  bool operator==(const NativeVertexStreamKey&) const = default;
};

struct NativeVertexStreamSelection {
  uint32_t mask = 0;
  bool valid = false;
  bool operator==(const NativeVertexStreamSelection&) const = default;
};

// Copies of an immutable pipeline snapshot may carry this memo. Both resource
// identities and their input signatures must match before the copy can hit.
class NativeVertexStreamMemo {
 public:
  const NativeVertexStreamSelection* Find(const NativeVertexStreamKey& key) const {
    return populated_ && key_ == key ? &selection_ : nullptr;
  }
  void Store(const NativeVertexStreamKey& key, NativeVertexStreamSelection selection) {
    key_ = key;
    selection_ = selection;
    populated_ = true;
  }

 private:
  NativeVertexStreamKey key_{};
  NativeVertexStreamSelection selection_{};
  bool populated_ = false;
};

}  // namespace rex::graphics::gta4_native

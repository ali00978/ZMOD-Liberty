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
    const auto& entry = entries_[Index(key)];
    return entry.image && entry.registry_revision == registry_revision && entry.key == key
               ? entry.image : nullptr;
  }

  void Store(const NativeSurfaceLookupKey& key, uint64_t registry_revision, Image* image) {
    if (image) entries_[Index(key)] = {key, registry_revision, image};
  }

  void Invalidate() {
    for (auto& entry : entries_) entry.image = nullptr;
  }

 private:
  static_assert(Capacity && !(Capacity & (Capacity - 1)));
  static size_t Index(const NativeSurfaceLookupKey& key) {
    uint32_t h = key.descriptor[0] * 0x9E3779B9u;
    h ^= h >> 16;
    return (h ^ (key.samples * 17u) ^ uint32_t(key.depth)) & (Capacity - 1);
  }
  struct Entry {
    NativeSurfaceLookupKey key{};
    uint64_t registry_revision = 0;
    Image* image = nullptr;
  };
  std::array<Entry, Capacity> entries_{};
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

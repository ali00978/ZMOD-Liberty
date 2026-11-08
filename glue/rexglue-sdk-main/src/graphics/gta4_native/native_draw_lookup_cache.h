#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

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
    // Phone traces record around 6,800 successful surface lookups/frame.
    // Scene color, depth, reflection and presentation may cycle within a
    // single frame. Try four EXACT, recent identities before hashing the set
    // cache. Order uses lightweight indices: cached image entries never move.
    for (size_t probe = 0; probe < hot_order_.size(); ++probe) {
      const size_t slot = hot_order_[probe];
      const auto& hot = hot_[slot];
      // A color/depth switch usually changes the handle or role. Reject
      // that candidate before comparing its full nine-word descriptor.
      // A matching short identity is never sufficient to reuse an image:
      // the exact full key, epoch and registry revision remain mandatory.
      if (hot.key.descriptor[0] != key.descriptor[0] ||
          hot.key.depth != key.depth || hot.key.samples != key.samples) continue;
      if (hot.epoch == epoch_ && hot.image &&
          hot.registry_revision == registry_revision && hot.key == key) {
        PromoteHot(probe);
        return hot.image;
      }
    }
    const size_t start = SetIndex(key) * kWays;
    for (size_t way = 0; way < kWays; ++way) {
      const auto& entry = entries_[start + way];
      if (entry.epoch == epoch_ && entry.image &&
          entry.registry_revision == registry_revision && entry.key == key) {
        Remember(key, registry_revision, entry.image);
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
        Remember(key, registry_revision, image);
        return;
      }
    }
    for (size_t way = 0; way < kWays; ++way) {
      auto& entry = entries_[start + way];
      if (entry.epoch != epoch_ || !entry.image ||
          entry.registry_revision != registry_revision) {
        entry = {key, registry_revision, image, epoch_};
        Remember(key, registry_revision, image);
        return;
      }
    }
    const size_t victim = size_t(next_victim_[set]++ % kWays);
    // Eviction must also remove any hot shortcut to the old cache key.
    // Otherwise an evicted surface pointer could remain externally visible.
    Forget(entries_[start + victim]);
    entries_[start + victim] = {key, registry_revision, image, epoch_};
    Remember(key, registry_revision, image);
  }

  void Invalidate() {
    // The registry owns image lifetimes. Immediately discard borrowed hot
    // pointers; generation invalidation keeps the main array O(1).
    for (auto& hot : hot_) hot.image = nullptr;
    hot_order_ = {0, 1, 2, 3};
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
    // The handle alone clustered every format, address and size variant of
    // the same render target into one two-way set. Fold in a few cheap exact
    // descriptor fields so a title alternating 3+ variants can keep them in
    // separate sets without increasing cache capacity or GPU memory usage.
    h ^= key.descriptor[5] * 0x85EBCA6Bu;  // image format
    h ^= key.descriptor[4] * 0xC2B2AE35u;  // dimensions/packing
    h ^= key.descriptor[3] * 0x27D4EB2Du;  // guest address
    return (h ^ (key.samples * 17u) ^ uint32_t(key.depth)) & (kSetCount - 1);
  }
  struct Entry {
    NativeSurfaceLookupKey key{};
    uint64_t registry_revision = 0;
    Image* image = nullptr;
    uint64_t epoch = 0;
  };
  // Recorder-local, borrowed pointers; no ownership or cross-frame lifetime.
  // A true MRU ordering ensures third/fourth surface variants do not evict
  // a hot color/depth pair. Only the index order changes on cache hits.
  void PromoteHot(size_t position) const {
    const size_t slot = hot_order_[position];
    for (; position > 0; --position) hot_order_[position] = hot_order_[position - 1];
    hot_order_[0] = slot;
  }
  void Remember(const NativeSurfaceLookupKey& key, uint64_t revision, Image* image) const {
    for (size_t position = 0; position < hot_order_.size(); ++position) {
      const size_t slot = hot_order_[position];
      if (hot_[slot].image && hot_[slot].epoch == epoch_ && hot_[slot].key == key) {
        hot_[slot] = {key, revision, image, epoch_};
        PromoteHot(position);
        return;
      }
    }
    const size_t victim = hot_order_.back();
    hot_[victim] = {key, revision, image, epoch_};
    PromoteHot(hot_order_.size() - 1);
  }
  void Forget(const Entry& evicted) {
    if (evicted.epoch != epoch_ || !evicted.image) return;
    for (auto& entry : hot_) {
      if (entry.image && entry.epoch == epoch_ && entry.key == evicted.key)
        entry.image = nullptr;
    }
  }
  mutable std::array<Entry, 4> hot_{};
  mutable std::array<size_t, 4> hot_order_{0, 1, 2, 3};
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
// Two exact entries allow an immutable pipeline snapshot to reuse alternating
// stream declarations without repeating expensive vertex-layout discovery.
// No shader or declaration pointer is dereferenced by the cache itself.
class NativeVertexStreamMemo {
 public:
  const NativeVertexStreamSelection* Find(const NativeVertexStreamKey& key) const {
    if (recent_.valid && recent_.key == key) return &recent_.selection;
    if (previous_.valid && previous_.key == key) return &previous_.selection;
    return nullptr;
  }
  void Store(const NativeVertexStreamKey& key, NativeVertexStreamSelection selection) {
    if (recent_.valid && recent_.key == key) {
      recent_.selection = selection;
    } else if (previous_.valid && previous_.key == key) {
      // Promote a previously cached identity without dropping either result.
      std::swap(recent_, previous_);
      recent_.selection = selection;
    } else {
      previous_ = recent_;
      recent_ = {key, selection, true};
    }
  }

 private:
  struct Entry {
    NativeVertexStreamKey key{};
    NativeVertexStreamSelection selection{};
    bool valid = false;
  };
  Entry recent_{};
  Entry previous_{};
};

}  // namespace rex::graphics::gta4_native

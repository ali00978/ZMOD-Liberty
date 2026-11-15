#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace rex::graphics::gta4_native {
// Borrowed descriptor handles only. The frame pool owns their lifetime.
// No entry survives pool reset or a switch of the recording context.
template <typename Descriptor>
class NativeResolveDescriptorCache {
 public:
  struct Key {
    uint64_t view = 0, surface_lifetime = 0, layout = 0;
    uint32_t image_layout = 0;
    bool operator==(const Key&) const = default;
  };
  struct Statistics { uint64_t hits = 0, misses = 0, inserts = 0; };
  void BeginScope(uint64_t command_buffer, uint64_t pool) {
    if (command_buffer_ != command_buffer || pool_ != pool) {
      Reset(); command_buffer_ = command_buffer; pool_ = pool;
    }
  }
  void Reset() {
    // Cached descriptors belong to the command buffer's descriptor pool.
    // Invalidate the one-entry hot lookup on every pool/context transition.
    entries_.clear(); 
    for (auto& recent : hot_) recent.valid = false;
    command_buffer_ = pool_ = 0;
  }
  Descriptor Find(const Key& key) {
    if (Valid(key)) {
      // Multiple postfx targets alternate during a single frame. The four
      // exact recent descriptors bypass hashing and unordered_map traversal
      // while preserving the command-buffer/pool lifetime checked on Reset.
      for (size_t i = 0; i < hot_.size(); ++i) {
        if (hot_[i].valid && hot_[i].key == key) {
          const Descriptor descriptor = hot_[i].descriptor;
          Promote(i);
          ++statistics_.hits;
          return descriptor;
        }
      }
      const auto found = entries_.find(key);
      if (found != entries_.end()) {
        Remember(key, found->second);
        ++statistics_.hits;
        return found->second;
      }
    }
    ++statistics_.misses; return {};
  }
  void Insert(const Key& key, Descriptor descriptor) {
    if (!descriptor || !Valid(key)) return;
    // Duplicate inserts must leave the existing valid handle untouched,
    // especially at capacity. The old path cleared all 256 entries before
    // checking for duplicates, causing a burst of redundant allocations.
    const auto existing = entries_.find(key);
    if (existing != entries_.end()) {
      Remember(key, existing->second);
      return;
    }
    // Pre-size the per-command-pool map once; Reset()/clear() keeps the
    // buckets for the next recording scope without keeping old handles.
    if (entries_.empty()) entries_.reserve(kCapacity);
    // A new descriptor only displaces ONE old entry, not the entire cache.
    // The unordered_map entry is deliberately an arbitrary victim: descriptor
    // validity is still controlled by the exact key and BeginScope/Reset.
    if (entries_.size() >= kCapacity) {
      const auto victim = entries_.begin();
      // Exact MRU entries must never outlive their backing pool-owned key.
      Forget(victim->first);
      entries_.erase(victim);
    }
    if (entries_.emplace(key, descriptor).second) {
      ++statistics_.inserts;
      Remember(key, descriptor);
    }
  }
  const Statistics& statistics() const { return statistics_; }
  size_t size() const { return entries_.size(); }
  static constexpr size_t kCapacity = 256;
 private:
  bool Valid(const Key& key) const {
    return command_buffer_ && pool_ && key.view && key.surface_lifetime && key.layout;
  }
  struct Hash {
    size_t operator()(const Key& key) const {
      size_t h = std::hash<uint64_t>{}(key.view);
      const auto mix = [&](uint64_t v) {
        h ^= std::hash<uint64_t>{}(v) + size_t(0x9e3779b9u) + (h << 6) + (h >> 2);
      };
      mix(key.surface_lifetime); mix(key.layout); mix(key.image_layout); return h;
    }
  };
  struct HotEntry {
    Key key{};
    Descriptor descriptor{};
    bool valid = false;
  };
  // Recorder-local and non-owning; never allowed to outlive the pool scope.
  // Four entries cover common scene/depth/reflection/postfx alternation.
  void Promote(size_t index) {
    if (!index) return;
    const HotEntry active = hot_[index];
    for (; index > 0; --index) hot_[index] = hot_[index - 1];
    hot_[0] = active;
  }
  void Remember(const Key& key, Descriptor descriptor) {
    for (size_t i = 0; i < hot_.size(); ++i) {
      if (hot_[i].valid && hot_[i].key == key) {
        hot_[i].descriptor = descriptor;
        Promote(i);
        return;
      }
    }
    for (size_t i = hot_.size() - 1; i > 0; --i) hot_[i] = hot_[i - 1];
    hot_[0] = {key, descriptor, true};
  }
  void Forget(const Key& key) {
    for (auto& recent : hot_)
      if (recent.valid && recent.key == key) recent.valid = false;
  }
  std::array<HotEntry, 4> hot_{};
  std::unordered_map<Key, Descriptor, Hash> entries_;
  uint64_t command_buffer_ = 0, pool_ = 0;
  Statistics statistics_;
};
}  // namespace rex::graphics::gta4_native

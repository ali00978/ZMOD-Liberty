#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace rex::graphics::gta4_native {

// Four exact entries with a one-comparison MRU hit. Real recording frequently
// cycles among scene, reflection, depth and presenter raster identities;
// retaining all four avoids repeated viewport/scissor computation and repeated
// frame-constant allocation lookups on 3/4-state workloads.
//
// No hashing, allocation, GPU ownership or cross-frame reuse. Values never
// move on a hit: the small index order changes instead of copying raster or
// mapped-buffer objects. The owner must Reset when its backing storage changes.
template <typename Key, typename Value>
class NativeExactMemo {
 public:
  const Value* Find(const Key& key) const {
    for (size_t position = 0; position < kSlots; ++position) {
      const auto& entry = entries_[order_[position]];
      if (entry.valid && entry.key == key) return &entry.value;
    }
    return nullptr;
  }

  const Value& Store(const Key& key, const Value& value) {
    for (size_t position = 0; position < kSlots; ++position) {
      auto& entry = entries_[order_[position]];
      if (entry.valid && entry.key == key) {
        entry.value = value;
        Promote(position);
        return entry.value;
      }
    }
    const size_t victim = order_[kSlots - 1];
    auto& entry = entries_[victim];
    entry.key = key;
    entry.value = value;
    entry.valid = true;
    Promote(kSlots - 1);
    return entry.value;
  }

  // Draw hot path: inspect each key at most once, compute only on a miss.
  // Promote hits, so a rare additional state does not evict a frequently
  // alternating pair. A cache miss replaces only the least-recently-used slot.
  template <typename Compute>
  const Value& Get(const Key& key, Compute compute) {
    for (size_t position = 0; position < kSlots; ++position) {
      auto& entry = entries_[order_[position]];
      if (entry.valid && entry.key == key) {
        Promote(position);
        return entry.value;
      }
    }
    Value computed = compute();
    const size_t victim = order_[kSlots - 1];
    auto& entry = entries_[victim];
    entry.value = std::move(computed);
    entry.key = key;
    entry.valid = true;
    Promote(kSlots - 1);
    return entry.value;
  }

  void Reset() {
    for (auto& entry : entries_) entry.valid = false;
    order_ = {0, 1, 2, 3};
  }

 private:
  static constexpr size_t kSlots = 4;
  void Promote(size_t position) {
    const size_t slot = order_[position];
    for (; position > 0; --position) order_[position] = order_[position - 1];
    order_[0] = slot;
  }
  struct Entry {
    Key key{};
    Value value{};
    bool valid = false;
  };
  std::array<Entry, kSlots> entries_{};
  std::array<size_t, kSlots> order_{0, 1, 2, 3};
};

}  // namespace rex::graphics::gta4_native

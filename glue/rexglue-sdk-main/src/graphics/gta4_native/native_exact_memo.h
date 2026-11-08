#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace rex::graphics::gta4_native {

// Two exact entries, no hashing or heap allocation. Alternating frame-local
// raster/constant-allocation identities no longer force recomputation on every
// other draw. The newest entry is tested first for the common repeated-key
// case. The caller owns resource lifetime and MUST Reset before replacing
// backing storage or reusing identities. No cross-frame GPU resource is owned.
template <typename Key, typename Value>
class NativeExactMemo {
 public:
  const Value* Find(const Key& key) const {
    if (entries_[most_recent_].valid && entries_[most_recent_].key == key) {
      return &entries_[most_recent_].value;
    }
    const size_t other = most_recent_ ^ size_t(1);
    if (entries_[other].valid && entries_[other].key == key) {
      return &entries_[other].value;
    }
    return nullptr;
  }

  const Value& Store(const Key& key, const Value& value) {
    // Store of an existing exact identity refreshes that slot, without
    // destroying the other identity. Misses evict the older slot only.
    if (!(entries_[most_recent_].valid && entries_[most_recent_].key == key)) {
      most_recent_ ^= size_t(1);
    }
    Entry& entry = entries_[most_recent_];
    entry.key = key;
    entry.value = value;
    entry.valid = true;
    return entry.value;
  }

  // Get is the draw-recording hot path. A hit in the older slot becomes
  // most-recently used, so an occasional third raster state does not evict the
  // state the recorder just consumed. Find() remains a read-only peek for
  // callers that do not want to change eviction order.
  // On a miss, Find()+Store() previously compared the latest large key twice.
  // Probe both slots once and fill the older slot directly.
  template <typename Compute>
  const Value& Get(const Key& key, Compute compute) {
    if (entries_[most_recent_].valid && entries_[most_recent_].key == key)
      return entries_[most_recent_].value;
    const size_t other = most_recent_ ^ size_t(1);
    if (entries_[other].valid && entries_[other].key == key) {
      most_recent_ = other;
      return entries_[other].value;
    }
    Value computed = compute();
    Entry& entry = entries_[other];
    entry.value = std::move(computed);
    entry.key = key;
    entry.valid = true;
    most_recent_ = other;
    return entry.value;
  }

  void Reset() {
    entries_[0].valid = false;
    entries_[1].valid = false;
    most_recent_ = 0;
  }

 private:
  struct Entry {
    Key key{};
    Value value{};
    bool valid = false;
  };
  std::array<Entry, 2> entries_{};
  size_t most_recent_ = 0;
};

}  // namespace rex::graphics::gta4_native

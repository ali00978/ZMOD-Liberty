#pragma once

#include <array>
#include <cstddef>

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

  template <typename Compute>
  const Value& Get(const Key& key, Compute compute) {
    if (const Value* value = Find(key)) return *value;
    return Store(key, compute());
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

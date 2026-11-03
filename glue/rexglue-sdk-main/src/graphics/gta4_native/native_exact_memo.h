#pragma once

namespace rex::graphics::gta4_native {

// One exact entry, no hashing or allocation. The caller owns resource lifetime
// and must Reset before replacing backing storage or reusing identities.
template <typename Key, typename Value>
class NativeExactMemo {
 public:
  const Value* Find(const Key& key) const {
    return valid_ && key == key_ ? &value_ : nullptr;
  }
  const Value& Store(const Key& key, const Value& value) {
    key_ = key;
    value_ = value;
    valid_ = true;
    return value_;
  }
  template <typename Compute>
  const Value& Get(const Key& key, Compute compute) {
    if (const Value* value = Find(key)) return *value;
    return Store(key, compute());
  }
  void Reset() { valid_ = false; }

 private:
  Key key_{};
  Value value_{};
  bool valid_ = false;
};

}  // namespace rex::graphics::gta4_native

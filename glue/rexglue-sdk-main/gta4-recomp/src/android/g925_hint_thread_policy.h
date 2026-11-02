#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace gta4::android {
// Keep critical game/recorder threads selected across brief 1Hz CPU-rank
// oscillations. Never retain dead TIDs or exceed ADPF session capacity.
// This selector only runs on the low-frequency Android hint worker.
template <typename Tid>
class G925StableHintThreads {
 public:
  explicit G925StableHintThreads(size_t cap = 6) : cap_(std::max<size_t>(1, cap)) {}
  void Reset() { chosen_.clear(); misses_.clear(); }
  template <typename Alive>
  const std::vector<Tid>& Select(const std::vector<Tid>& busy, Alive alive) {
    std::vector<Tid> next;
    next.reserve(cap_);
    for (Tid id : chosen_) {
      if (!alive(id)) { misses_.erase(id); continue; }
      auto& miss = misses_[id];
      if (std::find(busy.begin(), busy.end(), id) != busy.end()) {
        miss = 0;
        next.push_back(id);
      } else if (++miss < 3) {
        next.push_back(id);
      } else {
        misses_.erase(id);
      }
    }
    for (Tid id : busy) {
      if (next.size() >= cap_) break;
      if (alive(id) && std::find(next.begin(), next.end(), id) == next.end()) {
        next.push_back(id);
        misses_[id] = 0;
      }
    }
    std::sort(next.begin(), next.end());
    chosen_.swap(next);
    return chosen_;
  }
  const std::vector<Tid>& chosen() const noexcept { return chosen_; }
 private:
  size_t cap_;
  std::vector<Tid> chosen_;
  std::unordered_map<Tid, uint8_t> misses_;
};
}  // namespace gta4::android

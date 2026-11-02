#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "../../../gta4-recomp/src/android/g925_hint_thread_policy.h"

using gta4::android::G925StableHintThreads;

TEST_CASE("ADPF selection prevents 1-second core-role thrashing", "[g925][adpf]") {
  G925StableHintThreads<int> policy(3);
  auto alive = [](int) {return true;};
  CHECK(policy.Select({1,2,3}, alive) == std::vector<int>{1,2,3});
  CHECK(policy.Select({1,2,4}, alive) == std::vector<int>{1,2,3});
  CHECK(policy.Select({1,2,3}, alive) == std::vector<int>{1,2,3});
  CHECK(policy.Select({1,2,4}, alive) == std::vector<int>{1,2,3});
}
TEST_CASE("ADPF selector removes dead TIDs and persistently idle TIDs", "[g925][adpf]") {
  G925StableHintThreads<int> policy(3);
  auto alive = [](int id) { return id != 2; };
  CHECK(policy.Select({1,2,3}, [](int){return true;}) == std::vector<int>{1,2,3});
  CHECK(policy.Select({1,3,4}, alive) == std::vector<int>{1,3,4});
  CHECK(policy.Select({1,3,5}, alive) == std::vector<int>{1,3,4});
  CHECK(policy.Select({1,3,5}, alive) == std::vector<int>{1,3,4});
  CHECK(policy.Select({1,3,5}, alive) == std::vector<int>{1,3,5});
  policy.Reset();
  CHECK(policy.chosen().empty());
}

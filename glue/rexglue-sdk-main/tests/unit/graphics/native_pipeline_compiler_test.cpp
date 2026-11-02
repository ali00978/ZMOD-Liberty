#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <future>
#include <stdexcept>

#include "graphics/gta4_native/native_pipeline_compiler.h"

using Compiler = rex::graphics::gta4_native::NativePipelineCompiler<int, int, std::hash<int>>;

TEST_CASE("pipeline compiler bounds admission and deduplicates exact demand",
          "[gta4-native][pipeline-compiler]") {
  Compiler compiler(2);
  std::promise<void> entered;
  std::promise<void> release;
  auto gate = release.get_future().share();
  std::atomic<int> calls{0};
  REQUIRE(compiler.Enqueue(1, [&] {
    entered.set_value();
    gate.wait();
    ++calls;
    return 10;
  }));
  entered.get_future().wait();
  CHECK(compiler.Enqueue(1, [&] { ++calls; return 100; }));
  CHECK(compiler.Enqueue(2, [] { return 20; }));
  CHECK_FALSE(compiler.Enqueue(3, [] { return 30; }));
  CHECK(compiler.Enqueue(3, [] { return 30; }, true));
  CHECK_FALSE(compiler.Contains(2));
  release.set_value();
  const auto demanded = compiler.Take(3, true);
  REQUIRE(demanded);
  CHECK(*demanded == 30);
  const auto first = compiler.Take(1, true);
  REQUIRE(first);
  CHECK(*first == 10);
  CHECK(calls == 1);
}

TEST_CASE("pipeline compiler joins active work and contains callback failures",
          "[gta4-native][pipeline-compiler]") {
  Compiler compiler(2);
  REQUIRE(compiler.Enqueue(1, []() -> int { throw std::runtime_error("cache I/O"); }));
  const auto failed = compiler.Take(1, true);
  REQUIRE(failed);
  CHECK(*failed == 0);
  std::promise<void> entered;
  std::promise<void> release;
  auto gate = release.get_future().share();
  std::atomic<bool> finished{false};
  REQUIRE(compiler.Enqueue(2, [&] {
    entered.set_value();
    gate.wait();
    finished = true;
    return 20;
  }));
  entered.get_future().wait();
  auto stop = std::async(std::launch::async, [&] { compiler.Stop(); });
  CHECK_FALSE(finished);
  release.set_value();
  stop.get();
  CHECK(finished);
  CHECK_FALSE(compiler.Enqueue(3, [] { return 30; }));
  const auto completed = compiler.TakeCompleted();
  REQUIRE(completed.size() == 1);
  CHECK(completed.front().second == 20);
}

TEST_CASE("pipeline compiler never discards demanded work to admit another demand",
          "[gta4-native][pipeline-compiler][stutter]") {
  Compiler compiler(3);
  std::promise<void> entered, release;
  auto gate = release.get_future().share();
  REQUIRE(compiler.Enqueue(1, [&] {
    entered.set_value();
    gate.wait();
    return 10;
  }, true));
  entered.get_future().wait();
  const bool demand2 = compiler.Enqueue(2, [] { return 20; }, true);
  const bool speculative3 = compiler.Enqueue(3, [] { return 30; });
  const bool demand4 = compiler.Enqueue(4, [] { return 40; }, true);
  const bool can_displace_demanded = compiler.Enqueue(5, [] { return 50; }, true);
  const bool demand2_preserved = compiler.Contains(2);
  const bool demand4_preserved = compiler.Contains(4);
  const bool speculative3_evicted = !compiler.Contains(3);
  release.set_value();
  const auto result2 = compiler.Take(2, true);
  const auto result4 = compiler.Take(4, true);
  const auto result1 = compiler.Take(1, true);
  CHECK(demand2);
  CHECK(speculative3);
  CHECK(demand4);
  CHECK_FALSE(can_displace_demanded);
  CHECK(demand2_preserved);
  CHECK(demand4_preserved);
  CHECK(speculative3_evicted);
  REQUIRE(result2);
  REQUIRE(result4);
  REQUIRE(result1);
  CHECK(*result2 == 20);
  CHECK(*result4 == 40);
  CHECK(*result1 == 10);
}

TEST_CASE("pipeline compiler promotes a duplicate speculative request to demanded",
          "[gta4-native][pipeline-compiler][stutter]") {
  Compiler compiler(3);
  std::promise<void> entered, release;
  auto gate = release.get_future().share();
  REQUIRE(compiler.Enqueue(1, [&] {
    entered.set_value();
    gate.wait();
    return 10;
  }, true));
  entered.get_future().wait();
  const bool speculative2 = compiler.Enqueue(2, [] { return 20; });
  const bool speculative3 = compiler.Enqueue(3, [] { return 30; });
  // The second request is the moment this exact pipeline becomes needed.
  const bool promoted3 = compiler.Enqueue(3, [] { return -3; }, true);
  const bool demand4 = compiler.Enqueue(4, [] { return 40; }, true);
  const bool preserved3 = compiler.Contains(3);
  const bool evicted2 = !compiler.Contains(2);
  release.set_value();
  const auto result3 = compiler.Take(3, true);
  const auto result4 = compiler.Take(4, true);
  const auto result1 = compiler.Take(1, true);
  CHECK(speculative2);
  CHECK(speculative3);
  CHECK(promoted3);
  CHECK(demand4);
  CHECK(preserved3);
  CHECK(evicted2);
  REQUIRE(result3);
  REQUIRE(result4);
  REQUIRE(result1);
  CHECK(*result3 == 30);  // Deduplicate; never replace original recipe.
  CHECK(*result4 == 40);
  CHECK(*result1 == 10);
}

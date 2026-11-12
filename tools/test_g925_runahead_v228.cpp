#include <cassert>
#include <cstdint>
#include <random>

#include "../glue/rexglue-sdk-main/gta4-recomp/src/android/g925_run_ahead_policy.h"

using gta4::android::G925AdjustedPendingFrames;

static_assert(G925AdjustedPendingFrames(0, 4) == 0);
static_assert(G925AdjustedPendingFrames(1, 4) == 0);
static_assert(G925AdjustedPendingFrames(4, 4) == 0);
static_assert(G925AdjustedPendingFrames(5, 4) == 1);
static_assert(G925AdjustedPendingFrames(6, 4) == 2);
static_assert(G925AdjustedPendingFrames(0x7FFFFFFFu, 4) == 0x7FFFFFFBu);
static_assert(G925AdjustedPendingFrames(6, 999) == 2);
static_assert(G925AdjustedPendingFrames(6, 0) == 6);

int main() {
  std::mt19937 generator(0x925);
  for (uint32_t index = 0; index < 200000; ++index) {
    const uint32_t pending = generator() & 0x7FFFFFFFu;
    const uint32_t extra = generator() % 32;
    const uint32_t expected = pending > (extra < 4 ? extra : 4)
                                ? pending - (extra < 4 ? extra : 4) : 0;
    const uint32_t adjusted = G925AdjustedPendingFrames(pending, extra);
    assert(adjusted == expected);
    assert(adjusted <= pending);
    assert(adjusted <= 0x7FFFFFFFu);
  }
  for (uint32_t pending = 0; pending < 32; ++pending) {
    for (uint32_t extra = 0; extra <= 4; ++extra) {
      assert(G925AdjustedPendingFrames(pending, extra + 1) <=
             G925AdjustedPendingFrames(pending, extra));
    }
  }
}

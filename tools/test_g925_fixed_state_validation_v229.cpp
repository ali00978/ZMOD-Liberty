#include <cassert>
#include <cstddef>
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_fixed_state_validation_policy.h"

using rex::graphics::gta4_native::ShouldVerifyRecordedFixedState;

static_assert(ShouldVerifyRecordedFixedState(false, 0));
static_assert(ShouldVerifyRecordedFixedState(false, 15));
static_assert(!ShouldVerifyRecordedFixedState(false, 16));
static_assert(!ShouldVerifyRecordedFixedState(false, 63));
static_assert(ShouldVerifyRecordedFixedState(false, 64));
static_assert(ShouldVerifyRecordedFixedState(false, 128));
static_assert(!ShouldVerifyRecordedFixedState(false, 127));
static_assert(ShouldVerifyRecordedFixedState(true, 17));

int main() {
  size_t sampled = 0;
  for (size_t i = 0; i < 4096; ++i) {
    if (ShouldVerifyRecordedFixedState(false, i)) ++sampled;
    assert(ShouldVerifyRecordedFixedState(true, i));
  }
  // 16 initial commands plus 63 periodic checks in the remainder.
  assert(sampled == 79);
}

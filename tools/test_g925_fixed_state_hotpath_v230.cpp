#include <cassert>
#include <cstddef>
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_fixed_state_validation_policy.h"
using rex::graphics::gta4_native::ShouldCaptureRecordedFixedState;
using rex::graphics::gta4_native::ShouldVerifyRecordedFixedState;
static_assert(!ShouldCaptureRecordedFixedState(false, false, false, false));
static_assert(ShouldCaptureRecordedFixedState(true, false, false, false));
static_assert(ShouldCaptureRecordedFixedState(false, true, false, false));
static_assert(ShouldCaptureRecordedFixedState(false, false, true, false));
static_assert(ShouldCaptureRecordedFixedState(false, false, false, true));
int main() {
  unsigned normal_hashes=0, diagnostic_hashes=0, normal_verified=0;
  for (unsigned i=0;i<4096;++i) {
    if (ShouldCaptureRecordedFixedState(false,false,false,false)) ++normal_hashes;
    if (ShouldCaptureRecordedFixedState(true,false,false,false)) ++diagnostic_hashes;
    if (ShouldCaptureRecordedFixedState(false,false,false,false) &&
        ShouldVerifyRecordedFixedState(false,i)) ++normal_verified;
    assert(ShouldVerifyRecordedFixedState(true,i));
  }
  assert(normal_hashes==0 && normal_verified==0 && diagnostic_hashes==4096);
  for (unsigned f=1;f<16;++f)
    assert(ShouldCaptureRecordedFixedState(bool(f&1),bool(f&2),bool(f&4),bool(f&8)));
}

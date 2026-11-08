// Regression: keep under-budget cleanup small while retaining emergency drain.
#include <cassert>
#include <cstdint>
#include <iostream>
#include "graphics/gta4_native/native_frame_scheduling.h"
using namespace rex::graphics::gta4_native;
int main() {
  static_assert(kNativeSurfaceRetirementsPerFrame == 6);
  static_assert(NativeSurfaceRetirementLimit(false) == 6);
  static_assert(NativeSurfaceRetirementLimit(true) == size_t(-1));
  static_assert(ShouldKeepNativeSurfacePending(11, 10, 0, true));
  static_assert(ShouldKeepNativeSurfacePending(10, 10, 6, false));
  static_assert(!ShouldKeepNativeSurfacePending(10, 10, 5, false));
  static_assert(!ShouldKeepNativeSurfacePending(10, 10, 999, true));
  constexpr uint64_t mib = 1024ull * 1024;
  constexpr auto normal = GetNativeBufferReclamationWorkLimit(100 * mib, 256 * mib);
  constexpr auto emergency = GetNativeBufferReclamationWorkLimit(257 * mib, 256 * mib);
  static_assert(normal.max_resources == 8 && normal.max_bytes == 2 * mib);
  static_assert(emergency.max_resources == 64 && emergency.max_bytes == 16 * mib);
  std::cout << "PASS: bounded frame cleanup, forced drain, GPU in-flight safety and memory-pressure fallback\n";
}

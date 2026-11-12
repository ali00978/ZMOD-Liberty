#pragma once
#include <cstddef>

namespace rex::graphics::gta4_native {

// The expensive full-state serialization/hash is only an integrity diagnostic,
// never an input to recording a draw or selecting a Vulkan pipeline. Keep
// strong coverage during startup and across the frame, while allowing opt-in
// exhaustive validation for suspected state corruption. Draw state itself is
// always applied to every command without sampling.
constexpr bool ShouldVerifyRecordedFixedState(bool exhaustive, size_t command_index) {
  return exhaustive || command_index < 16 || (command_index & 63u) == 0;
}
}  // namespace rex::graphics::gta4_native

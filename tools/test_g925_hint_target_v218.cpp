#include <cassert>
#include <cstdint>
#include <iostream>
#include "android/g925_hint_target_policy.h"

int main() {
  using gta4::android::G925EffectiveHintTargetMs;
  // 60 FPS goal is 16.67ms; request a 16ms CPU work target from ADPF.
  static_assert(G925EffectiveHintTargetMs(16) == 16);
  // Previous launcher's Maximum option serialized 20ms: migrate to 16ms.
  static_assert(G925EffectiveHintTargetMs(20) == 16);
  // Explicit lower-power choices must remain unchanged.
  static_assert(G925EffectiveHintTargetMs(25) == 25);
  static_assert(G925EffectiveHintTargetMs(30) == 30);
  static_assert(G925EffectiveHintTargetMs(33) == 33);
  static_assert(G925EffectiveHintTargetMs(10) == 10);
  static_assert(G925EffectiveHintTargetMs(0) == 10);
  static_assert(G925EffectiveHintTargetMs(UINT32_MAX) == 50);
  for (uint32_t ms = 10; ms <= 50; ++ms) {
    const auto effective = G925EffectiveHintTargetMs(ms);
    assert(effective >= 10 && effective <= 50);
    if (ms != 20) assert(effective == ms);
  }
  std::cout << "PASS: 16ms maximum ADPF, legacy-20 migration, safe range and lower-power choices\n";
}

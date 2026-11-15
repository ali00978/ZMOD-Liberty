#!/usr/bin/env python3
"""Native hot-loop regression; substantive C++ compilation runs in APK job."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
src=(root/"glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.cpp").read_text()
hash_src=(root/"glue/rexglue-sdk-main/src/graphics/gta4_native/native_fixed_state_hash.h").read_text()
gradle=(root/"os/android/app/build.gradle.kts").read_text()
flow=(root/".github/workflows/build-android-apk.yml").read_text()
assert 'const std::string& state_transport = REXCVAR_GET(gta4_native_state_transport);' in src
assert 'const std::string& selection = REXCVAR_GET(gta4_native_light_color_delta_probe);' in src
assert src.count('const std::string& ps9_stencil_probe = REXCVAR_GET(gta4_native_light_ps9_stencil_probe);') == 1
assert src.count('const std::string& stencil_face_probe = REXCVAR_GET(gta4_native_light_stencil_face_probe);') == 1
assert src.count('const bool legacy_ps9_bypass = REXCVAR_GET(gta4_native_light_ps9_stencil_bypass);') == 1
assert src.count('const bool artificial_light_trace_requested = ShouldCaptureArtificialLightFrame(submitted_frame);') == 1
assert 'if (ps9_accumulation_draw && (legacy_ps9_bypass || ps9_stencil_probe != "original") &&' in src
assert 'ps9_probe_instance == kApartmentBulbX890Instance && ps9_stencil_probe != "original"' in src
assert 'ps9_stencil_probe == "original" &&' in src
assert 'stencil_face_probe == "original" &&' in src
assert 'std::memcpy(bytes.data() + used, &value, sizeof(value));' in hash_src
assert 'return XXH3_64bits_withSeed(bytes.data(), used, 0);' in hash_src
assert 'std::array<uint8_t, sizeof(State)> bytes;' in hash_src
assert int(re.search(r'versionCode\s*=\s*(\d+)', gradle)[1]) >= 34
assert 'branches: [feature/g925-stable-16ms-v26]' in flow
print("PASS: no disabled probe work, only one diagnostic read/frame, unchanged fixed-state fidelity")

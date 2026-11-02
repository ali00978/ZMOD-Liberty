#!/usr/bin/env python3
"""Preflight for exact first-use pipeline compile admission.

The C++ behavior has additional runtime tests in native_pipeline_compiler_test.cpp.
No graphics or pipeline state changes are authorized by this test.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
header = (root / "glue/rexglue-sdk-main/src/graphics/gta4_native/native_pipeline_compiler.h").read_text()
unit = (root / "glue/rexglue-sdk-main/tests/unit/graphics/native_pipeline_compiler_test.cpp").read_text()
workflow = (root / ".github/workflows/build-android-apk.yml").read_text()
source = (root / "os/android/app/src/main/assets/default_args.txt").read_text()
old = (root / "glue/rexglue-sdk-main/src/graphics/gta4_native/native_frame_scheduling.h").read_text()
assert 'bool demanded = false;' in header
assert 'existing->second->demanded = true;' in header
assert 'job->demanded = demanded;' in header
assert 'job->demanded = true;' in header
assert 'found->second->demanded' in header
assert 'if (!evicted) return false;' in header
assert 'jobs_.erase(queue_.back())' not in header
assert 'never discards demanded work' in unit
assert 'promotes a duplicate speculative request' in unit
assert 'CHECK_FALSE(can_displace_demanded)' in unit
assert 'CHECK(evicted2)' in unit
assert '--gta4_native_max_queued_frames=3' in source
assert 'kNativeTextureEvictionGraceFrames = 240;' in old
assert 'branches: [feature/g925-native-pipeline-stability-v21]' in workflow
assert "github.event_name == 'push'" in workflow
print("PASS: demanded pipeline priority, capacity rejection and stock Mali renderer preserved")

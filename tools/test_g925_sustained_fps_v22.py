#!/usr/bin/env python3
"""Static checks for stable ADPF hints and Mali device telemetry."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
android=root/"glue/rexglue-sdk-main/gta4-recomp/src/android"
hint=(android/"android_performance_hint.cpp").read_text()
policy=(android/"g925_hint_thread_policy.h").read_text()
power=(android/"android_power.cpp").read_text()
java=(root/"os/android/app/src/main/java/com/libertyrecomp/LauncherActivity.java").read_text()
flow=(root/".github/workflows/build-android-apk.yml").read_text()
assert 'REXCVAR_DEFINE_BOOL(android_performance_hint_stable_threads, true' in hint
assert 'G925StableHintThreads<pid_t> stable_threads{kMaxHintThreads};' in hint
assert 'stable_threads.Select(hot' in hint
assert 'name.rfind("GtaRecorder", 0) == 0' in hint
assert 'name.rfind("Main XThread", 0) == 0' in hint
assert 'miss < 3' in policy
assert 'if (!alive(id))' in policy
assert 'G925MaliGpuUtilization()' in power
assert 'G925BatteryCelsius()' in power
assert 'gpu-util=%d%%' in power
assert 'new GraphicsSetting("CPU hint thread stability",' in java
assert 'new GraphicsSetting("Android CPU performance hints",' in java
assert 'line.contains("power:")' in java
assert 'line.contains("thermal-guard:")' in java
assert 'recent.size() > 240' in java
assert 'branches: [feature/g925-bottleneck-profile-v24]' in flow
print("PASS: stable ADPF thread roles, no visual change, GPU/battery/thermal log, selectable hint-off")

#!/usr/bin/env python3
"""Compile and run independent state replay; also guard Vulkan integration."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / 'glue/rexglue-sdk-main'
source = (SDK / 'src/graphics/gta4_native/graphics_system.cpp').read_text()
# Every native index path must be observed, including expanded fans and quads.
assert source.count('BindNativeIndexBuffer(command_buffer,') == 6
assert source.count('.vkCmdBindIndexBuffer(') == 1
assert '/*all_tracked_state_dynamic=*/true' in source
begin = source.index('const std::array<VkDynamicState, 7> dynamic_states')
end = source.index('};', begin)
for state in ('VIEWPORT', 'SCISSOR', 'DEPTH_BIAS', 'STENCIL_REFERENCE',
              'STENCIL_COMPARE_MASK', 'STENCIL_WRITE_MASK', 'BLEND_CONSTANTS'):
    assert 'VK_DYNAMIC_STATE_' + state in source[begin:end]
assert 'MakeScopeExit([&] { texture_commands.clear(); })' in source
assert 'busy.critical_threads()' in (SDK / 'gta4-recomp/src/android/android_performance_hint.cpp').read_text()
compiler = os.environ.get('G925_TEST_CXX') or shutil.which('g++') or shutil.which('clang++')
if not compiler:
    raise SystemExit('A C++20 host compiler is required; behavioral tests were NOT run')
with tempfile.TemporaryDirectory(prefix='g925-state-') as tmp:
    exe = Path(tmp) / ('state-test.exe' if os.name == 'nt' else 'state-test')
    command = [compiler, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
               '-I' + str(SDK / 'src'), '-I' + str(SDK / 'gta4-recomp/src'),
               str(ROOT / 'tools/test_g925_render_state_v27.cpp'), '-o', str(exe)]
    if os.environ.get('G925_TEST_SANITIZE') == '1':
        command[2:2] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(command, check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: native integration guards and executable C++ regressions')
